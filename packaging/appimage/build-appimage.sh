#!/usr/bin/env bash
# ==============================================================================
# FastTransfer — Linux Universal AppImage Generator
# ==============================================================================

set -e

BUILD_DIR="build-linux"
APPDIR="build-appimage/AppDir"

echo "Building FastTransfer for AppImage packaging..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" --parallel

rm -rf build-appimage
mkdir -p "$APPDIR"

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

# Download linuxdeployqt if not present
if ! command -v linuxdeployqt &> /dev/null; then
    if [ ! -f linuxdeployqt ]; then
        echo "Downloading linuxdeployqt..."
        wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" -O linuxdeployqt
        chmod +x linuxdeployqt
    fi
    LINUXDEPLOYQT="./linuxdeployqt"
else
    LINUXDEPLOYQT="linuxdeployqt"
fi

echo "Creating AppImage..."
$LINUXDEPLOYQT "$APPDIR/usr/share/applications/fasttransfer.desktop" -appimage -unsupported-allow-new-glibc -qmake=/usr/bin/qmake6

echo "AppImage created successfully!"
