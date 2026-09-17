#pragma once
#include <memory>

#include "core/memory/interface.h"

#define PW_ACTIVITY_TIMER_RESET 90

class ActivityTimerBypass
{
public:
    explicit ActivityTimerBypass(const std::shared_ptr<MemoryInterface>& memory);

    void Cycle(uint8_t cycles);

private:
    std::shared_ptr<MemoryInterface> mem = nullptr;

    uint32_t timer_cycles = 0;
};
