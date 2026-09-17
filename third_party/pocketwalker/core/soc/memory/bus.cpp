#include "bus.h"

#include <memory>


MemoryBus::MemoryBus(std::shared_ptr<ROM> rom, std::shared_ptr<RAM> ram, std::shared_ptr<IO> io)
{
    this->rom = rom;
    this->ram = ram;
    this->io = io;
}

uint8_t MemoryBus::Read(uint16_t address)
{
    if (address >= ROM_START && address <= ROM_END)
        return rom->Read(address);

    if (address >= RAM_START && address <= RAM_END)
        return ram->Read(address);

    if (address >= IO_LOW_START && address <= IO_LOW_END)
        return io->Read(address);

    if (address >= IO_HIGH_START && address <= IO_HIGH_END)
        return io->Read(address);

    return 0xFF;
}

void MemoryBus::Write(uint16_t address, uint8_t value)
{
    if (address >= ROM_START && address <= ROM_END)
    {
        rom->Write(address, value);
        return;
    }

    if (address >= RAM_START && address <= RAM_END)
    {
        ram->Write(address, value);
        return;
    }

    if (address >= IO_LOW_START && address <= IO_LOW_END)
    {
        io->Write(address, value);
        return;
    }

    if (address >= IO_HIGH_START && address <= IO_HIGH_END)
    {
        io->Write(address, value);
        return;
    }
}

void* MemoryBus::Ptr(uint16_t address)
{
    if (address >= ROM_START && address <= ROM_END)
        return rom->Ptr(address);

    if (address >= RAM_START && address <= RAM_END)
        return ram->Ptr(address);

    if (address >= IO_LOW_START && address <= IO_LOW_END)
        return io->Ptr(address);

    if (address >= IO_HIGH_START && address <= IO_HIGH_END)
        return io->Ptr(address);

    return nullptr;
}
