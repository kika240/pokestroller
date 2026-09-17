#pragma once
#include <memory>

#include "../memory/interface.h"
#include "registers.h"

#define CPU_VECTOR_RESET_ADDR 0x0000

class CPU
{
public:
    explicit CPU(const std::shared_ptr<MemoryInterface>& memory);

    uint8_t Cycle();

    void Interrupt(uint16_t vector_address);

    void Push16(uint16_t value) const;
    uint16_t Pop16() const;

    Registers reg = {};
    bool sleep = false;

    std::shared_ptr<MemoryInterface> mem;

public:
    uint8_t aH() const { return mem->Read8(reg.PC) >> 4; }
    uint8_t aL() const { return mem->Read8(reg.PC) & 0xF; }
    uint8_t bH() const { return mem->Read8(reg.PC + 1) >> 4; }
    uint8_t bL() const { return mem->Read8(reg.PC + 1) & 0xF; }
    uint8_t cH() const { return mem->Read8(reg.PC + 2) >> 4; }
    uint8_t cL() const { return mem->Read8(reg.PC + 2) & 0xF; }
    uint8_t dH() const { return mem->Read8(reg.PC + 3) >> 4; }
    uint8_t dL() const { return mem->Read8(reg.PC + 3) & 0xF; }
    uint8_t eH() const { return mem->Read8(reg.PC + 4) >> 4; }
    uint8_t eL() const { return mem->Read8(reg.PC + 4) & 0xF; }
    uint8_t fH() const { return mem->Read8(reg.PC + 5) >> 4; }
    uint8_t fL() const { return mem->Read8(reg.PC + 5) & 0xF; }
    uint8_t gH() const { return mem->Read8(reg.PC + 6) >> 4; }
    uint8_t gL() const { return mem->Read8(reg.PC + 6) & 0xF; }
    uint8_t hH() const { return mem->Read8(reg.PC + 7) >> 4; }
    uint8_t hL() const { return mem->Read8(reg.PC + 7) & 0xF; }

    uint8_t a() const { return mem->Read8(reg.PC); }
    uint8_t b() const { return mem->Read8(reg.PC + 1); }
    uint8_t c() const { return mem->Read8(reg.PC + 2); }
    uint8_t d() const { return mem->Read8(reg.PC + 3); }
    uint8_t e() const { return mem->Read8(reg.PC + 4); }
    uint8_t f() const { return mem->Read8(reg.PC + 5); }
    uint8_t g() const { return mem->Read8(reg.PC + 6); }
    uint8_t h() const { return mem->Read8(reg.PC + 7); }

    uint16_t ab() const { return mem->Read16(reg.PC); }
    uint16_t cd() const { return mem->Read16(reg.PC + 2); }
    uint16_t ef() const { return mem->Read16(reg.PC + 4); }
    uint16_t gh() const { return mem->Read16(reg.PC + 6); }
};
