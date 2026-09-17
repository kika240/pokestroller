#include "registers.h"

Registers::Registers()
{
    SP = Reg32(REG_SP_INDEX);
}

uint8_t* Registers::Reg8(uint8_t select)
{
    const uint8_t index = select & 0b111;
    const uint8_t offset = ~(select >> 3) & 1;

    const auto base = reinterpret_cast<uint8_t*>(&registers[index]);
    return base + offset;
}

uint16_t* Registers::Reg16(uint8_t select)
{
    const uint8_t index = select & 0b111;
    const uint8_t offset = (select >> 3) & 1;

    const auto base = reinterpret_cast<uint16_t*>(&registers[index]);
    return base + offset;
}

uint32_t* Registers::Reg32(uint8_t select)
{
    const uint8_t index = select & 0b111;
    return &registers[index];
}
