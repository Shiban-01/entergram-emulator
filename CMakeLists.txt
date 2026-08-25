cmake_minimum_required(VERSION 3.20)
project(entergram_emulator
    VERSION 0.1.0
    DESCRIPTION "Cross-platform emulator for Entergram visual novel engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# FetchContent for dependencies
include(FetchContent)

# SDL2 (window + input + audio)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.8
)
set(SDLDYNAMICLOAD OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# stb_image (PNG loading)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

add_executable(entergram_emulator
    src/main.cpp
    src/core/rom_reader.cpp
    src/core/snr_vm.cpp
    src/renderer/renderer.cpp
    src/audio/player.cpp
    src/video/player.cpp
    src/input/manager.cpp
)

target_include_directories(entergram_emulator PRIVATE
    ${SDL2_SOURCE_DIR}/include
    src
)

target_link_libraries(entergram_emulator PRIVATE
    SDL2::SDL2
    ${stb_SOURCE_DIR}/stb_image.h
)

# Platform-specific
if(WIN32)
    target_link_libraries(entergram_emulator PRIVATE
        opengl32
    )
elseif(APPLE)
    find_library(OPENGL_LIBRARY OpenGL)
    target_link_libraries(entergram_emulator PRIVATE
        ${OPENGL_LIBRARY}
    )
else()
    target_link_libraries(entergram_emulator PRIVATE
        GL
    )
endif()
