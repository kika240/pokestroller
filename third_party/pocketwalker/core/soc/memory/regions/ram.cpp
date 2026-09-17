#include "ram.h"

uint8_t RAM::Read(uint16_t address)
{
    return this->ram[address - RAM_START];
}

void RAM::Write(uint16_t address, uint8_t value)
{
    this->ram[address - RAM_START] = value;
}

void* RAM::Ptr(uint16_t address)
{
    return &this->ram[address - RAM_START];
}
