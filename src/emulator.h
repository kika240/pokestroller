#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PW_ROM_SIZE 49152
#define PW_EEPROM_SIZE 65536
#define PW_CLOCK 3686400
#define PW_AUDIO_RATE 32000
#define PW_WIDTH 96
#define PW_HEIGHT 64
#define PW_CENTER 1
#define PW_LEFT 4
#define PW_RIGHT 16
typedef struct PWEmulator PWEmulator;
// Input buffers are copied to memory only. No file access in this API.
PWEmulator *pw_create(const uint8_t *rom, size_t rom_size,
    const uint8_t *eeprom, size_t eeprom_size, char *error, size_t capacity);
void pw_destroy(PWEmulator *);
bool pw_run(PWEmulator *, uint32_t cycles);
const char *pw_error(const PWEmulator *);
void pw_button(PWEmulator *, uint8_t mask, bool down);
void pw_walk(PWEmulator *, bool enabled);
void pw_frame(PWEmulator *, uint32_t pixels[PW_WIDTH * PW_HEIGHT]);
void pw_eeprom(PWEmulator *, uint8_t bytes[PW_EEPROM_SIZE]);
size_t pw_audio(PWEmulator *, int16_t *samples, size_t capacity);
size_t pw_ir_transmit(PWEmulator *, uint8_t *bytes, size_t capacity);
bool pw_ir_receive(PWEmulator *, const uint8_t *bytes, size_t count);
void pw_ir_clear(PWEmulator *);
uint16_t pw_pc(const PWEmulator *);
uint32_t pw_steps(PWEmulator *);
uint16_t pw_watts(PWEmulator *);
// Negative uses host local time (default). Otherwise advance Unix origin by
// emulated cycles, allowing reproducible tests without changing the Mac clock.
void pw_clock_origin(PWEmulator *, int64_t unix_seconds);
#ifdef __cplusplus
}
#endif
