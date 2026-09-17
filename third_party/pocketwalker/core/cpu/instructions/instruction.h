#pragma once

#include <functional>
#include <string>
#include <cstdint>

class CPU;

using InstructionExecute = std::function<void(CPU& cpu)>;

struct InstructionCycles
{
    uint8_t instruction_fetch;
    uint8_t branch_address_read;
    uint8_t stack_operation;
    uint8_t byte_data_access;
    uint8_t word_data_access;
    uint8_t internal_operation;

    uint8_t Count() const
    {
        return instruction_fetch + branch_address_read + stack_operation + byte_data_access + word_data_access + internal_operation;
    }
};

struct Instruction
{
    std::string name;
    uint8_t size;
    InstructionCycles cycles;
    InstructionExecute execute;
    bool branch_absolute;
};
