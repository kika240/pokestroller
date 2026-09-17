#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SYSTEM_CLOCK_CYCLES_PER_SECOND 3686400 /* 3.6864 MHz */
#define SUB_CLOCK_CYCLES_PER_SECOND 32768 /* 32.768 KHz */

#define ENTER (1<<0)
#define LEFT (1<<2)
#define RIGHT (1<<4)

#define LCD_WIDTH 96
#define LCD_HEIGHT 64

#define WALKER_ROM_SIZE (48 * 1024)
#define WALKER_EEPROM_SIZE (64 * 1024)

void initWalker(void); // Legacy Windows entry point: rom.bin and eeprom.bin
// On failure, the existing session is left untouched. Neither file is modified.
bool initWalkerFromFiles(const char* romPath, const char* eepromPath,
                         char* error, size_t errorCapacity);
bool copyWalkerEEPROM(uint8_t* destination, size_t capacity);
uint16_t walkerProgramCounter(void);
int runNextInstruction(uint64_t* cycleCount); // Must be called once every main loop iteration and given a cycleCount variable defined globally
void fillVideoBuffer(uint32_t* videoBuffer);
void setKeys(uint8_t input); // Must be called every time a key is pressed down. 'input' should be one of ENTER, LEFT or RIGHT
void quarterRTCInterrupt(void);// Must be called once every quarter second
