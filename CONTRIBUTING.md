# Contributing to Entergram Emulator

Thanks for your interest in contributing! This document explains how to set up your development environment.

## Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022+)
- CMake 3.20+
- SDL2 development libraries
- FFmpeg development libraries
- OpenGL development libraries
- Git

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install libsdl2-dev libavcodec-dev libavformat-dev libswscale-dev cmake build-essential
```

**macOS (Homebrew):**
```bash
brew install sdl2 ffmpeg cmake
```

**Windows (vcpkg):**
```bash
vcpkg install sdl2 ffmpeg --triplet x64-windows
```

## Building from Source

```bash
git clone https://github.com/Shiban-01/entergram-emulator.git
cd entergram-emulator
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j4
```

## Code Style

- **C++20** features are encouraged (concepts, coroutines, modules where appropriate)
- **Google C++ Style Guide** guidelines
- Use `clang-format` with the project's `.clang-format` configuration
- Comment non-obvious logic with `//` comments
- Keep functions small (< 100 lines)

## Testing

```bash
cd build
ctest --output-on-failure
```

## Reporting Issues

1. Check existing issues at https://github.com/Shiban-01/entergram-emulator/issues
2. Create a new issue with:
   - Platform (Windows/Linux/macOS, version)
   - ROM file being used (game + version)
   - Error logs or screenshots
   - Steps to reproduce

## License

By contributing, you agree that your contributions will be licensed under the project's license (see [LICENSE](LICENSE)).

## Code of Conduct

Be respectful and constructive. We're all here to build something awesome together.
