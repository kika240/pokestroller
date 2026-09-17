#include "rom.h"

ROM::ROM(RomBuffer buffer)
{
    this->rom = buffer;
}

uint8_t ROM::Read(uint16_t address)
{
    return this->rom[address - ROM_START];
}

void ROM::Write(uint16_t address, uint8_t value)
{
    // TODO this case is invalid (unless I add flash commands), add warning
}

void* ROM::Ptr(uint16_t address)
{
    return &this->rom[address - ROM_START];
}
