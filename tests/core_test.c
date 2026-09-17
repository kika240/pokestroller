#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "walker.h"
#include "queue.h"

// Internal memory bus operations: regression coverage for 16-bit wrapping.
void setMemory16(uint32_t address, uint16_t value);
void setMemory32(uint32_t address, uint32_t value);
uint16_t getMemory16(uint32_t address);
uint32_t getMemory32(uint32_t address);

static void writeFile(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    char romPath[1024], eepromPath[1024], invalidPath[1024], error[2048];
    snprintf(romPath, sizeof(romPath), "%s/synthetic-rom.bin", argv[1]);
    snprintf(eepromPath, sizeof(eepromPath), "%s/synthetic-eeprom.bin", argv[1]);
    snprintf(invalidPath, sizeof(invalidPath), "%s/invalid.bin", argv[1]);
    uint8_t rom[65537] = {0}, eeprom[WALKER_EEPROM_SIZE], copy[WALKER_EEPROM_SIZE];
    for (size_t i = 0; i < sizeof(eeprom); i++) eeprom[i] = (uint8_t)(i * 37);
    // H8 BRA -2: a tiny original program that loops without Nintendo code.
    rom[0x2c4] = 0x40;
    rom[0x2c5] = 0xfe;
    writeFile(romPath, rom, WALKER_ROM_SIZE);
    writeFile(eepromPath, eeprom, sizeof(eeprom));
    assert(!copyWalkerEEPROM(copy, sizeof(copy)));
    assert(!initWalkerFromFiles(NULL, eepromPath, error, sizeof(error)));
    assert(error[0]);
    assert(initWalkerFromFiles(romPath, eepromPath, error, sizeof(error)));
    assert(!error[0]);
    assert(!copyWalkerEEPROM(copy, sizeof(copy) - 1));
    assert(copyWalkerEEPROM(copy, sizeof(copy)));
    assert(memcmp(copy, eeprom, sizeof(copy)) == 0);

    uint64_t cycles = 0;
    for (int i = 0; i < 100000; i++) assert(runNextInstruction(&cycles) == 0);
    assert(cycles == 200000);
    assert(walkerProgramCounter() == 0x2c4);

    const size_t invalidSizes[] = {0, 1, WALKER_ROM_SIZE - 1, WALKER_ROM_SIZE + 1, 65537};
    for (size_t i = 0; i < sizeof(invalidSizes) / sizeof(invalidSizes[0]); i++) {
        writeFile(invalidPath, rom, invalidSizes[i]);
        assert(!initWalkerFromFiles(invalidPath, eepromPath, error, sizeof(error)));
        assert(error[0]);
        assert(copyWalkerEEPROM(copy, sizeof(copy)));
        assert(memcmp(copy, eeprom, sizeof(copy)) == 0);
        assert(walkerProgramCounter() == 0x2c4);
    }
    writeFile(invalidPath, eeprom, sizeof(eeprom) - 1);
    assert(!initWalkerFromFiles(romPath, invalidPath, error, sizeof(error)));
    assert(!initWalkerFromFiles(romPath, "/this/file/does/not/exist", error, sizeof(error)));
    assert(copyWalkerEEPROM(copy, sizeof(copy)));
    assert(memcmp(copy, eeprom, sizeof(copy)) == 0);
    writeFile(invalidPath, rom, sizeof(rom));
    assert(!initWalkerFromFiles(romPath, invalidPath, error, sizeof(error)));

    // A full address-space dump is accepted, but RAM/MMIO bytes are not ROM.
    memset(rom + WALKER_ROM_SIZE, 0xa5, 65536 - WALKER_ROM_SIZE);
    writeFile(romPath, rom, 65536);
    assert(initWalkerFromFiles(romPath, eepromPath, error, sizeof(error)));
    assert(getMemory16(0xc000) == 0);
    setMemory16(0xffff, 0x80ff);
    assert(getMemory16(0xffff) == 0x80ff);
    setMemory32(0xfffe, 0xff123456);
    assert(getMemory32(0xfffe) == 0xff123456);
    // Fetching operands near the boundary must not read beyond host memory.
    setMemory16(0x2c4, 0x5a00); // JMP @0xfffe
    setMemory16(0x2c6, 0xfffe);
    setMemory16(0xfffe, 0x40fe); // BRA -2
    for (int i = 0; i < 10; i++) assert(runNextInstruction(&cycles) == 0);
    assert(walkerProgramCounter() == 0xfffe);

    // Draining and refilling the original queue used to write through freed memory.
    struct Queue queue = {0};
    for (int i = 0; i < 1000; i++) {
        addElement(&queue, LEFT);
        addElement(&queue, RIGHT);
        assert(popElement(&queue) == LEFT);
        assert(popElement(&queue) == RIGHT);
        assert(isEmpty(&queue) && queue.last == NULL);
    }
    assert(popElement(&queue) == 0);
    for (int i = 0; i < 100; i++) {
        setKeys(LEFT);
        assert(initWalkerFromFiles(romPath, eepromPath, error, sizeof(error)));
        assert(walkerProgramCounter() == 0x2c4);
    }
    uint32_t frame[LCD_WIDTH * LCD_HEIGHT];
    fillVideoBuffer(frame);
    for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) assert(frame[i] == 0xcccccc);
    puts("PASS: queue reuse, image validation, atomic reload, EEPROM copy, CPU loop, memory wrapping");
    return 0;
}
