#!/usr/bin/env python3
"""Check actual Mach-O deployment targets, not just the application's plist."""

import argparse
import plistlib
import struct
from pathlib import Path

TARGETS = {"x86_64": (10, 15, 0), "arm64": (11, 0, 0)}
MAGICS = {b"\xcf\xfa\xed\xfe": "<", b"\xfe\xed\xfa\xcf": ">"}
FAT_MAGICS = {
    b"\xca\xfe\xba\xbe": (">", False),
    b"\xbe\xba\xfe\xca": ("<", False),
    b"\xca\xfe\xba\xbf": (">", True),
    b"\xbf\xba\xfe\xca": ("<", True),
}


def version(number):
    return number >> 16, (number >> 8) & 255, number & 255


def version_text(value):
    return ".".join(map(str, value))


def slices(data):
    if data[:4] in FAT_MAGICS:
        endian, wide = FAT_MAGICS[data[:4]]
        count = struct.unpack_from(endian + "I", data, 4)[0]
        entry_size = 32 if wide else 20
        if count > 64 or 8 + count * entry_size > len(data):
            raise ValueError("invalid fat header")
        result = {}
        for index in range(count):
            entry = 8 + index * entry_size
            offset, size = struct.unpack_from(endian + ("QQ" if wide else "II"), data, entry + 8)
            if offset < 8 + count * entry_size or offset + size > len(data):
                raise ValueError("invalid fat slice")
            for arch, info in slices(data[offset:offset + size]).items():
                if arch in result:
                    raise ValueError("duplicate architecture")
                result[arch] = info
        return result
    endian = MAGICS.get(data[:4])
    if not endian or len(data) < 32:
        raise ValueError("not a 64-bit Mach-O binary")
    cpu, subtype, _, count, command_size = struct.unpack_from(endian + "5I", data, 4)
    arch = {0x1000007: "x86_64", 0x100000C: "arm64"}.get(cpu, hex(cpu))
    subtype &= 0xFFFFFF
    if cpu == 0x1000007 and subtype == 8:
        arch = "x86_64h"
    if cpu == 0x100000C and subtype == 2:
        arch = "arm64e"
    end = 32 + command_size
    if end > len(data) or count > command_size // 8:
        raise ValueError("invalid load commands")
    cursor, minimum, dependencies, rpaths = 32, None, [], []
    for _ in range(count):
        command, size = struct.unpack_from(endian + "II", data, cursor)
        if size < 8 or cursor + size > end:
            raise ValueError("invalid load command")
        if command == 0x24 and size >= 16:  # LC_VERSION_MIN_MACOSX
            minimum = version(struct.unpack_from(endian + "I", data, cursor + 8)[0])
        elif command == 0x32 and size >= 24:  # LC_BUILD_VERSION
            platform, target = struct.unpack_from(endian + "II", data, cursor + 8)
            if platform != 1:
                raise ValueError("non-macOS platform")
            minimum = version(target)
        elif command in (0xC, 0x80000018, 0x8000001F, 0x80000023, 0x8000001C):
            offset = struct.unpack_from(endian + "I", data, cursor + 8)[0]
            if offset < 12 or offset >= size:
                raise ValueError("invalid load path")
            value = data[cursor + offset:cursor + size].split(b"\0", 1)[0].decode("utf-8")
            (rpaths if command == 0x8000001C else dependencies).append(value)
        cursor += size
    if minimum is None:
        raise ValueError("missing macOS deployment target")
    return {arch: (minimum, dependencies, rpaths)}


def check_binary(path, arches):
    info = slices(path.read_bytes())
    for arch in arches:
        if arch not in info:
            raise ValueError("missing " + arch)
        if info[arch][0] > TARGETS[arch]:
            raise ValueError("%s requires macOS %s (target %s)" % (
                arch, version_text(info[arch][0]), version_text(TARGETS[arch])))
    return info


def check_bundle(app, arches):
    contents = (app / "Contents").resolve()
    with (contents / "Info.plist").open("rb") as stream:
        plist = plistlib.load(stream)
    for arch in arches:
        declared = plist.get("LSMinimumSystemVersionByArchitecture", {}).get(
            arch, plist.get("LSMinimumSystemVersion", ""))
        if declared != version_text(TARGETS[arch]).removesuffix(".0"):
            raise ValueError("incorrect plist minimum for " + arch)
    executable = contents / "MacOS" / plist["CFBundleExecutable"]
    if not executable.resolve().is_relative_to(contents):
        raise ValueError("executable points outside bundle")
    binaries = {executable.resolve(): check_binary(executable, arches)}
    for path in contents.rglob("*"):
        if path.is_symlink() and not path.resolve().is_relative_to(contents):
            raise ValueError("symlink points outside bundle: " + str(path.relative_to(contents)))
        if not path.is_file() or path.resolve() in binaries:
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if magic in MAGICS or magic in FAT_MAGICS:
            try:
                binaries[path.resolve()] = check_binary(path, arches)
            except (ValueError, struct.error) as error:
                raise ValueError("%s: %s" % (path.relative_to(contents), error)) from error

    def expand(value, loader):
        return Path(value.replace("@loader_path", str(loader.parent)).replace(
            "@executable_path", str(executable.parent)))

    for path, info in binaries.items():
        for arch in arches:
            _, dependencies, rpaths = info[arch]
            search = [expand(r, path) for r in rpaths]
            search += [expand(r, executable) for r in binaries[executable.resolve()][arch][2]]
            for dependency in dependencies:
                if dependency.startswith(("/usr/lib/", "/System/Library/")):
                    continue
                if dependency.startswith("@rpath/"):
                    candidates = [r / dependency[7:] for r in search]
                elif dependency.startswith(("@loader_path/", "@executable_path/")):
                    candidates = [expand(dependency, path)]
                else:
                    raise ValueError("external dependency: " + dependency)
                if not any(p.resolve() in binaries and p.resolve().is_relative_to(contents) for p in candidates):
                    raise ValueError("%s: unresolved bundled dependency %s" % (path.name, dependency))
    return len(binaries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--binary", action="store_true")
    parser.add_argument("--arch", action="append", choices=TARGETS)
    args = parser.parse_args()
    arches = args.arch or list(TARGETS)
    try:
        count = 1 if args.binary else check_bundle(args.path, arches)
        if args.binary:
            check_binary(args.path, arches)
    except (ValueError, OSError, KeyError, struct.error) as error:
        parser.exit(1, "Compatibility check failed: %s\n" % error)
    print("Verified %d Mach-O files: %s" % (count, ", ".join(
        "%s macOS %s+" % (a, version_text(TARGETS[a])) for a in arches)))


if __name__ == "__main__":
    main()
