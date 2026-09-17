#include "interface.h"

#include <cstdint>

uint8_t MemoryInterface::Read8(uint16_t address)
{
    return Read(address);
}

uint16_t MemoryInterface::Read16(uint16_t address)
{
    return Read(address) << 8 | Read(address + 1);
}

uint32_t MemoryInterface::Read32(uint16_t address)
{
    return (uint32_t) Read(address) << 24 | Read(address + 1) << 16 | Read(address + 2) << 8 | Read(address + 3);
}

void MemoryInterface::Write8(uint16_t address, uint8_t value)
{
    Write(address, value);
}

void MemoryInterface::Write16(uint16_t address, uint16_t value)
{
    Write(address, (value >> 8) & 0xFF);
    Write(address + 1, (value >> 0) & 0xFF);
}

void MemoryInterface::Write32(uint16_t address, uint32_t value)
{
    Write(address, (value >> 24) & 0xFF);
    Write(address + 1, (value >> 16) & 0xFF);
    Write(address + 2, (value >> 8) & 0xFF);
    Write(address + 3, (value >> 0) & 0xFF);
}

h8300h_ptr<uint8_t> MemoryInterface::Ptr8(uint16_t address)
{
    return h8300h_ptr(static_cast<uint8_t*>(Ptr(address)));
}

h8300h_ptr<uint16_t> MemoryInterface::Ptr16(uint16_t address)
{
    return h8300h_ptr(static_cast<uint16_t*>(Ptr(address)));
}

h8300h_ptr<uint32_t> MemoryInterface::Ptr32(uint16_t address)
{
    return h8300h_ptr(static_cast<uint32_t*>(Ptr(address)));
}
