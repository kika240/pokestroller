#pragma once
#include "adc/adc.h"
#include "core/cpu/cpu.h"
#include "core/utils/event_handler.h"
#include "interrupts/interrupts.h"
#include "memory/bus.h"
#include "memory/regions/rom.h"
#include "rtc/rtc.h"
#include "sci3/sci3.h"
#include "ssu/ssu.h"
#include "timer/timerb1.h"
#include "timer/timerw.h"

#define SOC_ADDR_CKSTPR1 0xFFFA
#define SOC_ADDR_CKSTPR2 0xFFFB

union CKSTPR1_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t RTCCKSTP : 1;
        uint8_t FROMCKSTP : 1;
        uint8_t TB1CKSTP : 1;
        uint8_t  : 1;
        uint8_t ADCKSTP : 1;
        uint8_t  : 1;
        uint8_t S3CKSTP : 1;
        uint8_t  : 1;
    };
};


union CKSTPR2_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t  : 1;
        uint8_t COMPCKSTP : 1;
        uint8_t WDCKSTP : 1;
        uint8_t AECCKSTP : 1;
        uint8_t SSUCKSTP : 1;
        uint8_t IICCKSTP : 1;
        uint8_t TWCKSTP : 1;
        uint8_t  : 1;
    };
};


class H838606
{
public:
    explicit H838606(RomBuffer rom_buffer);

    uint8_t Cycle();

    std::shared_ptr<MemoryBus> memory = nullptr;

    std::shared_ptr<CPU> cpu = nullptr;
    std::shared_ptr<Interrupts> interrupts = nullptr;

    std::shared_ptr<SSU> ssu = nullptr;
    std::shared_ptr<SCI3> sci3 = nullptr;

    std::shared_ptr<TimerB1> timer_b1 = nullptr;
    std::shared_ptr<TimerW> timer_w = nullptr;

    std::shared_ptr<RTC> rtc = nullptr;
    std::shared_ptr<ADC> adc = nullptr;

    CKSTPR1_t CKSTPR1 = {};
    CKSTPR2_t CKSTPR2 = {};
};
