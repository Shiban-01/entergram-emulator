#!/bin/bash
# Build script for Entergram Emulator releases
# Usage: ./scripts/build_release.sh [version]

set -e

VERSION=${1:-"0.1.0-beta"}
BUILD_DIR="build-release"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Detect platform
case "$(uname -s)" in
    Darwin*)    PLATFORM="macos" ;;
    Linux*)     PLATFORM="linux" ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM="windows" ;;
    *)          PLATFORM="unknown" ;;
esac

echo "=== Entergram Emulator Release Build v$VERSION ==="
echo "Platform: $PLATFORM"
echo "Project root: $PROJECT_ROOT"

# Clean previous build
rm -rf "$BUILD_DIR"

# Configure
echo ""
echo "--- Configuring (CMake) ---"
if [ "$PLATFORM" = "windows" ]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -G "Ninja"
else
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

# Build
echo ""
echo "--- Building ---"
cmake --build "$BUILD_DIR" --config Release --parallel

# Package
echo ""
echo "--- Packaging ---"
OUTPUT_DIR="$PROJECT_ROOT/release-$PLATFORM-$VERSION"
mkdir -p "$OUTPUT_DIR"

# Copy binary
if [ "$PLATFORM" = "windows" ]; then
    cp "$BUILD_DIR/entergram_emulator.exe" "$OUTPUT_DIR/"
    # Copy FFmpeg DLLs if they exist
    if [ -d "/c/vcpkg/installed/x64-windows/bin" ]; then
        cp /c/vcpkg/installed/x64-windows/bin/av*.dll "$OUTPUT_DIR/" 2>/dev/null || true
        cp /c/vcpkg/installed/x64-windows/bin/sw*.dll "$OUTPUT_DIR/" 2>/dev/null || true
    fi
else
    cp "$BUILD_DIR/entergram_emulator" "$OUTPUT_DIR/"
fi

# Create zip
echo ""
echo "--- Creating archive ---"
cd "$OUTPUT_DIR"
zip -r "../entergram-emulator-$VERSION-$PLATFORM.zip" . || true
cd "$PROJECT_ROOT"

echo ""
echo "=== Build complete! ==="
echo "Release: $OUTPUT_DIR"
echo "Archive: $PROJECT_ROOT/entergram-emulator-$VERSION-$PLATFORM.zip"
