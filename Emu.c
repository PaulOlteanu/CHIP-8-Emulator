#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include "Chip8.c"

int8_t decodeKey(SDL_Keycode);

int main(int argc, char *argv[]) {
    const unsigned int SCALE = 5;
    const unsigned int CYCLES_PER_SECOND = 720;
    const bool MODERN_COMPAT = true;

    srand(time(NULL));

    if(argc != 2) {
        printf("Incorrect usage. Expected a filename for the rom\n");
        return -1;
    }

    chip8 emulator;
    char *filename = argv[1];
    if(initialize(&emulator, filename, MODERN_COMPAT) != 0) {
        return -1;
    }

    // SET UP SDL ==============================================================
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Could not start SDL\n");
        printf("Could not initialize emulator :(\n");
        SDL_Quit();
        return -1;
    }

    SDL_Window *window;
    window = SDL_CreateWindow(
        "CHIP-8",                // Title
        SDL_WINDOWPOS_UNDEFINED, // Initial x position
        SDL_WINDOWPOS_UNDEFINED, // Initial y position
        64 * SCALE,              // Width
        32 * SCALE,              // Height
        SDL_WINDOW_OPENGL        // Flags
    );
    if (window == NULL) {
        printf("SDL Error: %s\n", SDL_GetError());
        printf("Could not initialize emulator :(\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("SDL Error: %s\n", SDL_GetError());
        printf("Could not initialize emulator :(\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("Successfully initialized\n");

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // For limiting FPS
    uint32_t frameStartTick;
    uint32_t frameEndTick;
    uint32_t frameChange;

    // Rectangle for drawing pixels? Yes, because it makes my code for scaling up the display O(1)
    SDL_Rect pixel;
    int renderColour;

    // The type of key event that happened, and the key that was pressed
    SDL_Event event;
    int8_t changedKey;

    bool quit = false;

    while(!quit) {
        // This is for later calculating how long this frame took
        frameStartTick = SDL_GetTicks();

        // INPUT HANDLING
        if (emulator.keyWait) {
            while (!SDL_PollEvent(&event) || decodeKey(event.key.keysym.sym) == -1 || event.type != SDL_KEYDOWN) {}
            changedKey = decodeKey(event.key.keysym.sym);
            emulator.V[emulator.keySpot] = changedKey;
            emulator.keyWait = false;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP){
                changedKey = decodeKey(event.key.keysym.sym);
                if (changedKey != -1) {
                    // If the event is a keydown, set the key to true in the array, otherwise, false
                    emulator.keys[changedKey] = (event.type == SDL_KEYDOWN);
                }
            }
        }

        if (emulateCycle(&emulator) == -1) {
            printf("Error: Program out of bounds\n");
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        if (emulator.awaitingRedraw) {
            for (int i = 0; i < 64 * 32; i++) {
                // Render colour is either 255 if screen[i] == 1 or 0 if screen[i] == 0
                // The I_MAX ensures that pixels are never 100% black
                renderColour = I_MAX(255 * emulator.screen[i], 35);
                SDL_SetRenderDrawColor(renderer, renderColour, renderColour, renderColour, renderColour);

                pixel.x = (i % 64) * SCALE;
                pixel.y = (i / 64) * SCALE;
                pixel.w = SCALE;
                pixel.h = SCALE;

                SDL_RenderFillRect(renderer, &pixel);
            }

            SDL_RenderPresent(renderer);
            emulator.awaitingRedraw = false;
        }

        // Limit the FPS
        // Every frame should take (1000 ms) / CYCLES_PER_SECOND ticks to complete
        // If they take less, sleep for the number of ticks left over
        frameEndTick = SDL_GetTicks();
        frameChange = frameEndTick - frameStartTick;
        if(frameChange < 1000 / CYCLES_PER_SECOND) {
            SDL_Delay((1000 / CYCLES_PER_SECOND) - frameChange);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// This is disgusting, but probably the simplest (and best) way to do it
int8_t decodeKey(SDL_Keycode k) {
    switch (k) {
    case SDLK_1:
        return 0x1;
    case SDLK_2:
        return 0x2;
    case SDLK_3:
        return 0x3;
    case SDLK_4:
        return 0xC;
    case SDLK_q:
        return 0x4;
    case SDLK_w:
        return 0x5;
    case SDLK_e:
        return 0x6;
    case SDLK_r:
        return 0xD;
    case SDLK_a:
        return 0x7;
    case SDLK_s:
        return 0x8;
    case SDLK_d:
        return 0x9;
    case SDLK_f:
        return 0xE;
    case SDLK_z:
        return 0xA;
    case SDLK_x:
        return 0x0;
    case SDLK_c:
        return 0xB;
    case SDLK_v:
        return 0xF;
    default:
        return -1;
    }
}
