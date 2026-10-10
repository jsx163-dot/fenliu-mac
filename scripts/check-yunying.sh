#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$ROOT_DIR/scripts/macos-build-env.sh"
mkdir -p "$ROOT_DIR/.build-yunying"
CHECK_BUILD_DIR=$(mktemp -d "$ROOT_DIR/.build-yunying/checks-compat.XXXXXX")
cd "$CHECK_BUILD_DIR"
configure_yunying "$ROOT_DIR/Vendor/Yunying/macos-checks.pro"
make -j "$BUILD_JOBS"
/usr/bin/arch -"${TEST_ARCH:-$(uname -m)}" ./FenliuYunyingChecks.app/Contents/MacOS/FenliuYunyingChecks "$@"
