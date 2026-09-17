#pragma once

#include <memory>

#include "core/soc/memory/regions/io.h"

#define ADC_ADDR_ADRR 0xFFBC
#define ADC_ADDR_AMR 0xFFBE
#define ADC_ADDR_ADSR 0xFFBF

#define ADC_READ_CONST 0b11'1111'1111

union ADSR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t  : 6;
        uint8_t LADS : 1;
        uint8_t ADSF : 1;
    };
};

union AMR_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t CH : 4;
        uint8_t CKS : 2;
        uint8_t TRGE : 1;
        uint8_t : 1;
    };
};


class ADC
{
public:
    explicit ADC(const std::shared_ptr<MemoryInterface>& memory);

    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(uint8_t cycles);

    ADSR_t ADSR = {};
    AMR_t AMR = {};

    h8300h_ptr<uint16_t> ADRR = nullptr;

private:
    uint32_t adc_cycles = 0;
};
