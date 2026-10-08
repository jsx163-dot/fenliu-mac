#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
APP_DIR="$ROOT_DIR/dist/分流.app"
CONTENTS_DIR="$APP_DIR/Contents"

cd "$ROOT_DIR"
swift build --build-system native -c release
mkdir -p "$CONTENTS_DIR/MacOS"
cp .build/release/FenliuMac "$CONTENTS_DIR/MacOS/FenliuMac"
cp Info.plist "$CONTENTS_DIR/Info.plist"
codesign --force --deep --sign - "$APP_DIR"
printf 'Created %s\n' "$APP_DIR"
