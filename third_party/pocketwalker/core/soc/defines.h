#pragma once

#define PHI_CLK 3686400 // 3.6864mHz
#define PHI_W_CLK 32000 // 32kHz

#define IO_HANDLER_READ_UNION(addr, reg) \
    io->RegisterReadHandler(addr, [this]() { return reg.VALUE; })

#define IO_HANDLER_WRITE_UNION(addr, reg) \
    io->RegisterWriteHandler(addr, [this](const uint8_t value) { reg.VALUE = value; })

#define IO_HANDLER_READ_VALUE(addr, reg) \
    io->RegisterReadHandler(addr, [this]() { return reg; })

#define IO_HANDLER_WRITE_VALUE(addr, reg) \
    io->RegisterWriteHandler(addr, [this](const uint8_t value) { reg = value; })
