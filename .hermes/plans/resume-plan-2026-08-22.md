# Entergram Emulator — C++ Implementation Plan (Paused)

> **Estado:** PAUSADO el 2026-08-22. Guardado para reanudir después.
> **Workspace:** C:\Users\francisco.q\Desktop\entergram-emulator
> **GitHub:** https://github.com/Shiban-01/entergram-emulator (rama develop)

## Progreso completado ✅

1. **GitHub configurado al 100%** — repo público, rama develop default, branch protection en main, CI workflow, release script, README, CONTRIBUTING, CODEOWNERS, issue templates
2. **RomReader implementado** — parser ROM2 (header, B-tree directory, file extraction con unit tests ✅)
3. **SnrVm implementado** — interpreter de instrucciones SNR0 de 18 bytes con dispatch de opcodes ✅
4. **VideoPlayer header + implementation** — FFmpeg-based H.264 decoder, NV12→RGBA conversion vía sws_scale
5. **Estructura del proyecto** — CMake con FetchContent para SDL2 + stb, scripts de build, tests

## Bloqueado ⏸️

- **FFmpeg libraries**: vcpkg build timed out (300s). Necesitamos instalar:
  - `vcpkg install ffmpeg[core,avcodec,avformat,swscale,avutil]:x64-windows`
  - Usar binary caching para acelerar
  - O usar un mirror alternativo (https://github.com/BtbN/FFmpeg-Builds/releases — descargar la versión con headers)
- Alternativa: Usar FFmpeg DLLs precompiladas desde https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-lgpl.zip pero necesitamos la versión con `include/` y `lib/`

## Próximos pasos (por hacer al reanudar)

1. **Completar la instalación de FFmpeg**:
   - Opción A: Dejar `vcpkg install ffmpeg` corriendo en background con timeout mayor
   - Opción B: Descargar manualmente headers + import libs
   - Opción C: Usar CMake `FetchContent` para FFmpeg (más lento pero no requiere vcpkg)

2. **Compilar VideoPlayer.cpp** con FFmpeg include paths
3. **Escribir tests para VideoPlayer** (test con intro video de Umineko)
4. **Implementar RomReader::extract_all()** para descomprimir todos los 109,791 archivos
5. **SNR0 file format parser** — decompress y parsear los 18-byte instructions
6. **SDL2 window + OpenGL 3.3 renderer** (sprite shader)
7. **Audio player** — .nxa files (FFmpeg libavformat + libavcodec)
8. **Input manager** — SDL_Event to game keys
9. **Main game loop** — integrar todos los componentes

## Archivos clave creados/modificados
- `CMakeLists.txt` — configuración de build con FetchContent
- `src/core/rom_reader.hpp/.cpp` — parser ROM2 ✅
- `src/core/snr_vm.hpp/.cpp` — VM SNR0 ✅
- `src/video/player.hpp/.cpp` — video player con FFmpeg (pendiente compilar)
- `tests/test_rom_reader.cpp` — 5 tests ✅
- `tests/test_snr_vm.cpp` — 7 tests ✅
- `.github/workflows/build.yml` — CI cross-platform
- `scripts/build_release.sh` — script de release
- `README.md`, `CONTRIBUTING.md`, `.gitignore`, `CODEOWNERS`, bug report template
- `build_msvc.bat` — script local de build/debug

## Configuración del entorno
- Windows 10, MSVC 19.44.35228.0 (VS 2022 Community)
- CMake 3.30.3
- Ninja build system
- vcpkg clonado en C:\vcpkg (bootstrap completado)
- FFmpeg build runtime: C:\ffmpeg\ (solo ejecutables, necesitamos dev build)
