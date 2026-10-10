#!/bin/sh
# Sourced by the application and offline-check builds.
QT_BIN=${QT_BIN:-$(brew --prefix qtbase)/bin}
MAC_ARCHS=${MAC_ARCHS:-"x86_64 arm64"}
MAC_SDK=${MAC_SDK:-macosx}
BUILD_JOBS=${BUILD_JOBS:-$(sysctl -n hw.activecpu)}
PYTHON=${PYTHON:-python3}
case "$MAC_ARCHS" in
    x86_64|"x86_64 arm64"|"arm64 x86_64") MIN_OS=10.15 ;;
    arm64) MIN_OS=11.0 ;;
    *) printf 'MAC_ARCHS must be x86_64, arm64, or "x86_64 arm64"\n' >&2; exit 1 ;;
esac
test "$(uname -s)" = Darwin
test -x "$QT_BIN/qmake"
test -x "$QT_BIN/macdeployqt"
QT_LIBS=$("$QT_BIN/qmake" -query QT_INSTALL_LIBS)
SDK_PATH=$(xcrun --sdk "$MAC_SDK" --show-sdk-path)
VERIFY_ARCH_ARGS=
for mac_arch in $MAC_ARCHS; do
    VERIFY_ARCH_ARGS="$VERIFY_ARCH_ARGS --arch $mac_arch"
done
if ! "$PYTHON" "$ROOT_DIR/scripts/verify-macos-bundle.py" \
    "$QT_LIBS/QtCore.framework/QtCore" --binary $VERIFY_ARCH_ARGS; then
    printf 'Use the official Qt 6.2.4 universal SDK via QT_BIN; Homebrew Qt is not a legacy-compatible release SDK.\n' >&2
    exit 1
fi
if ! test -d "$SDK_PATH/System/Library/Frameworks/AGL.framework"; then
    printf 'This SDK lacks AGL, which Qt 6.2 requires. Select an older Xcode SDK with MAC_SDK.\n' >&2
    exit 1
fi
if test -n "${MAC_CXX_HEADERS:-}"; then
    test -f "$MAC_CXX_HEADERS/iostream"
    test -f "$MAC_CXX_HEADERS/../LICENSE.TXT"
fi

configure_yunying() {
    project=$1
    # SDK discovery happens before the project's normal command-line assignments.
    set -- "$project" -early "QMAKE_MAC_SDK=$MAC_SDK" "device.sdk=$MAC_SDK" -before CONFIG+=release \
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=$MIN_OS" "QMAKE_APPLE_DEVICE_ARCHS=$MAC_ARCHS"
    if test -n "${MAC_CXX_HEADERS:-}"; then
        set -- "$@" "QMAKE_CXXFLAGS+=-nostdinc++ -isystem $MAC_CXX_HEADERS"
    elif test -f "$SDK_PATH/usr/include/c++/v1/iostream"; then
        set -- "$@" "QMAKE_CXXFLAGS+=-isystem $SDK_PATH/usr/include/c++/v1"
    fi
    "$QT_BIN/qmake" "$@"
}
