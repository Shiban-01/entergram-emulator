cmake_minimum_required(VERSION 3.20)
project(entergram_emulator
    VERSION 0.1.0
    DESCRIPTION "Cross-platform emulator for Entergram visual novel engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# =============================================================================
# Tests
# =============================================================================

add_executable(test_rom_reader
    tests/test_rom_reader.cpp
    src/core/rom_reader.cpp
)
target_include_directories(test_rom_reader PRIVATE src)
add_test(NAME RomReaderTest COMMAND test_rom_reader)

add_executable(test_snr_vm
    tests/test_snr_vm.cpp
    src/core/snr_vm.cpp
)
target_include_directories(test_snr_vm PRIVATE src)
add_test(NAME SnrVmTest COMMAND test_snr_vm)

add_executable(test_snr0_parser
    tests/test_snr0_parser.cpp
    src/core/snr0_parser.cpp
)
target_include_directories(test_snr0_parser PRIVATE src)
add_test(NAME Snr0ParserTest COMMAND test_snr0_parser)

# =============================================================================
# Main executable (requires FFmpeg + SDL2)
# =============================================================================
# Uncomment when FFmpeg/SDL2 is available:
# find_package(FFmpeg CONFIG REQUIRED COMPONENTS avcodec avformat swscale avutil)
# add_executable(entergram_emulator
#     src/main.cpp
#     src/core/rom_reader.cpp
#     src/core/snr_vm.cpp
#     src/core/snr0_parser.cpp
#     src/video/player.cpp
#     src/renderer/renderer.cpp
#     src/audio/player.cpp
#     src/input/manager.cpp
# )
