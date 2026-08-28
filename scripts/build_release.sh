#!/bin/bash
# Build the Entergram Emulator in Release mode
# Usage: ./scripts/build_release.sh

set -e

BUILD_DIR="build-release"
VCPKG_TOOLCHAIN="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

echo "=== Building Entergram Emulator (Release) ==="

# Configure
cmake -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_TOOLCHAIN \
    -DUSE_SDL2=ON

# Build
cmake --build $BUILD_DIR --config Release -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "=== Build complete ==="
echo "Executable: $BUILD_DIR/entergram_emulator.exe"
echo ""

# Copy DLLs
echo "Copying DLLs..."
for dll in avcodec-63.dll avformat-63.dll avutil-61.dll swscale-10.dll opus.dll; do
    if [ -f "C:/vcpkg/installed/x64-windows/bin/$dll" ]; then
        cp -f "C:/vcpkg/installed/x64-windows/bin/$dll" $BUILD_DIR/
    fi
done

echo "Done!"
