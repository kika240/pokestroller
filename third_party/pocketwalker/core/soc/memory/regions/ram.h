#pragma once
#include <array>

#include "core/memory/interface.h"

#define RAM_SIZE 0x800
#define RAM_START 0xF780
#define RAM_END 0xFF7F

class RAM : public MemoryInterface
{
public:
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    void* Ptr(uint16_t address) override;

private:
    std::array<uint8_t, RAM_SIZE> ram = {};
};
