#pragma once
#include <array>

#include "interface.h"

template <uint16_t TSize>
class Memory : public MemoryInterface
{
public:
    uint8_t Read(uint16_t address) override
    {
        return this->buffer[address];
    }

    void Write(uint16_t address, uint8_t value) override
    {
        this->buffer[address] = value;
    }

    void* Ptr(uint16_t address) override
    {
        return &this->buffer[address];
    }

private:

    std::array<uint8_t, TSize> buffer = {};
};
