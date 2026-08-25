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
# Test: RomReader
add_executable(test_rom_reader
    tests/test_rom_reader.cpp
    src/core/rom_reader.cpp
)
target_include_directories(test_rom_reader PRIVATE src)
add_test(NAME RomReaderTest COMMAND test_rom_reader)

# Test: SnrVm
add_executable(test_snr_vm
    tests/test_snr_vm.cpp
    src/core/snr_vm.cpp
)
target_include_directories(test_snr_vm PRIVATE src)
add_test(NAME SnrVmTest COMMAND test_snr_vm)
