#include "adc.h"

#include "core/soc/defines.h"

ADC::ADC(const std::shared_ptr<MemoryInterface>& memory)
{
    ADRR = memory->Ptr16(ADC_ADDR_ADRR);
}

void ADC::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(ADC_ADDR_ADSR, ADSR);
    IO_HANDLER_WRITE_UNION(ADC_ADDR_ADSR, ADSR);

    IO_HANDLER_READ_UNION(ADC_ADDR_AMR, AMR);
    IO_HANDLER_WRITE_UNION(ADC_ADDR_AMR, AMR);
}

void ADC::Cycle(uint8_t cycles)
{
    // for emulation purposes, just convert instantly
    if (ADSR.ADSF)
    {
        *ADRR = ADC_READ_CONST << 6; // upper 10 bits is the result
        ADSR.ADSF = false;
    }
}
