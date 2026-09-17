#include "activity_timer_bypass.h"

#include "core/pokewalker/pocketwalker.h"
#include "core/soc/defines.h"

ActivityTimerBypass::ActivityTimerBypass(const std::shared_ptr<MemoryInterface>& memory)
{
    this->mem = memory;
}

void ActivityTimerBypass::Cycle(uint8_t cycles)
{
    timer_cycles +=  cycles;
    while (timer_cycles >= PHI_CLK)
    {
        timer_cycles -= PHI_CLK;

        // sets the activity timer to its max value every second to bypass power-save mode
        mem->Write8(PW_ADDR_ACTIVITY_TIMER, PW_ACTIVITY_TIMER_RESET);
    }
}
