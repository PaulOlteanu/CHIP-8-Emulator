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
unsigned short ip;
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
        return -1;
    }

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
    ip = 0;
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
        fread(&buf, 1, 1, file);
        memory[counter] = *buf;
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
                    break;

                case 0x00EE:
                    sp--;
                    pc = stack[sp];
                    break;

                default:
                    printf("Unknown opcode: 0x%04x", opcode);
            }

            break;

        default:
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
