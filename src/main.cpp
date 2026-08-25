/**
 * Entergram Emulator - Main Entry Point
 *
 * Cross-platform emulator for the Entergram visual novel engine,
 * supporting Umineko When They Cry and Higurashi When They Cry.
 *
 * Written in C++20 with SDL2 for windowing/input/audio and
 * OpenGL for rendering. Video decoding uses FFmpeg libraries.
 */

#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Entergram Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create renderer (will be replaced with OpenGL in later tasks)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "Entergram Emulator initialized successfully!" << std::endl;

    // Main loop
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                quit = true;
            }
        }

        // Clear screen (black)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render frame (placeholder - will show game content later)
        SDL_RenderPresent(renderer);

        SDL_Delay(16);  // ~60 FPS
    }

    std::cout << "Shutting down..." << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
