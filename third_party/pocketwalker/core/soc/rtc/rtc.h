#pragma once
#include <ctime>
#include <functional>

#include "core/soc/interrupts/interrupts.h"

#define RTC_ADDR_RSECDR 0xF068
#define RTC_ADDR_RMINDR 0xF069
#define RTC_ADDR_RHRDR 0xF06A
#define RTC_ADDR_RWKDR 0xF06B
#define RTC_ADDR_RTCCR1 0xF06C

union RTCCR1_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t : 3;
        uint8_t INT : 1;
        uint8_t RST : 1;
        uint8_t PM : 1;
        uint8_t HR24 : 1;
        uint8_t RUN : 1;
    };
};

class RTC
{
public:
    explicit RTC(const std::shared_ptr<Interrupts>& interrupts);

    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(uint8_t cycles);

    // Injectable time source for deterministic tests; production uses host local time.
    std::function<time_t()> Now = [] { return std::time(nullptr); };

    RTCCR1_t RTCCR1 = {};
    uint8_t RSECDR = 0;
    uint8_t RMINDR = 0;
    uint8_t RHRDR = 0;
    uint8_t RWKDR = 0;

private:
    std::shared_ptr<Interrupts> interrupts;

    uint32_t rtc_cycles = 0;
    uint8_t quarters = 0;
    bool initialized = false;
    std::tm last_time = {};
};
