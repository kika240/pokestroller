#pragma once
#include <cstdint>

struct AccelSample
{
    int8_t x, y, z;
};

class SampleProvider
{
public:
    virtual ~SampleProvider() = default;
    virtual AccelSample GetSample() = 0;

    bool is_enabled = false;
};
