#ifndef CHIP8_H_INCLUDE
#define CHIP8_H_INCLUDE

#include <stdbool.h>
#define I_MAX(A, B) (A > B ? A : B)

typedef struct {
    // MEMORY THINGS ===============================================================
    unsigned char  V[16]; // Registers. V[0xF] is the carry flag
    unsigned char  memory[4096];
    unsigned short opcode, I, programCounter, stackPointer;
    unsigned short stack[24];

    // DISPLAY THINGS ==============================================================
    unsigned char screen[64 * 32];
    bool awaitingRedraw;

    // KEYBOARD THINGS =============================================================
    unsigned char keys[16];
    bool          keyWait;
    unsigned char keySpot;

    // MISC THINGS =================================================================
    unsigned char delayTimer;
    unsigned char soundTimer;
} chip8;

int initialize(chip8 *c8, char *filename);
void emulateCycle(chip8 *c8);

#endif // CHIP8_H_INCLUDE
