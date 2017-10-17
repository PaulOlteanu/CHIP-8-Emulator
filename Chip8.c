#ifndef CHIP8_C_INCLUDE
#define CHIP8_C_INCLUDE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "Chip8.h"

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
const uint8_t font[16 * 5] =
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

int initialize(chip8 *c8, char *filename) {
    // The program starts at location 512 aka 0x200, anything before that is reserved
    c8->programCounter = 0x200;
    c8->I              = 0;
    c8->stackPointer   = 0;
    c8->opcode         = 0;
    c8->keySpot        = 0;
    c8->delayTimer     = 0;
    c8->soundTimer     = 0;
    c8->keyWait        = false;
    c8->awaitingRedraw = false;

    // Clear things
    for(int i = 0; i < 64 * 32; i++) {
        c8->screen[i] = 0;
    }
    for(int i = 0; i < 16; i++) {
        c8->V[i] = 0;
        c8->stack[i] = 0;
        c8->keys[i] = 0;
    }
    for(int i = 0; i < 4096; i++) {
        c8->memory[i]  = 0;
    }

    // Load the font from the font array into reserved c8->memory
    for(int i = 0; i < 16 * 5; i++) {
        c8->memory[i] = font[i];
    }

    // Read the program from the file into c8->memory
    FILE *file = fopen(filename, "rb");
    if(!file) {
        printf("Could not find ROM\n");
        return -1;
    }

    // 0x0200 == 512 This is the c8->memory location where the program starts
    int counter = 0x0200;
    while(!feof(file)) {
        fread(&c8->memory[counter], 1, 1, file);
        counter++;
    }

    fclose(file);
    return 0;
}

void emulateCycle(chip8 *c8) {
    // CHIP-8 is big-endian so memory[pc] is the "left" half of the opcode
    c8->opcode = (c8->memory[c8->programCounter] << 8) | c8->memory[c8->programCounter + 1];

    // Switch on the leftmost nibble of the c8->opcode
    switch(c8->opcode & 0xF000) {
        case 0x0000:
            switch(c8->opcode & 0x00FF) {
                case 0x00E0:
                    for(int i = 0; i < 64 * 32; i++) {
                        c8->screen[i] = 0;
                    }
                    c8->awaitingRedraw = true;
                    break;

                case 0x00EE:
                    c8->stackPointer--;
                    c8->programCounter = c8->stack[c8->stackPointer];
                    break;

                default:
                    printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
                    break;
            }

            c8->programCounter += 2;
            break;

        case 0x1000:
            c8->programCounter = (c8->opcode & 0x0FFF);
            break;

        case 0x2000:
            c8->stack[c8->stackPointer] = c8->programCounter;
            c8->stackPointer++;
            c8->programCounter = (c8->opcode & 0x0FFF);
            break;

        case 0x3000:
            if(c8->V[(c8->opcode & 0x0F00) >> 8] == (c8->opcode & 0x00FF)) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x4000:
            if(c8->V[(c8->opcode & 0x0F00) >> 8] != (c8->opcode & 0x00FF)) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x5000:
            if(c8->V[(c8->opcode & 0x0F00) >> 8] == c8->V[(c8->opcode & 0x00F0) >> 4]) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x6000:
            c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->opcode & 0x00FF);
            c8->programCounter += 2;
            break;

        case 0x7000:
            c8->V[(c8->opcode & 0x0F00) >> 8] += (c8->opcode & 0x00FF);
            c8->programCounter += 2;
            break;

        case 0x8000:
            switch(c8->opcode & 0x000F) {
                case 0x0000:
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0001:
                    c8->V[(c8->opcode & 0x0F00) >> 8] |= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0002:
                    c8->V[(c8->opcode & 0x0F00) >> 8] &= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0003:
                    c8->V[(c8->opcode & 0x0F00) >> 8] ^= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0004:
                    // This checks for overflow by checking if one of the operands is greater than the max 8 bit value minus the other operand
                    if(c8->V[(c8->opcode & 0x00F0) >> 4] > (0xFF - c8->V[(c8->opcode & 0x0F00) >> 8])) {
                        c8->V[0xF] = 1;
                    } else {
                        c8->V[0xF] = 0;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] += c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0005:
                    if(c8->V[(c8->opcode & 0x0F00) >> 8] < c8->V[(c8->opcode & 0x00F0) >> 4]) {
                        c8->V[0xF] = 0;
                    } else {
                        c8->V[0xF] = 1;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] -= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                // I think this is technically inaccurate since the original implementation did something different
                // Most programs written for the CHIP-8 assume this c8->opcode works like this though
                case 0x0006:
                    c8->V[0xF] = (c8->V[(c8->opcode & 0x0F00) >> 8] & 1);
                    c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x0F00) >> 8]) >> 1;
                    break;

                case 0x0007:
                    if(c8->V[(c8->opcode & 0x0F00) >> 8] > c8->V[(c8->opcode & 0x00F0) >> 4]) {
                        c8->V[0xF] = 0;
                    } else {
                        c8->V[0xF] = 1;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4] - c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                // See comment above instruction 0x8XY6
                case 0x000E:
                    c8->V[0xF] = (c8->V[(c8->opcode & 0x0F00) >> 8] >> 7) & 1;
                    c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x0F00) >> 8]) << 1;
                    break;

                default:
                    printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
                    break;
            }

            c8->programCounter += 2;
            break;

        case 0x9000:
            if(c8->V[(c8->opcode & 0x0F00) >> 8] != c8->V[(c8->opcode & 0x00F0) >> 4]) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0xA000:
            c8->I = (c8->opcode & 0x0FFF);
            c8->programCounter += 2;
            break;

        case 0xB000:
            c8->programCounter = c8->V[0] + (c8->opcode & 0x0FFF);
            break;

        case 0xC000:
            ;
            int n = (rand() % (0xFF + 1));
            c8->V[(c8->opcode & 0x0F00) >> 8] = (n & (c8->opcode & 0x00FF));
            c8->programCounter += 2;
            break;

        // TODO: Make this not copy-pasted shit
        case 0xD000:
            ;
            uint16_t x = c8->V[(c8->opcode & 0x0F00) >> 8];
            uint16_t y = c8->V[(c8->opcode & 0x00F0) >> 4];
            uint16_t height = c8->opcode & 0x000F;
            uint16_t pixel;

            c8->V[0xF] = 0;
            for (int ypos = 0; ypos < height; ypos++) {
                pixel = c8->memory[c8->I + ypos];

                for(int xpos = 0; xpos < 8; xpos++) {
                    if((pixel & (0x80 >> xpos)) != 0) {
                        if(c8->screen[(x + xpos + ((y + ypos) * 64))] == 1) {
                            c8->V[0xF] = 1;
                        }
                        c8->screen[x + xpos + ((y + ypos) * 64)] ^= 1;
                    }
                }
            }
            c8->awaitingRedraw = true;
            c8->programCounter += 2;
            break;

        case 0xE000:
            switch(c8->opcode & 0x00FF) {
                case 0x009E:
                    if(c8->keys[c8->V[(c8->opcode & 0x0F00) >> 8]] != 0) {
                        c8->programCounter += 2;
                    }
                    break;

                case 0x00A1:
                    if(c8->keys[c8->V[(c8->opcode & 0x0F00) >> 8]] == 0) {
                        c8->programCounter += 2;
                    }
                    break;

                default:
                    printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
                    break;

            }
            c8->programCounter += 2;
            break;

        case 0xF000:
            switch(c8->opcode & 0x00FF) {
                case 0x0007:
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->delayTimer;
                    break;

                case 0x000A:
                    c8->keyWait = true;
                    c8->keySpot = ((c8->opcode & 0x0F00) >> 8);
                    break;

                case 0x0015:
                    c8->delayTimer = c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x0018:
                    c8->soundTimer = c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x001E:
                    if((c8->I + c8->V[(c8->opcode & 0x0F00) >> 8]) > 0xFFF) {
                        c8->V[0xF] = 1;
                    } else {
                        c8->V[0xF] = 0;
                    }
                    c8->I += c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x0029:
                    c8->I = c8->V[(c8->opcode & 0x0F00) >> 8] * 0x5;
                    break;

                // TODO: Explain this
                case 0x0033:
                    c8->memory[c8->I]     =  c8->V[(c8->opcode & 0x0F00) >> 8] / 100;
                    c8->memory[c8->I + 1] = (c8->V[(c8->opcode & 0x0F00) >> 8] / 10) % 10;
                    c8->memory[c8->I + 2] = (c8->V[(c8->opcode & 0x0F00) >> 8] % 100) % 10;
                    break;

                case 0x0055:
                    for (int i = 0; i <= ((c8->opcode & 0x0F00) >> 8); i++) {
                        c8->memory[c8->I + i] = c8->V[i];
                    }

                    c8->I += ((c8->opcode & 0x0F00) >> 8) + 1;
                    break;

                case 0x0065:
                    for (int i = 0; i <= ((c8->opcode & 0x0F00) >> 8); i++) {
                        c8->V[i] = c8->memory[c8->I + i];
                    }
                    c8->I += ((c8->opcode & 0x0F00) >> 8) + 1;
                    break;

                default:
                    printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
                    break;
            }

            c8->programCounter += 2;
            break;

        default:
            printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
            c8->programCounter += 2;
            break;
    }

    if(c8->delayTimer > 0) {
        c8->delayTimer--;
    }
    if(c8->soundTimer > 0) {
        if(c8->soundTimer == 1) {
            // TODO: Make sound
        }
        c8->soundTimer--;
    }
    return;
}

#endif // CHIP8_C_INCLUDE
