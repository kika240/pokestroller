#pragma once
#include <memory>

#include "core/cpu/cpu.h"
#include "core/soc/memory/regions/io.h"

#define INTERRUPT_ADDR_RTCFLG 0xF067
#define INTERRUPT_ADDR_RTCCR2 0xF06D

#define INTERRUPT_ADDR_TIERW 0xF0F2
#define INTERRUPT_ADDR_TSRW 0xF0F3

#define INTERRUPT_ADDR_IEGR 0xFFF2
#define INTERRUPT_ADDR_IENR1 0xFFF3
#define INTERRUPT_ADDR_IENR2 0xFFF4

#define INTERRUPT_ADDR_IRR1 0xFFF6
#define INTERRUPT_ADDR_IRR2 0xFFF7

#define INTERRUPT_VECTOR_ADDR_IRQ0 0x0020
#define INTERRUPT_VECTOR_ADDR_IRQ1 0x0022

#define INTERRUPT_VECTOR_ADDR_RTC_QUARTER_SEC 0x002E
#define INTERRUPT_VECTOR_ADDR_RTC_HALF_SEC 0x0030
#define INTERRUPT_VECTOR_ADDR_RTC_SEC 0x0032
#define INTERRUPT_VECTOR_ADDR_RTC_MIN 0x0034
#define INTERRUPT_VECTOR_ADDR_RTC_HOUR 0x0036
#define INTERRUPT_VECTOR_ADDR_RTC_DAY 0x0038
#define INTERRUPT_VECTOR_ADDR_RTC_WEEK 0x003A
#define INTERRUPT_VECTOR_ADDR_RTC_FREE 0x003C

#define INTERRUPT_VECTOR_ADDR_TIMER_B1 0x0042
#define INTERRUPT_VECTOR_ADDR_TIMER_W 0x0046

union IENR1_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IEN0 : 1;
        uint8_t IEN1 : 1;
        uint8_t IENEC2 : 1;
        uint8_t  : 4;
        uint8_t IENRTC : 1;
    };
};

union IENR2_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IENEC : 1;
        uint8_t  : 1;
        uint8_t IENTB1 : 1;
        uint8_t  : 3;
        uint8_t IENAD : 1;
        uint8_t  : 1;
    };
};


union IRR1_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IRRI0 : 1;
        uint8_t IRRI1 : 1;
        uint8_t IRREC2 : 1;
        uint8_t  : 5;
    };
};

union IRR2_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IRREC : 1;
        uint8_t  : 1;
        uint8_t IRRTB1 : 1;
        uint8_t  : 3;
        uint8_t IRRAD : 1;
        uint8_t  : 1;
    };
};


union RTCFLG_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t SEIFG025 : 1;
        uint8_t SEIFG05 : 1;
        uint8_t SEIFG1 : 1;
        uint8_t MNIFG : 1;
        uint8_t HRIFG : 1;
        uint8_t DYIFG : 1;
        uint8_t WKIFG : 1;
        uint8_t FOIFG : 1;
    };
};

union RTCCR2_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t SEIE025 : 1;
        uint8_t SEIE05 : 1;
        uint8_t SEIE1 : 1;
        uint8_t MNIE : 1;
        uint8_t HRIE : 1;
        uint8_t DYIE : 1;
        uint8_t WKIE : 1;
        uint8_t FOIE : 1;
    };
};

union TIERW_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IMIEA : 1;
        uint8_t IMIEB : 1;
        uint8_t IMIEC : 1;
        uint8_t IMIED : 1;
        uint8_t : 3;
        uint8_t OVIE : 1;
    };
};

union TSRW_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t IMFA : 1;
        uint8_t IMFB : 1;
        uint8_t IMFC : 1;
        uint8_t IMFD : 1;
        uint8_t : 3;
        uint8_t OVF : 1;
    };
};


class Interrupts
{
public:
    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(const std::shared_ptr<CPU>& cpu);

    IENR1_t IENR1 = {};
    IENR2_t IENR2 = {};
    IRR1_t IRR1 = {};
    IRR2_t IRR2 = {};

    RTCFLG_t RTCFLG = {};
    RTCCR2_t RTCCR2 = {};

    TIERW_t TIERW = {};
    TSRW_t TSRW = {};
};
