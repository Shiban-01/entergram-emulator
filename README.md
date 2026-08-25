# Entergram Emulator

A cross-platform emulator for the Entergram visual novel engine, supporting **Umineko When They Cry** and **Higurashi When They Cry** (future support).

Written in C++20 with SDL2 for window management, OpenGL for rendering, and FFmpeg for video decoding.

## ⚠️ Disclaimer

This project is for educational purposes only. It requires you to provide your own ROM files from games you legally own. The emulator does not include any copyrighted game data.

## Supported Games

- Umineko When They Cry (Switch version)
- Higurashi When They Cry (future support)

## Building

### Prerequisites

- **CMake** 3.20 or later
- **C++20** compiler (GCC 11+, Clang 14+, MSVC 2022+)
- **SDL2** development libraries
- **FFmpeg** development libraries (libavcodec, libavformat, libswscale)
- **OpenGL** development libraries

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/Shiban-01/entergram-emulator.git
cd entergram-emulator

# Create a build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
```

### Running

```bash
./build/entergram_emulator path/to/data.rom
```

## Downloads

Pre-built binaries are available in [Releases](https://github.com/Shiban-01/entergram-emulator/releases).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, coding standards, and how to contribute.

## License

See [LICENSE](LICENSE) file for details. This project is not affiliated with Entergram or its subsidiaries.

## Credits

- Original engine: Entergram
- ROM format analysis: community reverse engineering
- This emulator: built with ❤️ using SDL2, FFmpeg, and OpenGL
