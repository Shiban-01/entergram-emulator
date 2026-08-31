# Entergram Engine Emulator

A cross-platform C++20 emulator for the Entergram visual novel engine, currently focused on **Umineko no Naku Koro ni** (Switch/Entergram edition). Uses SDL2 + OpenGL 3.3 + FFmpeg + libopus.

## Features

- **ROM2 Parser** — Reads the proprietary ROM2 archive format used by Entergram engine, extracting and navigating 109,791+ files from a 10.3 GB data.rom
- **SNR VM** — Executes the 18-byte instruction bytecode from `main.snr`, with callbacks for voice, BGM, SE, movie, sprites, and text display
- **Video Player** — FFmpeg-based H.264/AVC decoder producing RGBA frames at 29.97fps (1920×1088), with real-time frame timing
- **NXA Audio Decoder** — Custom parser for Entergram's NXA1 container format, using libopus to decode raw Opus frames into 16-bit PCM
- **SDL2 Audio** — PCM streaming playback with volume control via SDL_Audio callbacks
- **OpenGL 3.3 Renderer** — Core profile with custom shaders, VAO/VBO, texture management, and layer-based sprite rendering
- **Layer Manager** — Sprite positioning, alpha blending, transitions, and batch rendering

## Building

### Prerequisites
- **MSVC 2022** (v14.44+)
- **CMake** 3.20+
- **vcpkg** — install dependencies:
  ```bat
  vcpkg install ffmpeg[x86,x64]:x64-windows sdl2:x64-windows opus:x64-windows
  ```
- **Windows SDK** (10.0.26100.0+)

### Build
```bat
build_msvc.bat
```
This runs: vcvars64 → CMake configure with vcpkg toolchain → Ninja build.

### Manual Build
```bat
cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Usage

```bat
build\entergram_emulator.exe C:\path\to\data.rom
```

The emulator will:
1. Parse the ROM2 archive structure
2. Extract and play the intro video (`movie/sakucs_op.mp4`)
3. Load `main.snr` and begin executing VM instructions
4. Show the SELECT menu after the intro finishes

### Controls
| Key | Action |
|-----|--------|
| `ESC` | Quit |
| `SPACE` | Skip intro video / advance text |

## Testing

```bat
# Run all tests
build\test_integration.exe
build\test_nxa.exe
build\tests.exe
```

## Data Format Notes

### ROM2 Format
- Magic: `ROM2` at offset 0
- Version: `0x00010001` (low 16 bits = 1)
- Index offset: `0x20`
- Offset multiplier: 512
- B-tree directory structure

### NXA Format
- Magic: `NXA1`
- Audio data at offset 0x30 (after header)
- Uses raw Opus frames (180 bytes per frame, 960 samples per frame)
- Sample rate: 48000 Hz
- Pre-skip applied before playback

### SNR Format
- 18-byte instructions: flag(1) + opcode(1) + arg1(4) + arg2(4) + arg3(4) + arg4(4)
- Opcodes: SYS(0x00), VOICEPLAY(0x9c), BGMPLAY(0xa0), MOVIE(0xb0), LAYER(0xc1), etc.

## Release

- **v0.1.0** — Initial release with full game loop integration

## License

This project is for research and educational purposes. It does not include any copyrighted game assets.
