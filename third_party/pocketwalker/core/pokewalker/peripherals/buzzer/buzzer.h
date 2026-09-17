#pragma once
#include "core/soc/timer/timerw.h"
#include "core/utils/event_handler.h"

// Note: This is not hardware accurate as the real buzzer receives an analog square wave signal from FTIOB/FTIOC
// This was added as a "wrapper" to create a separate "peripheral" for sending over frequency samples calculated by FTIOA

struct BuzzerInformation
{
    float frequency = 0;
    bool is_full_volume = true;
};

class Buzzer
{
public:
    explicit Buzzer(const std::shared_ptr<TimerW>& timer_w);

    void Cycle(uint8_t cycles);

    EventHandler<BuzzerInformation> OnSamplePushed;

private:
    std::shared_ptr<TimerW> timer_w = nullptr;

    uint32_t buzzer_cycles = 0;
};
