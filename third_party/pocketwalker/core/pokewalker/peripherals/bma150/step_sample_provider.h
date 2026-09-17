#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include "sample_provider.h"
#include "core/memory/interface.h"

#define PW_ADDR_ACCEL_SAMPLE_COUNT 0xF7AE
#define PW_ADDR_IS_NOT_WALKING  0xF8EF

static constexpr std::array<int8_t, 8> STEP_SINE_LUT = {
    0, 18, 26, 18, 0, -18, -26, -18
};

class StepSampleProvider : public SampleProvider
{
public:
    explicit StepSampleProvider(const std::shared_ptr<MemoryInterface>& memory)
        : mem(memory) {}

    AccelSample GetSample() override
    {
        const uint8_t sample_count = mem->Read8(PW_ADDR_ACCEL_SAMPLE_COUNT);

        if (sample_count == 0)
            mem->Write8(PW_ADDR_IS_NOT_WALKING, 0);

        const int8_t s = STEP_SINE_LUT[sample_count & 7];
        return {s, static_cast<int8_t>(s >> 1), 0};
    }

private:
    std::shared_ptr<MemoryInterface> mem;
};
