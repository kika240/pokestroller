#include "sci3.h"

#include "core/soc/defines.h"
#include "core/utils/logger.h"

static std::array<uint32_t, 4> clock_rates = {
    PHI_CLK,
    PHI_CLK / PHI_W_CLK * 1,
    PHI_CLK / PHI_W_CLK * 16,
    PHI_CLK / PHI_W_CLK * 64

};

void SCI3::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(SCI3_ADDR_SMR, SMR);
    IO_HANDLER_WRITE_UNION(SCI3_ADDR_SMR, SMR);

    IO_HANDLER_READ_UNION(SCI3_ADDR_SCR, SCR);
    IO_HANDLER_WRITE_UNION(SCI3_ADDR_SCR, SCR);

    IO_HANDLER_READ_UNION(SCI3_ADDR_IRCR, IRCR);
    io->RegisterWriteHandler(SCI3_ADDR_IRCR, [this](uint8_t value) {
        IRCR.VALUE = value;
        if (!IRCR.IRE) ClearReceiveIR();
    });

    IO_HANDLER_READ_VALUE(SCI3_ADDR_BRR, BRR);
    IO_HANDLER_WRITE_VALUE(SCI3_ADDR_BRR, BRR);

    io->RegisterReadHandler(SCI3_ADDR_SSR, [this]()
    {
        return SSR.VALUE;
    });

    io->RegisterWriteHandler(SCI3_ADDR_SSR, [this](const uint8_t value)
    {
        if (!SCR.TE)
            SSR.TDRE = true;
    });

    io->RegisterReadHandler(SCI3_ADDR_TDR, [this]
    {
        return TDR;
    });

    io->RegisterWriteHandler(SCI3_ADDR_TDR, [this](const uint8_t value)
    {
        TDR = value;
        SSR.TDRE = false;
        SSR.TEND = false;
    });

    io->RegisterReadHandler(SCI3_ADDR_RDR, [this]
    {
        SSR.RDRF = false;
        return RDR;
    });

    io->RegisterWriteHandler(SCI3_ADDR_RDR, [this](const uint8_t value)
    {
        // read only
    });
}

void SCI3::Cycle(uint8_t cycles)
{
    // Asynchronous 8N1: ten bits, phi/(32*(BRR+1)*4^CKS) baud.
    const uint32_t clock_rate = 10u * 32u * (uint32_t(BRR) + 1u) * (1u << (2 * SMR.CKS));

    sci3_cycles += cycles;
    if (sci3_cycles >= clock_rate)
    {
        sci3_cycles -= clock_rate;

        if (SCR.TE)
        {
            if (!SSR.TDRE)
            {
                if (IRCR.IRE)
                {
                    on_transmit_ir(TDR);
                    SSR.TDRE = true;
                    SSR.TEND = true;
                }
                else
                {
                    Log::Fatal("Unimplemented SCI3 TXD Transmit");
                }
            }
            else
            {
                SSR.TEND = true;
            }
        }

        if (SCR.RE)
        {
            if (!SSR.RDRF)
            {
                std::lock_guard lock(receive_mutex);
                if (!receive_queue.empty())
                {
                    if (IRCR.IRE)
                    {
                        RDR = receive_queue.front();
                        receive_queue.pop();
                        SSR.RDRF = true;
                    }
                    else
                    {
                        Log::Fatal("Unimplemented SCI3 RXD Receive");
                    }
                }
            }
        }
    }
}

void SCI3::OnTransmitIR(const EventHandlerCallback<uint8_t>& callback)
{
    on_transmit_ir += callback;
}

void SCI3::ReceiveIR(uint8_t data)
{
    if (!IRCR.IRE) return; // Discard data when the IR peripheral is disabled.
    std::lock_guard lock(receive_mutex);
    if (receive_queue.size() >= 65536) Log::Fatal("IR receive queue overflow");
    receive_queue.push(data);
}

void SCI3::ClearReceiveIR() {
    std::lock_guard lock(receive_mutex);
    receive_queue = {};
    SSR.RDRF = false;
}
