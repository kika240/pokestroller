#pragma once
#include "table.h"

class InstructionSet
{
public:
    InstructionSet();

    const Instruction* Decode(const CPU& cpu) const;

private:
    InstructionTable root;
};
