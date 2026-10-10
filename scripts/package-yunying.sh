#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
QT_BIN=${QT_BIN:-$(brew --prefix qtbase)/bin}
QT_LIBS=$("$QT_BIN/qmake" -query QT_INSTALL_LIBS)
BUILD_ROOT="$ROOT_DIR/.build-yunying"
APP_DIR="$ROOT_DIR/dist/分流购票app.app"
SDK_PATH=$(xcrun --show-sdk-path)
MIN_OS=$(sw_vers -productVersion | cut -d. -f1,2)

test "$(uname -s)" = Darwin
test -x "$QT_BIN/qmake"
test -x "$QT_BIN/macdeployqt"
mkdir -p "$BUILD_ROOT" "$ROOT_DIR/dist"
BUILD_DIR=$(mktemp -d "$BUILD_ROOT/package.XXXXXX")
cd "$BUILD_DIR"

# Recent CLT installations put the standard C++ headers in the SDK only.
if test -f "$SDK_PATH/usr/include/c++/v1/iostream"; then
    "$QT_BIN/qmake" "$ROOT_DIR/Vendor/Yunying/yunying.pro" CONFIG+=release \
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MIN_OS" \
        "QMAKE_CXXFLAGS+=-isystem $SDK_PATH/usr/include/c++/v1"
else
    "$QT_BIN/qmake" "$ROOT_DIR/Vendor/Yunying/yunying.pro" CONFIG+=release \
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MIN_OS"
fi
make -j "$(sysctl -n hw.activecpu)"
"$QT_BIN/macdeployqt" FenliuYunying.app -always-overwrite "-libpath=$QT_LIBS" > deploy.log 2>&1
cat deploy.log
if grep -q '^ERROR:' deploy.log; then
    printf 'Qt deployment failed; see %s/deploy.log\n' "$BUILD_DIR" >&2
    exit 1
fi
/usr/libexec/PlistBuddy -c "Add LSMinimumSystemVersion string $MIN_OS" FenliuYunying.app/Contents/Info.plist
mkdir -p FenliuYunying.app/Contents/Resources/Notices
cp "$ROOT_DIR/Vendor/Yunying/LICENSE" FenliuYunying.app/Contents/Resources/Notices/Yunying-GPLv3.txt
cp "$ROOT_DIR/Vendor/Yunying/lib/qt-notify/LICENSE" FenliuYunying.app/Contents/Resources/Notices/qt-notify-LICENSE.txt
cp "$ROOT_DIR/Vendor/Yunying/ORIGIN.md" FenliuYunying.app/Contents/Resources/Notices/ORIGIN.md
cp "$ROOT_DIR/Vendor/Yunying/images/ui/LICENSE" FenliuYunying.app/Contents/Resources/Notices/Lucide-LICENSE.txt
codesign --force --deep --sign - FenliuYunying.app
codesign --verify --deep --strict FenliuYunying.app

# Preserve any previous generated bundle, rather than mixing old/new frameworks.
if test -e "$APP_DIR"; then
    mv "$APP_DIR" "$BUILD_DIR/previous.app"
fi
ditto FenliuYunying.app "$APP_DIR"
printf 'Created %s (macOS %s+, %s)\n' "$APP_DIR" "$MIN_OS" "$(uname -m)"
