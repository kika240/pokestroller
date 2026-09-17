#pragma once
#include <array>
#include <cstdint>

#include "core/memory/interface.h"

#define ROM_SIZE 0xC000
#define ROM_START 0x0000
#define ROM_END 0xBFFF

using RomBuffer = std::array<uint8_t, ROM_SIZE>;

class ROM : public MemoryInterface
{
public:
    explicit ROM(RomBuffer buffer);

    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    void* Ptr(uint16_t address) override;

private:
    RomBuffer rom = {};
};
