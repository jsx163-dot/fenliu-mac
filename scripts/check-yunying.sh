#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
QT_BIN=${QT_BIN:-$(brew --prefix qtbase)/bin}
SDK_PATH=$(xcrun --show-sdk-path)
MIN_OS=$(sw_vers -productVersion | cut -d. -f1,2)
mkdir -p "$ROOT_DIR/.build-yunying/checks"
cd "$ROOT_DIR/.build-yunying/checks"
if test -f "$SDK_PATH/usr/include/c++/v1/iostream"; then
    "$QT_BIN/qmake" "$ROOT_DIR/Vendor/Yunying/macos-checks.pro" CONFIG+=release \
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MIN_OS" \
        "QMAKE_CXXFLAGS+=-isystem $SDK_PATH/usr/include/c++/v1"
else
    "$QT_BIN/qmake" "$ROOT_DIR/Vendor/Yunying/macos-checks.pro" CONFIG+=release \
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MIN_OS"
fi
make -j "$(sysctl -n hw.activecpu)"
./FenliuYunyingChecks.app/Contents/MacOS/FenliuYunyingChecks "$@"
