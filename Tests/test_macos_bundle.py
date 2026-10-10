import importlib.util
import plistlib
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bundle", ROOT / "scripts/verify-macos-bundle.py")
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


def macho(arch="x86_64", minimum=0x0A0F00, legacy=False, dependency=None, rpath=None):
    commands = [struct.pack("<4I", 0x24, 16, minimum, 0)] if legacy else [
        struct.pack("<6I", 0x32, 24, 1, minimum, 0, 0)]
    for text, command, offset in ((dependency, 0xC, 24), (rpath, 0x8000001C, 12)):
        if text:
            payload = text.encode() + b"\0"
            size = (offset + len(payload) + 7) // 8 * 8
            commands.append(struct.pack("<3I", command, size, offset) + b"\0" * (offset - 12)
                            + payload + b"\0" * (size - offset - len(payload)))
    cpu = {"x86_64": 0x1000007, "arm64": 0x100000C}[arch]
    return struct.pack("<8I", 0xFEEDFACF, cpu, 3 if arch == "x86_64" else 0,
                       2, len(commands), sum(map(len, commands)), 0, 0) + b"".join(commands)


def universal():
    intel, arm = macho(legacy=True), macho("arm64", 0x0B0000)
    header = struct.pack(">2I", 0xCAFEBABE, 2)
    header += struct.pack(">5I", 0x1000007, 3, 48, len(intel), 0)
    header += struct.pack(">5I", 0x100000C, 0, 48 + len(intel), len(arm), 0)
    return header + intel + arm


class BundleChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.app = Path(self.temp.name) / "Example.app"
        self.contents = self.app / "Contents"
        (self.contents / "MacOS").mkdir(parents=True)
        self.plist = {"CFBundleExecutable": "Example", "LSMinimumSystemVersion": "10.15",
                      "LSMinimumSystemVersionByArchitecture": {"x86_64": "10.15", "arm64": "11.0"}}
        self.write_plist()
        self.binary = self.contents / "MacOS/Example"
        self.binary.write_bytes(universal())

    def write_plist(self):
        (self.contents / "Info.plist").write_bytes(plistlib.dumps(self.plist))

    def test_universal_and_both_deployment_commands(self):
        self.assertEqual(bundle.check_bundle(self.app, ["x86_64", "arm64"]), 1)
        self.assertEqual(bundle.slices(universal())["x86_64"][0], (10, 15, 0))
        self.assertEqual(bundle.slices(universal())["arm64"][0], (11, 0, 0))

    def test_missing_architecture(self):
        self.binary.write_bytes(macho())
        with self.assertRaisesRegex(ValueError, "missing arm64"):
            bundle.check_bundle(self.app, ["x86_64", "arm64"])

    def test_newer_binary_not_hidden_by_plist(self):
        self.binary.write_bytes(macho(minimum=0x1B0000))
        with self.assertRaisesRegex(ValueError, "requires macOS 27"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_all_plugins_checked(self):
        plugins = self.contents / "PlugIns"
        plugins.mkdir()
        (plugins / "new.dylib").write_bytes(macho(minimum=0x0B0000))
        with self.assertRaisesRegex(ValueError, "new.dylib.*requires macOS 11"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_incorrect_plist(self):
        self.plist["LSMinimumSystemVersionByArchitecture"]["x86_64"] = "27.0"
        self.write_plist()
        with self.assertRaisesRegex(ValueError, "incorrect plist"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_external_dependency_rejected(self):
        self.binary.write_bytes(macho(dependency="/opt/homebrew/lib/example.dylib"))
        with self.assertRaisesRegex(ValueError, "external dependency"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_unresolved_dependency_rejected(self):
        self.binary.write_bytes(macho(dependency="@rpath/missing.dylib", rpath="@executable_path/../Frameworks"))
        with self.assertRaisesRegex(ValueError, "unresolved"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_bundled_dependency_and_symlink(self):
        frameworks = self.contents / "Frameworks"
        frameworks.mkdir()
        (frameworks / "real.dylib").write_bytes(macho(dependency="/usr/lib/libSystem.B.dylib"))
        (frameworks / "example.dylib").symlink_to("real.dylib")
        self.binary.write_bytes(macho(dependency="@rpath/example.dylib", rpath="@executable_path/../Frameworks"))
        self.assertEqual(bundle.check_bundle(self.app, ["x86_64"]), 2)

    def test_malformed_fat_binary(self):
        with self.assertRaises(ValueError):
            bundle.slices(universal()[:50])

    def test_external_symlink_rejected(self):
        (self.contents / "external").symlink_to(self.app.parent, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink points outside"):
            bundle.check_bundle(self.app, ["x86_64"])

    def test_missing_deployment_command(self):
        with self.assertRaisesRegex(ValueError, "missing macOS deployment"):
            bundle.slices(struct.pack("<8I", 0xFEEDFACF, 0x1000007, 3, 2, 0, 0, 0, 0))


if __name__ == "__main__":
    unittest.main()
