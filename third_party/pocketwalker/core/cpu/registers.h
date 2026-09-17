#pragma once
#include <array>
#include <cstdint>

#define REG_SP_INDEX 7

#define NEGATIVE_MASK(bits) (uint32_t{1} << (bits - 1))

typedef union flags
{
    uint8_t CCR = 0;
    struct
    {
        bool C : 1;
        bool V : 1;
        bool Z : 1;
        bool N : 1;
        bool U : 1;
        bool H : 1;
        bool UI : 1;
        bool I : 1;
    };
} flags_t;

class Registers
{
public:
    Registers();

    uint8_t* Reg8(uint8_t select);
    uint16_t* Reg16(uint8_t select);
    uint32_t* Reg32(uint8_t select);

    template <typename T>
    void MovFlags(T value, const size_t bits = sizeof(T) * 8)
    {
        const uint32_t negative_mask = NEGATIVE_MASK(bits);

        flags.N = value & negative_mask;
        flags.Z = value == 0;
        flags.V = false;
    }

    template <typename T>
    T Add(T rd, T rs, const size_t bits = sizeof(T) * 8)
    {
        const uint64_t mask = (uint64_t{1} << bits) - 1;
        const uint64_t sum = uint64_t(rd) + uint64_t(rs);
        const T result = static_cast<T>(sum & mask);
        const uint32_t sign = NEGATIVE_MASK(bits);
        flags.Z = result == 0;
        flags.N = (result & sign) != 0;
        flags.V = (~(rd ^ rs) & (rd ^ result) & sign) != 0;
        flags.C = sum > mask;
        flags.H = ((rd ^ rs ^ result) & (uint32_t{1} << (bits - 4))) != 0;
        return result;
    }

    template <typename T>
    T Sub(T rd, T rs, const size_t bits = sizeof(T) * 8)
    {
        const T result = static_cast<T>(rd - rs);
        const uint32_t sign = NEGATIVE_MASK(bits);
        flags.Z = result == 0;
        flags.N = (result & sign) != 0;
        flags.V = ((rd ^ rs) & (rd ^ result) & sign) != 0;
        flags.C = rs > rd;
        flags.H = ((rd ^ rs ^ result) & (uint32_t{1} << (bits - 4))) != 0;
        return result;
    }

    template <typename T>
    T Inc(T value, size_t inc, const size_t bits = sizeof(T) * 8)
    {
        const T result = static_cast<T>(value + inc);
        const uint32_t sign = NEGATIVE_MASK(bits);
        flags.N = (result & sign) != 0;
        flags.Z = result == 0;
        flags.V = (~(value ^ inc) & (value ^ result) & sign) != 0;
        return result;
    }

    template <typename T>
    T Dec(T value, size_t dec, const size_t bits = sizeof(T) * 8)
    {
        const T result = static_cast<T>(value - dec);
        const uint32_t sign = NEGATIVE_MASK(bits);
        flags.N = (result & sign) != 0;
        flags.Z = result == 0;
        flags.V = ((value ^ dec) & (value ^ result) & sign) != 0;
        return result;
    }


    uint16_t PC;
    uint32_t* SP;
    flags_t flags;

private:
    std::array<uint32_t, 8> registers = {};
};
