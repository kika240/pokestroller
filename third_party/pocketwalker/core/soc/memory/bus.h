#pragma once

#include <memory>

#include "core/memory/interface.h"
#include "regions/io.h"
#include "regions/ram.h"
#include "regions/rom.h"

class MemoryBus : public MemoryInterface
{
public:
    MemoryBus(std::shared_ptr<ROM> rom, std::shared_ptr<RAM> ram, std::shared_ptr<IO> io);

protected:
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    void* Ptr(uint16_t address) override;

private:
    std::shared_ptr<ROM> rom = nullptr;
    std::shared_ptr<RAM> ram = nullptr;
    std::shared_ptr<IO> io = nullptr;
};
