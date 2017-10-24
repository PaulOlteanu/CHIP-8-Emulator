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

int initialize(chip8 *c8, char *filename, bool modernCompat) {
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
    c8->modernCompat   = modernCompat;
    clock_gettime(CLOCK_REALTIME, &(c8->previousDelayTimerTick));
    clock_gettime(CLOCK_REALTIME, &(c8->previousSoundTimerTick));

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

    // Load the font from the font array into reserved memory
    for(int i = 0; i < 16 * 5; i++) {
        c8->memory[i] = font[i];
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
        if(counter >= 4096) {
            printf("ROM too large or invalid file type\n");
            return -1;
        }
        fread(&c8->memory[counter], 1, 1, file);
        counter++;
    }

    fclose(file);
    return 0;
}

int emulateCycle(chip8 *c8) {
    if(c8->programCounter >= 4096) {
        return -1;
    }
    // CHIP-8 is big-endian so memory[pc] is the "left" half of the opcode
    c8->opcode = (c8->memory[c8->programCounter] << 8) | c8->memory[c8->programCounter + 1];

    // Switch on the leftmost nibble of the opcode
    switch(c8->opcode & 0xF000) {
        case 0x0000:
            switch(c8->opcode & 0x00FF) {
                case 0x00E0:
                    // 00E0: Clear the screen
                    for(int i = 0; i < 64 * 32; i++) {
                        c8->screen[i] = 0;
                    }
                    c8->awaitingRedraw = true;
                    break;

                case 0x00EE:
                    // 00EE: Return from a subroutine
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
            // 1NNN: Jump to address NNN
            c8->programCounter = (c8->opcode & 0x0FFF);
            break;

        case 0x2000:
            // 2NNN: Execute subroutine starting at address NNN
            c8->stack[c8->stackPointer] = c8->programCounter;
            c8->stackPointer++;
            c8->programCounter = (c8->opcode & 0x0FFF);
            break;

        case 0x3000:
            // 3XNN: Skip the following instruction if the value of register VX equals NN
            if(c8->V[(c8->opcode & 0x0F00) >> 8] == (c8->opcode & 0x00FF)) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x4000:
            // 4XNN: Skip the following instruction if the value of register VX is not equal to NN
            if(c8->V[(c8->opcode & 0x0F00) >> 8] != (c8->opcode & 0x00FF)) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x5000:
            // 5XY0: Skip the following instruction if the value of register VX is equal to the value of register VY
            if(c8->V[(c8->opcode & 0x0F00) >> 8] == c8->V[(c8->opcode & 0x00F0) >> 4]) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0x6000:
            // 6XNN: Store number NN in register VX
            c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->opcode & 0x00FF);
            c8->programCounter += 2;
            break;

        case 0x7000:
            // 7XNN: Add the value NN to register VX
            c8->V[(c8->opcode & 0x0F00) >> 8] += (c8->opcode & 0x00FF);
            c8->programCounter += 2;
            break;

        case 0x8000:
            switch(c8->opcode & 0x000F) {
                case 0x0000:
                    // 8XY0: Store the value of register VY in register VX
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0001:
                    // 8XY1: Set VX to VX OR VY
                    c8->V[(c8->opcode & 0x0F00) >> 8] |= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0002:
                    // 8XY2: Set VX to VX AND VY
                    c8->V[(c8->opcode & 0x0F00) >> 8] &= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0003:
                    // 8XY3: Set VX to VX XOR VY
                    c8->V[(c8->opcode & 0x0F00) >> 8] ^= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0004:
                    // 8XY4: Add the value of register VY to register VX. Set VF to 01 if a carry occurs. Set VF to 00 if a carry does not occur
                    // This checks for overflow by checking if one of the operands is greater than the max 8 bit value minus the other operand
                    if(c8->V[(c8->opcode & 0x00F0) >> 4] > (0xFF - c8->V[(c8->opcode & 0x0F00) >> 8])) {
                        c8->V[0xF] = 1;
                    } else {
                        c8->V[0xF] = 0;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] += c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0005:
                    // 8XY5: Subtract the value of register VY from register VX. Set VF to 00 if a borrow occurs. Set VF to 01 if a borrow does not occur
                    if(c8->V[(c8->opcode & 0x0F00) >> 8] < c8->V[(c8->opcode & 0x00F0) >> 4]) {
                        c8->V[0xF] = 0;
                    } else {
                        c8->V[0xF] = 1;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] -= c8->V[(c8->opcode & 0x00F0) >> 4];
                    break;

                case 0x0006:
                    // 8XY6: Store the value of register VY shifted right one bit in register VX. Set register VF to the least significant bit prior to the shift
                    // Historically VY was shifted and the value was copied to VX
                    // Modern programs (incorrectly) assume VX is shifted
                    if (c8->modernCompat) {
                        c8->V[0xF] = (c8->V[(c8->opcode & 0x0F00) >> 8] & 1);
                        c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x0F00) >> 8]) >> 1;
                    } else {
                        c8->V[0xF] = (c8->V[(c8->opcode & 0x00F0) >> 4] & 1);
                        c8->V[(c8->opcode & 0x00F0) >> 4] = (c8->V[(c8->opcode & 0x00F0) >> 4]) >> 1;
                        c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x00F0) >> 4]);
                    }
                    break;

                case 0x0007:
                    // 8XY7: Set register VX to the value of VY minus VX. Set VF to 00 if a borrow occurs. Set VF to 01 if a borrow does not occur
                    if(c8->V[(c8->opcode & 0x0F00) >> 8] > c8->V[(c8->opcode & 0x00F0) >> 4]) {
                        c8->V[0xF] = 0;
                    } else {
                        c8->V[0xF] = 1;
                    }
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4] - c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x000E:
                    // 8XYE: Store the value of register VY shifted left one bit in register VX. Set register VF to the most significant bit prior to the shift
                    // Historically VY was shifted and the value was copied to VX
                    // Modern programs (incorrectly) assume VX is shifted
                    if (c8->modernCompat) {
                        c8->V[0xF] = (c8->V[(c8->opcode & 0x0F00) >> 8] >> 7) & 1;
                        c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x0F00) >> 8]) << 1;
                    } else {
                        c8->V[0xF] = (c8->V[(c8->opcode & 0x00F0) >> 4] >> 7) & 1;
                        c8->V[(c8->opcode & 0x00F0) >> 4] = (c8->V[(c8->opcode & 0x00F0) >> 4]) << 1;
                        c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->V[(c8->opcode & 0x00F0) >> 4]);
                    }
                    break;

                default:
                    printf("Unknown c8->opcode: 0x%04x\n", c8->opcode);
                    break;
            }

            c8->programCounter += 2;
            break;

        case 0x9000:
            // 9XY0: Skip the following instruction if the value of register VX is not equal to the value of register VY
            if(c8->V[(c8->opcode & 0x0F00) >> 8] != c8->V[(c8->opcode & 0x00F0) >> 4]) {
                c8->programCounter += 2;
            }
            c8->programCounter += 2;
            break;

        case 0xA000:
            // ANNN: Store memory address NNN in register I
            c8->I = (c8->opcode & 0x0FFF);
            c8->programCounter += 2;
            break;

        case 0xB000:
            // BNNN: Jump to address NNN + V0
            c8->programCounter = c8->V[0] + (c8->opcode & 0x0FFF);
            break;

            // TODO: EXPLAIN THIS MORE
        case 0xC000:
            // CXNN: Set VX to a random number with a mask of NN
            ;
            int n = (rand() % (0xFF + 1));
            c8->V[(c8->opcode & 0x0F00) >> 8] = (n & (c8->opcode & 0x00FF));
            c8->programCounter += 2;
            break;

            // TODO: Explain this
            // DXYN: Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
            // Set VF to 01 if any set pixels are changed to unset, and 00 otherwise
        case 0xD000:
            ;
            uint16_t startX = c8->V[(c8->opcode & 0x0F00) >> 8];
            uint16_t startY = c8->V[(c8->opcode & 0x00F0) >> 4];

            c8->V[0xF] = 0;

            for (int ypos = 0; ypos < (c8->opcode & 0x000F); ypos++) {
                for(int xpos = 0; xpos < 8; xpos++) {
                    if((c8->memory[c8->I + ypos] & (0x80 >> xpos)) != 0) {
                        if(c8->screen[(startX + xpos + ((startY + ypos) * 64))] == 1) {
                            c8->V[0xF] = 1;
                        }
                        c8->screen[startX + xpos + ((startY + ypos) * 64)] ^= 1;
                    }
                }
            }

            c8->awaitingRedraw = true;
            c8->programCounter += 2;
            break;

        case 0xE000:
            switch(c8->opcode & 0x00FF) {
                case 0x009E:
                    // EX9E: Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed
                    if(c8->keys[c8->V[(c8->opcode & 0x0F00) >> 8]] != 0) {
                        c8->programCounter += 2;
                    }
                    break;

                case 0x00A1:
                    // EXA1: Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed
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
                    // FX07: Store the current value of the delay timer in register VX
                    c8->V[(c8->opcode & 0x0F00) >> 8] = c8->delayTimer;
                    break;

                case 0x000A:
                    // FX0A: Wait for a keypress and store the result in register VX
                    c8->keyWait = true;
                    c8->keySpot = ((c8->opcode & 0x0F00) >> 8);
                    break;

                case 0x0015:
                    // FX15: Set the delay timer to the value of register VX
                    c8->delayTimer = c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x0018:
                    // FX18: Set the sound timer to the value of register VX
                    c8->soundTimer = c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x001E:
                    // FX1E: Add the value stored in register VX to register I
                    if((c8->I + c8->V[(c8->opcode & 0x0F00) >> 8]) > 0xFFF) {
                        c8->V[0xF] = 1;
                    } else {
                        c8->V[0xF] = 0;
                    }
                    c8->I += c8->V[(c8->opcode & 0x0F00) >> 8];
                    break;

                case 0x0029:
                    // FX29: Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX
                    c8->I = c8->V[(c8->opcode & 0x0F00) >> 8] * 0x5;
                    break;

                case 0x0033:
                    // FX33: Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I+1, and I+2
                    // I     = Hundreds digit of decimal equivalent
                    // I + 1 = Tens digit of decimal equivalent
                    // I + 2 = Ones digit of decimal equivalent
                    c8->memory[c8->I]     =  c8->V[(c8->opcode & 0x0F00) >> 8] / 100;
                    c8->memory[c8->I + 1] = (c8->V[(c8->opcode & 0x0F00) >> 8] / 10) % 10;
                    c8->memory[c8->I + 2] = (c8->V[(c8->opcode & 0x0F00) >> 8] % 100) % 10;
                    break;

                case 0x0055:
                    // FX55: Store the values of registers V0 to VX inclusive in memory starting at address I. I is set to I + X + 1 after operation
                    for (int i = 0; i <= ((c8->opcode & 0x0F00) >> 8); i++) {
                        c8->memory[c8->I + i] = c8->V[i];
                    }

                    c8->I += ((c8->opcode & 0x0F00) >> 8) + 1;
                    break;

                case 0x0065:
                    // FX65: Fill registers V0 to VX inclusive with the values stored in memory starting at address I. I is set to I + X + 1 after operation
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

    clock_gettime(CLOCK_REALTIME, &(c8->currentTime));
    double diff = (c8->currentTime).tv_sec - (c8->previousDelayTimerTick).tv_sec
                + ((c8->currentTime).tv_nsec - (c8->previousDelayTimerTick).tv_nsec) / 1E9;

    printf("DIFFERENCE: %lf\n", diff);
    if(((c8->currentTime.tv_sec + c8->currentTime.tv_nsec) - (c8->previousDelayTimerTick.tv_sec + c8->previousDelayTimerTick.tv_nsec)) / 1E9 >= 1/60 && c8->delayTimer > 0) {
        clock_gettime(CLOCK_REALTIME, &(c8->previousDelayTimerTick));
        c8->delayTimer--;
    }
    if(c8->soundTimer > 0) {
        if(c8->soundTimer == 1) {
            // TODO: Make sound
        }
        c8->soundTimer--;
    }
    return 0;
}

#endif // CHIP8_C_INCLUDE
