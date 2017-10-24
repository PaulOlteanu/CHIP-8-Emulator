#ifndef CHIP8_H_INCLUDE
#define CHIP8_H_INCLUDE

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#define I_MAX(A, B) (A > B ? A : B)

typedef struct {
    // MEMORY THINGS ===============================================================
    uint8_t  V[16]; // Registers. V[0xF] is the carry flag
    uint8_t  memory[4096];
    uint16_t opcode, I, programCounter, stackPointer;
    uint16_t stack[24];

    // DISPLAY THINGS ==============================================================
    uint8_t screen[64 * 32];
    bool awaitingRedraw;

    // KEYBOARD THINGS =============================================================
    uint8_t keys[16];
    bool    keyWait;
    uint8_t keySpot;

    // MISC THINGS =================================================================
    uint8_t delayTimer;
    uint8_t soundTimer;
    bool modernCompat;
    struct timespec previousDelayTimerTick;
    struct timespec previousSoundTimerTick;
    struct timespec currentTime;
} chip8;

int initialize(chip8 *c8, char *filename, bool modernCompat);
int emulateCycle(chip8 *c8);

#endif // CHIP8_H_INCLUDE
