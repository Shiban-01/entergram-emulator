#!/bin/bash
# Build script for Entergram Emulator releases
# Usage: ./scripts/build_release.sh [version]

set -e

VERSION=${1:-"0.1.0-beta"}
BUILD_DIR="build-release"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Building Entergram Emulator v${VERSION}"
echo "Project root: ${PROJECT_ROOT}"

cd "${PROJECT_ROOT}"
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Compiling..."
NUM_CPUS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build . --config Release -j${NUM_CPUS}

# Package for distribution
echo "Packaging release..."

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    echo "Packaging for Windows..."
    # Windows packaging (zip)
    powershell -Command "Compress-Archive -Path entergram_emulator.exe -DestinationPath entergram-emulator-${VERSION}-win64.zip -Force"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Packaging for macOS..."
    tar czf entergram-emulator-${VERSION}-macos.tar.gz entergram_emulator
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Packaging for Linux..."
    tar czf entergram-emulator-${VERSION}-linux.tar.gz entergram_emulator
else
    echo "Unknown OS: $OSTYPE"
    exit 1
fi

echo "Build complete. Release artifacts in ${BUILD_DIR}/"
