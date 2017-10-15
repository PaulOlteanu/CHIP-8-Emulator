#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL.h>

// TODO: Add explanation for this
unsigned char font[16 * 5] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

unsigned short opcode;
unsigned char memory[4096];
unsigned char V[16]; // Registers. V[0xF] is the carry flag
unsigned short I;
unsigned short pc;

unsigned char screen[64 * 32];

// Currently useless
unsigned char delayTimer;
unsigned char soundTimer;

unsigned short stack[24];
unsigned short sp;

unsigned char keys[16];

bool quit = false;
bool redraw = false;
bool clearScreen = false;
bool keyWait = false;
unsigned char keySpot;

int initialize(char *filename);
void emulateCycle();
int decodeKey(SDL_Keycode);

int main(int argc, char *argv[]) {
    char *filename = argv[1];
    if(initialize(filename) != 0) {
        printf("Could not initialize emulator :(\n");
        return -1;
    }

    printf("Successfully initialized\n");

    SDL_Window *window;

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        "CHIP-8",                // Title
        SDL_WINDOWPOS_UNDEFINED, // Initial x position
        SDL_WINDOWPOS_UNDEFINED, // Initial y position
        64,                      // Width
        32,                      // Height
        SDL_WINDOW_OPENGL        // Flags
    );

    if (window == NULL) {
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == NULL) {
        printf("Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    unsigned int previous_time, current_time;
    previous_time = SDL_GetTicks();

    SDL_Event event;

    // TODO: Limit speed of emulation
    while(!quit) {
        if (pc >= 2048) {
            break;
        }

        current_time = SDL_GetTicks();

        if (current_time - previous_time >= 2) {
            previous_time = current_time;

            // INPUT
            if (keyWait) {
                SDL_Event temp_event;

                while (!SDL_PollEvent(&event) || decodeKey(event.key.keysym.sym) == -1) {}
                int k = decodeKey(event.key.keysym.sym);
                V[keySpot] = k;
                keyWait = false;
            }
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                }

                if (event.type == SDL_KEYDOWN) {
                    int k = decodeKey(event.key.keysym.sym);

                    if (k != -1) {
                        keys[k] = true;
                    }
                }

                if (event.type == SDL_KEYUP) {
                    int k = decodeKey(event.key.keysym.sym);

                    if (k != -1) {
                        keys[k] = false;
                    }
                }
            }

            emulateCycle();

            if (clearScreen) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
                SDL_RenderClear(renderer);
            }

            // If the draw flag is set, update the screen
            if (redraw) {
                for (int i = 0; i < 64 * 32; i++) {
                    if (screen[i]) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
                    }
                    SDL_RenderDrawPoint(renderer, i % 64, i / 64);
                }

                SDL_RenderPresent(renderer);

                redraw = false;
            }
        }
    }

    return 0;
}

int initialize(char *filename) {
    pc = 0x200; // The program starts at location 512 aka 0x200, anything before that is reserved
    opcode = 0;
    I = 0;
    sp = 0;
    delayTimer = 0;
    soundTimer = 0;
    quit = false;
    redraw = false;
    clearScreen = false;
    keyWait = false;

    srand(time(NULL));

    // Clear things
    for(int i = 0; i < 64 * 32; i++) {
        screen[i] = 0;
    }
    for(int i = 0; i < 16; i++) {
        V[i] = 0;
        stack[i] = 0;
    }
    for(int i = 0; i < 4096; i++) {
        memory[i]  = 0;
    }

    // Load the font from the font array into reserved memory
    for(int i = 0; i < 16 * 5; i++) {
        memory[i] = font[i];
    }

    // Read the program from the file into memory
    FILE *file = fopen(filename, "rb");

    if(!file) {
        return -1;
    }

    unsigned char *buf;

    int counter = 512;
    while(!feof(file)) {
        fread(&memory[counter], 1, 1, file);
        counter++;
    }

    fclose(file);

    return 0;
}

void emulateCycle() {
    // CHIP-8 is big-endian so memory[pc] is the "left" half of the opcode
    opcode = (memory[pc] << 8) | memory[pc + 1];

    printf("OPCODE: 0x%04x\n", opcode);

    // Switch on the leftmost nibble of the opcode
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF) {
                case 0x00E0:
                    clearScreen = true;
                    pc += 2;
                    break;

                case 0x00EE:
                    sp--;
                    pc = stack[sp];
                    pc += 2;
                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
                    pc += 2;
                    break;
            }

            break;

        case 0x1000:
            pc = (opcode & 0x0FFF);
            break;

        case 0x2000:
            stack[sp] = pc;
            sp++;
            pc = (opcode & 0x0FFF);
            break;

        case 0x3000:
            if(V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x4000:
            if(V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x5000:
            if(V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x6000:
            V[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF);
            pc += 2;
            break;

        case 0x7000:
            V[(opcode & 0x0F00) >> 8] += (opcode & 0x00FF);
            pc += 2;
            break;

        case 0x8000:
            switch(opcode & 0x000F) {
                case 0x0000:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0001:
                    V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0002:
                    V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0003:
                    V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
                    break;

                // TODO: Add explanation for this
                // Replace with casts to int then answer comparison
                case 0x0004:
                    if(V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8])) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];

                    break;

                case 0x0005:
                    if(V[(opcode & 0x0F00) >> 8] < V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 0;
                    } else {
                        V[0xF] = 1;
                    }

                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];

                    break;

                case 0x0006:
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] & 1);
                    V[(opcode & 0x0F00) >> 8] = (V[(opcode & 0x0F00) >> 8]) >> 1;
                    /* V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4]; */

                    break;

                case 0x0007:
                    if(V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 0;
                    } else {
                        V[0xF] = 1;
                    }
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];

                    break;

                case 0x000E:
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] >> 7) & 1;
                    V[(opcode & 0x0F00) >> 8] = (V[(opcode & 0x0F00) >> 8]) << 1;
                    /* V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4]; */

                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
                    pc += 2;
                    break;
            }

            pc += 2;

            break;

        case 0x9000:
            if(V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
                pc += 2;
            }

            pc += 2;
            break;

        case 0xA000:
            I = (opcode & 0x0FFF);
            pc += 2;
            break;

        case 0xB000:
            pc = V[0] + (opcode & 0x0FFF);
            break;

        case 0xC000:
            ;
            int n = (rand() % (0xFF + 1));
            V[(opcode & 0x0F00) >> 8] = (n & (opcode & 0x00FF));

            pc += 2;
            break;

        // TODO: Make this not copy-pasted shit
        case 0xD000:
            ;
            unsigned short x = V[(opcode & 0x0F00) >> 8];
            unsigned short y = V[(opcode & 0x00F0) >> 4];
            unsigned short height = opcode & 0x000F;
            unsigned short pixel;

            V[0xF] = 0;
            for (int ypos = 0; ypos < height; ypos++) {
                pixel = memory[I + ypos];

                for(int xpos = 0; xpos < 8; xpos++) {
                    if((pixel & (0x80 >> xpos)) != 0) {
                        if(screen[(x + xpos + ((y + ypos) * 64))] == 1) {
                            V[0xF] = 1;
                        }
                        screen[x + xpos + ((y + ypos) * 64)] ^= 1;
                    }
                }
            }

            redraw = true;
            pc += 2;
            break;

        case 0xE000:
            switch(opcode & 0x00FF) {
                case 0x009E:
                    if(keys[V[(opcode & 0x0F00) >> 8]] != 0) {
                        pc += 2;
                    }
                    break;

                case 0x00A1:
                    if(keys[V[(opcode & 0x0F00) >> 8]] == 0) {
                        pc += 2;
                    }
                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
                    pc += 2;
                    break;

            }

            pc += 2;
            break;

        case 0xF000:
            switch(opcode & 0x00FF) {
                case 0x0007:
                    V[(opcode & 0x0F00) >> 8] = delayTimer;
                    break;

                case 0x000A:
                    keyWait = true;
                    keySpot = ((opcode & 0x0F00) >> 8);
                    break;

                case 0x0015:
                    delayTimer = V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x0018:
                    soundTimer = V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x001E:
                    if((I + V[(opcode & 0x0F00) >> 8]) > 0xFFF) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    I += V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x0029:
                    I = V[(opcode & 0x0F00) >> 8] * 0x5;
                    break;

                case 0x0033:
                    memory[I]     = V[(opcode & 0x0F00) >> 8] / 100;
                    memory[I + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[I + 2] = (V[(opcode & 0x0F00) >> 8] % 100) % 10;
                    break;

                case 0x0055:
                    for (int i = 0; i <= V[(opcode & 0x0F00) >> 8]; i++) {
                        memory[I + i] = V[i];
                    }

                    I += ((opcode & 0x0F00) >> 8) + 1;
                    break;

                case 0x0065:
                    for (int i = 0; i <= V[(opcode & 0x0F00) >> 8]; i++) {
                        V[i] = memory[I + i];
                    }
                    I += ((opcode & 0x0F00) >> 8) + 1;
                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
                    break;
            }

            pc += 2;
            break;

        default:
            printf("Unknown opcode: 0x%04x\n", opcode);
            pc += 2;
            break;
    }

    if(delayTimer > 0) {
        delayTimer--;
    }
    if(soundTimer > 0) {
        if(soundTimer == 1) {
            // TODO: Make sound
        }
        soundTimer--;
    }
    return;
}

// TODO: Make this not retarded
int decodeKey(SDL_Keycode k) {
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
