cmake_minimum_required(VERSION 3.20)
project(entergram_emulator
    VERSION 0.1.0
    DESCRIPTION "Cross-platform emulator for Entergram visual novel engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# =============================================================================
# Tests (no SDL2/OpenGL needed for core module tests)
# =============================================================================
add_executable(test_rom_reader
    tests/test_rom_reader.cpp
    src/core/rom_reader.cpp
)

target_include_directories(test_rom_reader PRIVATE src)

# Add test
add_test(NAME RomReaderTest COMMAND test_rom_reader)

# =============================================================================
# Main executable (disabled until SDL2 is available)
# =============================================================================
# Uncomment when SDL2 is installed:
# find_package(SDL2 REQUIRED)
# add_executable(entergram_emulator
#     src/main.cpp
#     src/core/rom_reader.cpp
#     src/core/snr_vm.cpp
#     src/renderer/renderer.cpp
#     src/audio/player.cpp
#     src/video/player.cpp
#     src/input/manager.cpp
# )
# target_include_directories(entergram_emulator PRIVATE src)
# target_link_libraries(entergram_emulator PRIVATE SDL2::SDL2)
