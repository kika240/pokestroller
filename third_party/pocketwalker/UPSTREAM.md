# PocketWalker core attribution

Source: https://github.com/h4lfheart/pocketwalker
Revision: `2f3b4512a668e3b7c321f213c1c8d5344627e96e`
License: GPL-3.0 (see LICENSE), h4lfheart and PocketWalker contributors.
Imported on 2026-09-17. Only `core/` source code is included, without ROM,
EEPROM, desktop assets or Qt. PokeStroller's macOS adapter is in `src/emulator.cpp`.
The original Windows frontend continues to use the historical C core.

Local changes to upstream code:

- Replace C++ print/format and process exits with portable exceptions caught by
  the native adapter; use memcpy for potentially unaligned big-endian registers.
- Correct arithmetic CCR flags and 32-bit integer overflows; mask register bit
  indices. Regression coverage is in `tests/emulator_test.cpp`.
- Inject an RTC clock source for tests, set PM, avoid false midnight interrupts
  on first initialization, and produce exactly 32,000 buzzer samples per second.
- Bound SCI3 input queues, clear disabled IR state, correct 8N1 byte timing and
  transmit-complete flag. The adapter groups complete UART bursts before TCP
  transmission, preserving the firmware's inter-byte timeout.
- The adapter synchronizes cached HealthData into exported EEPROM snapshots for
  the recognized firmware. Its layout and checksum initialization were checked
  against the user-owned ROM and Dmitry Grinberg's annotated disassembly:
  https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker
  No disassembly or device dump is redistributed here.

H8 arithmetic reference:
https://www.renesas.com/en/document/mah/h8300h-series-software-manual
