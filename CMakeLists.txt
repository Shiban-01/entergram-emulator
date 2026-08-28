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
# Find dependencies
# =============================================================================

# Find FFmpeg (video decoding) - use vcpkg installed targets
set(FFMPEG_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include")
set(FFMPEG_LIB_DIR "C:/vcpkg/installed/x64-windows/lib")

# FFmpeg libraries we need (from vcpkg installed)
find_library(AVCODEC_LIB NAMES avcodec PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
find_library(AVFORMAT_LIB NAMES avformat PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
find_library(AVUTIL_LIB NAMES avutil PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
find_library(SWSCALE_LIB NAMES swscale PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
find_library(SWRESAMPLE_LIB NAMES swresample PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)

set(FFMPEG_FOUND TRUE)
if(NOT AVCODEC_LIB)
    message(FATAL_ERROR "FFmpeg not found (avcodec missing)")
endif()

# Find SDL2
find_package(SDL2 REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Opus is installed via vcpkg; use the include/lib dirs directly
set(OPUS_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include")
set(OPUS_LIBRARY "C:/vcpkg/installed/x64-windows/lib/opus.lib")

# =============================================================================
# Unit tests (no FFmpeg/SDL2 dependency)
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

add_executable(minimal_test
    tests/minimal_test.cpp
    src/core/rom_reader.cpp
)
target_include_directories(minimal_test PRIVATE src)
add_test(NAME MinimalTest COMMAND minimal_test)

# =============================================================================
# NXA decoder test (uses libopus)
# =============================================================================

add_executable(test_nxa
    tests/test_nxa.cpp
    src/audio/nxa_decoder.cpp
)
target_include_directories(test_nxa PRIVATE src ${OPUS_INCLUDE_DIR})
target_link_libraries(test_nxa PRIVATE ${OPUS_LIBRARY})
add_test(NAME NxaDecoderTest COMMAND test_nxa)

# =============================================================================
# Integration test (with FFmpeg video decoding)
# =============================================================================

add_executable(test_integration
    tests/test_integration.cpp
    src/core/rom_reader.cpp
    src/core/snr_vm.cpp
    src/core/snr0_parser.cpp
    src/video/player.cpp
    src/audio/nxa_decoder.cpp
)
target_include_directories(test_integration PRIVATE src)
target_include_directories(test_integration PRIVATE ${FFMPEG_INCLUDE_DIR} ${OPUS_INCLUDE_DIR})
target_link_libraries(test_integration PRIVATE ${AVCODEC_LIB} ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} SDL2::SDL2 SDL2::SDL2main ${OPUS_LIBRARY})
add_test(NAME IntegrationTest COMMAND test_integration)

# =============================================================================
# Main executable (with SDL2 + FFmpeg + OpenGL)
# =============================================================================

add_executable(entergram_emulator
    src/main.cpp
    src/core/rom_reader.cpp
    src/core/snr_vm.cpp
    src/core/snr0_parser.cpp
    src/video/player.cpp
    src/audio/nxa_decoder.cpp
    src/renderer/renderer.cpp
    src/renderer/gl_loader.cpp
    src/renderer/layer_manager.cpp
    src/audio/player.cpp
    src/input/manager.cpp
)

target_include_directories(entergram_emulator PRIVATE src)
target_compile_definitions(entergram_emulator PRIVATE USE_SDL2)

# Conditional HAVE_SWRESAMPLE define
find_library(SWRESAMPLE_LIBRARY NAMES swresample PATHS "C:/vcpkg/installed/x64-windows/lib" NO_DEFAULT_PATH)
if(SWRESAMPLE_LIBRARY)
    set(SWRESAMPLE_FOUND TRUE)
    target_compile_definitions(entergram_emulator PRIVATE HAVE_SWRESAMPLE)
    target_compile_definitions(test_integration PRIVATE HAVE_SWRESAMPLE)
    target_link_libraries(test_integration PRIVATE ${SWRESAMPLE_LIBRARY})
endif()

target_include_directories(entergram_emulator PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${OPUS_INCLUDE_DIR}
)

target_link_libraries(entergram_emulator PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    ${AVCODEC_LIB}
    ${AVFORMAT_LIB}
    ${AVUTIL_LIB}
    ${SWSCALE_LIB}
    ${OPUS_LIBRARY}
    ${OPENGL_LIBRARIES}
)

if(SWRESAMPLE_LIBRARY)
    target_link_libraries(entergram_emulator PRIVATE ${SWRESAMPLE_LIBRARY})
endif()

# Copy FFmpeg + SDL2 DLLs next to the executable
set(DLL_DEPS avcodec-63.dll avformat-63.dll avutil-61.dll swscale-10.dll swresample-7.dll opus.dll)
if(WIN32)
    foreach(DLL ${DLL_DEPS})
        add_custom_command(TARGET entergram_emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/vcpkg/installed/x64-windows/bin/${DLL}"
                "$<TARGET_FILE_DIR:entergram_emulator>"
        )
    endforeach()
endif()
