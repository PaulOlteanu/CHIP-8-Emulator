#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

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
unsigned char V[16]; // Registers
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

int initialize(char *filename);
void emulateCycle();

int main(int argc, char *argv[]) {
    char *filename = argv[1];
    if(initialize(filename) != 0) {
        printf("Could not initialize emulator :(\n");
        return -1;
    }

    printf("Successfully initialized\n");

    // TODO: Limit speed of emulation
    while(!quit) {
       emulateCycle();

       if(redraw) {
        // Graphics things
        redraw = false;
       }
        // Key input
    }
    return 0;
}

int initialize(char *filename) {
    pc = 0x200; // The program starts at location 512, anything before that is reserved
    opcode = 0;
    I = 0;
    sp = 0;

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

    // Switch on the first nibble of the opcode
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
                    break;

                default:
                    printf("Unknown opcode: 0x%04x", opcode);
            }

            break;

        case 0x1000:
            pc = opcode & 0x0FFF;
            break;

        case 0x2000:
            stack[sp] = pc;
            sp++;
            pc = opcode & 0x0FFF;
            break;

        case 0x3000:
            if(V[(opcode & 0x0F00) >> 8] == opcode & 0x00FF) {
                pc += 2;
            }
            pc += 2;
            break;

        case 0x4000:
            if(V[(opcode & 0x0F00) >> 8] != opcode & 0x00FF) {
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
                case 0x000:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x001:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] | V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x002:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] & V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x003:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] ^ V[(opcode & 0x00F0) >> 4];
                    break;

                // TODO: Add explanation for this
                // Replace with casts to int then answer comparison
                case 0x004:
                    if(V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8])) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];

                    break;

                case 0x005:
                    if(V[(opcode & 0x0F00) >> 8] < V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }

                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];

                    break;

                case 0x0006:
                    V[0xF] = (V[(opcode & 0x00F0) >> 4] & 1);
                    V[(opcode & 0x00F0) >> 4] = (V[(opcode & 0x00F0) >> 4]) >> 1;
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];

                    break;

                case 0x0007:
                    if(V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x000E:
                    V[0xF] = (V[(opcode & 0x00F0) >> 4] >> 7) & 1;
                    V[(opcode & 0x00F0) >> 4] = (V[(opcode & 0x00F0) >> 4]) << 1;
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];

                    break;

                default:
                    printf("Unknown opcode: 0x%04x", opcode);
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

        default:
            printf("Unknown opcode: 0x%04x", opcode);
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
