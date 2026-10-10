#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$ROOT_DIR/scripts/macos-build-env.sh"
BUILD_ROOT="$ROOT_DIR/.build-yunying"
APP_DIR=${APP_DIR:-"$ROOT_DIR/dist/分流购票app.app"}

mkdir -p "$BUILD_ROOT" "$ROOT_DIR/dist"
BUILD_DIR=$(mktemp -d "$BUILD_ROOT/package.XXXXXX")
cd "$BUILD_DIR"

configure_yunying "$ROOT_DIR/Vendor/Yunying/yunying.pro"
make -j "$BUILD_JOBS"
MACDEPLOY_ARCH=${MAC_ARCHS%% *} PATH="$ROOT_DIR/scripts/compat-tools:$PATH" \
    "$QT_BIN/macdeployqt" FenliuYunying.app -always-overwrite "-libpath=$QT_LIBS" > deploy.log 2>&1
cat deploy.log
if grep -q '^ERROR:' deploy.log; then
    printf 'Qt deployment failed; see %s/deploy.log\n' "$BUILD_DIR" >&2
    exit 1
fi
/usr/libexec/PlistBuddy -c "Add LSMinimumSystemVersion string $MIN_OS" FenliuYunying.app/Contents/Info.plist
/usr/libexec/PlistBuddy -c 'Add LSMinimumSystemVersionByArchitecture dict' FenliuYunying.app/Contents/Info.plist
for mac_arch in $MAC_ARCHS; do
    case "$mac_arch" in x86_64) minimum=10.15 ;; arm64) minimum=11.0 ;; esac
    /usr/libexec/PlistBuddy -c "Add LSMinimumSystemVersionByArchitecture:$mac_arch string $minimum" FenliuYunying.app/Contents/Info.plist
done
mkdir -p FenliuYunying.app/Contents/Resources/Notices
cp "$ROOT_DIR/Vendor/Yunying/LICENSE" FenliuYunying.app/Contents/Resources/Notices/Yunying-GPLv3.txt
cp "$ROOT_DIR/Vendor/Yunying/lib/qt-notify/LICENSE" FenliuYunying.app/Contents/Resources/Notices/qt-notify-LICENSE.txt
cp "$ROOT_DIR/Vendor/Yunying/ORIGIN.md" FenliuYunying.app/Contents/Resources/Notices/ORIGIN.md
cp "$ROOT_DIR/Vendor/Yunying/images/ui/LICENSE" FenliuYunying.app/Contents/Resources/Notices/Lucide-LICENSE.txt
if test "$("$QT_BIN/qmake" -query QT_VERSION)" = 6.2.4; then
    cp "$ROOT_DIR/Vendor/Yunying/macos/Qt-6.2.4-NOTICE.txt" FenliuYunying.app/Contents/Resources/Notices/
    cp "$ROOT_DIR/Vendor/Yunying/LICENSE" FenliuYunying.app/Contents/Resources/Notices/Qt-GPLv3.txt
fi
if test -n "${MAC_CXX_HEADERS:-}"; then
    cp "$MAC_CXX_HEADERS/../LICENSE.TXT" FenliuYunying.app/Contents/Resources/Notices/libcxx-LICENSE.txt
fi
"$PYTHON" "$ROOT_DIR/scripts/verify-macos-bundle.py" FenliuYunying.app $VERIFY_ARCH_ARGS
codesign --force --deep --sign - FenliuYunying.app
codesign --verify --deep --strict FenliuYunying.app

# Preserve any previous generated bundle, rather than mixing old/new frameworks.
if test -e "$APP_DIR"; then
    mv "$APP_DIR" "$BUILD_DIR/previous.app"
fi
ditto FenliuYunying.app "$APP_DIR"
printf 'Created %s (%s; Intel macOS 10.15+, Apple Silicon macOS 11+)\n' "$APP_DIR" "$MAC_ARCHS"
