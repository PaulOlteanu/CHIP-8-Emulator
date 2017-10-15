#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL.h>

#define I_MAX(A, B) (A > B ? A : B)

// Characters are 4 pixels wide and 5 tall
// The top nibble of the byte is used to set the pixels displayed for the character
// For example, 0 is 0xF0, 0x90, 0x90, 0x90, 0xF0
// This is shown as:
// ****    0xF0 = 1111 0000
// *..*    0x90 = 1001 0000
// *..*    0x90 = 1001 0000
// *..*    0x90 = 1001 0000
// ****    0xF0 = 1111 0000
// The '*'s correspond to the bits that are set to 1 in the number
const unsigned char font[16 * 5] =
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

// MEMORY THINGS ===============================================================
unsigned short opcode;
unsigned char  memory[4096];
unsigned char  V[16]; // Registers. V[0xF] is the carry flag
unsigned short I;
unsigned short pc; // Program counter
unsigned short stack[24];
unsigned short sp; // Stack pointer

// DISPLAY THINGS ==============================================================
unsigned char      screen[64 * 32];
const unsigned int FPS_CAP = 360;
const int          SCALE = 5;

bool redraw;

// KEYBOARD THINGS =============================================================
unsigned char keys[16];
bool          keyWait;
unsigned char keySpot;

// MISC THINGS =================================================================
unsigned char delayTimer;
unsigned char soundTimer;
bool quit;

// FUNCTION PROTOTYPES =========================================================
int decodeKey(SDL_Keycode);
int initialize(char *filename);
void emulateCycle();

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Incorrect usage. Expected a filename for the rom\n");
        return -1;
    }

    // TODO: Maybe get the scaling factor from the user
    char *filename = argv[1];
    if(initialize(filename) != 0) {
        printf("Could not initialize emulator :(\n");
        return -1;
    }

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
    unsigned int frameStartTick;
    unsigned int frameEndTick;
    unsigned int frameChange;

    // Rectangle for drawing pixels? Yes, because it makes scaling up the display O(1) (in this code)
    SDL_Rect pixel;
    int renderColour;

    // The type of key event that happened, and the key that was pressed
    SDL_Event event;
    int changedKey;

    while(!quit) {
        if (pc >= 4096) {
            printf("Error: Program out of bounds\n");
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        // This is for later calculating how long this frame took
        frameStartTick = SDL_GetTicks();

        // INPUT HANDLING
        if (keyWait) {
            while (!SDL_PollEvent(&event) || decodeKey(event.key.keysym.sym) == -1 || event.type != SDL_KEYDOWN) {}
            changedKey = decodeKey(event.key.keysym.sym);
            V[keySpot] = changedKey;
            keyWait = false;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP){
                changedKey = decodeKey(event.key.keysym.sym);
                if (changedKey != -1) {
                    // If the event is a keydown, set the key to true in the array, otherwise, false
                    keys[changedKey] = (event.type == SDL_KEYDOWN);
                }
            }
        }

        emulateCycle();

        if (redraw) {
            for (int i = 0; i < 64 * 32; i++) {
                // Render colour is either 255 if screen[i] == 1 or 0 if screen[i] == 0
                // The I_MAX ensures that pixels are never 100% black
                renderColour = I_MAX(255 * screen[i], 35);
                SDL_SetRenderDrawColor(renderer, renderColour, renderColour, renderColour, renderColour);

                pixel.x = (i % 64) * SCALE;
                pixel.y = (i / 64) * SCALE;
                pixel.w = SCALE;
                pixel.h = SCALE;

                SDL_RenderFillRect(renderer, &pixel);
            }

            SDL_RenderPresent(renderer);
            redraw = false;
        }

        // Limit the FPS
        // Every frame should take (1000 ms) / FPS_CAP ticks to complete
        // If they take less, sleep for the number of ticks left over
        frameEndTick = SDL_GetTicks();
        frameChange = frameEndTick - frameStartTick;
        if(frameChange < 1000 / FPS_CAP) {
            SDL_Delay((1000 / FPS_CAP) - frameChange);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int initialize(char *filename) {
    // The program starts at location 512 aka 0x200, anything before that is reserved
    pc = 0x200;
    I          = 0;
    sp         = 0;
    opcode     = 0;
    keySpot    = 0;
    delayTimer = 0;
    soundTimer = 0;
    keyWait    = false;
    redraw     = false;
    quit       = false;

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
        printf("Could not find ROM\n");
        return -1;
    }

    // 0x0200 == 512 This is the memory location where the program starts
    int counter = 0x0200;
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

    // Switch on the leftmost nibble of the opcode
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF) {
                case 0x00E0:
                    for(int i = 0; i < 64 * 32; i++) {
                        screen[i] = 0;
                    }
                    redraw = true;
                    break;

                case 0x00EE:
                    sp--;
                    pc = stack[sp];
                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
                    break;
            }

            pc += 2;
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

                case 0x0004:
                    // This checks for overflow by checking if one of the operands is greater than the max 8 bit value minus the other operand
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

                // I think this is technically inaccurate since the original implementation did something different
                // Most programs written for the CHIP-8 assume this opcode works like this though
                case 0x0006:
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] & 1);
                    V[(opcode & 0x0F00) >> 8] = (V[(opcode & 0x0F00) >> 8]) >> 1;
                    break;

                case 0x0007:
                    if(V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 0;
                    } else {
                        V[0xF] = 1;
                    }
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
                    break;

                // See comment above instruction 0x8XY6
                case 0x000E:
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] >> 7) & 1;
                    V[(opcode & 0x0F00) >> 8] = (V[(opcode & 0x0F00) >> 8]) << 1;
                    break;

                default:
                    printf("Unknown opcode: 0x%04x\n", opcode);
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

                // TODO: Explain this
                case 0x0033:
                    memory[I]     =  V[(opcode & 0x0F00) >> 8] / 100;
                    memory[I + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[I + 2] = (V[(opcode & 0x0F00) >> 8] % 100) % 10;
                    break;

                case 0x0055:
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
                        memory[I + i] = V[i];
                    }

                    I += ((opcode & 0x0F00) >> 8) + 1;
                    break;

                case 0x0065:
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
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

// This is disgusting, but probably the simplest (and best) way to do it
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
