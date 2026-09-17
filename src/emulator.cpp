#include "emulator.h"
#include "core/soc/h838606.h"
#include "core/pokewalker/peripherals/bma150/bma150.h"
#include "core/pokewalker/peripherals/bma150/step_sample_provider.h"
#include "core/pokewalker/peripherals/m95512/m95512.h"
#include "core/pokewalker/peripherals/ssd1854/ssd1854.h"
#include "core/pokewalker/peripherals/buzzer/buzzer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>

struct PWEmulator {
    std::shared_ptr<H838606> soc;
    std::shared_ptr<BMA150> accelerometer;
    std::shared_ptr<StepSampleProvider> steps;
    std::shared_ptr<M95512> eeprom;
    std::shared_ptr<SSD1854> lcd;
    std::shared_ptr<Buzzer> buzzer;
    std::deque<int16_t> audio;
    std::deque<uint8_t> ir;
    std::string error;
    uint64_t cycles = 0, target = 0, last_ir_cycle = 0;
    int64_t clock_origin = -1;
    bool known_firmware = false;
    double phase = 0, envelope = 0, frequency = 0;
    PWEmulator(const RomBuffer &rom, const uint8_t *data) {
        uint64_t fingerprint = 14695981039346656037ull;
        for (uint8_t byte : rom) fingerprint = (fingerprint ^ byte) * 1099511628211ull;
        known_firmware = fingerprint == 0x4fb3ade124517802ull;
        soc = std::make_shared<H838606>(rom);
        accelerometer = std::make_shared<BMA150>();
        soc->ssu->RegisterPeripheral(accelerometer, SSU_ADDR_PDR9, 0);
        soc->ssu->RegisterOutputPin(accelerometer, BMA150_PIN_INT, SSU_ADDR_PDRB, 1);
        steps = std::make_shared<StepSampleProvider>(soc->memory);
        accelerometer->SetSampleProvider(steps);
        eeprom = std::make_shared<M95512>();
        std::copy_n(data, PW_EEPROM_SIZE, eeprom->eeprom.begin());
        soc->ssu->RegisterPeripheral(eeprom, SSU_ADDR_PDR1, 2);
        lcd = std::make_shared<SSD1854>();
        soc->ssu->RegisterPeripheral(lcd, SSU_ADDR_PDR1, 0);
        soc->ssu->RegisterInputPin(lcd, SSU_ADDR_PDR1, 1, SSD1854_PIN_DC);
        buzzer = std::make_shared<Buzzer>(soc->timer_w);
        buzzer->OnSamplePushed += [this](BuzzerInformation info) {
            // Band-limited square wave, 3 ms attack/release, bounded queue.
            bool active = info.frequency >= 120 && info.frequency < PW_AUDIO_RATE / 2;
            if (active) frequency = info.frequency;
            envelope = std::clamp(envelope + (active ? 1 : -1) / 96.0, 0.0, 1.0);
            double sample = 0;
            if (envelope > 0 && frequency > 0) {
                for (int h = 1; h * frequency < PW_AUDIO_RATE / 2; h += 2)
                    sample += std::sin(6.283185307179586 * h * phase) / h;
                phase += frequency / PW_AUDIO_RATE;
                phase -= std::floor(phase);
            }
            if (audio.size() == PW_AUDIO_RATE / 2) audio.pop_front();
            audio.push_back(static_cast<int16_t>(std::clamp(sample * envelope *
                (info.is_full_volume ? 12000 : 6000), -32768.0, 32767.0)));
        };
        soc->sci3->OnTransmitIR([this](uint8_t byte) {
            if (ir.size() == 65536) throw std::runtime_error("IR transmit queue overflow");
            ir.push_back(byte);
            last_ir_cycle = cycles;
        });
        soc->rtc->Now = [this] {
            return clock_origin < 0 ? std::time(nullptr) :
                static_cast<time_t>(clock_origin + cycles / PW_CLOCK);
        };
    }
};
PWEmulator *pw_create(const uint8_t *rom, size_t rom_size, const uint8_t *eeprom,
    size_t eeprom_size, char *error, size_t capacity) {
    try {
        if (!rom || !eeprom || (rom_size != PW_ROM_SIZE && rom_size != 65536) || eeprom_size != PW_EEPROM_SIZE)
            throw std::runtime_error("ROM: 48 or 64 KiB; EEPROM: exactly 64 KiB required.");
        RomBuffer bytes;
        std::copy_n(rom, PW_ROM_SIZE, bytes.begin());
        return new PWEmulator(bytes, eeprom);
    } catch (const std::exception &e) {
        if (error && capacity) std::snprintf(error, capacity, "%s", e.what());
        return nullptr;
    }
}
void pw_destroy(PWEmulator *pw) { delete pw; }
bool pw_run(PWEmulator *pw, uint32_t cycles) {
    if (!pw || !pw->error.empty()) return false;
    try {
        pw->target += cycles;
        while (pw->cycles < pw->target) {
            uint8_t elapsed = pw->soc->Cycle();
            pw->cycles += elapsed;
            pw->buzzer->Cycle(elapsed);
        }
        return true;
    } catch (const std::exception &e) { pw->error = e.what(); return false; }
}
const char *pw_error(const PWEmulator *pw) { return pw ? pw->error.c_str() : "No emulator"; }
void pw_button(PWEmulator *pw, uint8_t mask, bool down) {
    if (!pw) return;
    uint8_t value = pw->soc->memory->Read8(SSU_ADDR_PDRB);
    mask &= PW_CENTER | PW_LEFT | PW_RIGHT;
    pw->soc->memory->Write8(SSU_ADDR_PDRB, down ? value | mask : value & ~mask);
}
void pw_walk(PWEmulator *pw, bool enabled) { if (pw) pw->steps->is_enabled = enabled; }
void pw_frame(PWEmulator *pw, uint32_t *pixels) {
    static const uint32_t palette[] = {0xe0e7ce, 0xa5af94, 0x69765d, 0x2b3726};
    auto &info = pw->lcd->draw_info;
    for (int y = 0; y < PW_HEIGHT; y++) for (int x = 0; x < PW_WIDTH; x++) {
        int base = (y / 8 + info.page_offset) * 256 + x * 2;
        unsigned shade = info.power_save_mode ? 0 :
            (((info.vram.Read8(base) >> (y % 8)) & 1) << 1) |
            ((info.vram.Read8(base + 1) >> (y % 8)) & 1);
        pixels[y * PW_WIDTH + x] = palette[shade];
    }
}
void pw_eeprom(PWEmulator *pw, uint8_t *bytes) {
    std::copy(pw->eeprom->eeprom.begin(), pw->eeprom->eeprom.end(), bytes);
    // The known firmware caches HealthData (steps/watts/settings) in RAM and
    // commits it infrequently. Flush this cache into the exported snapshot so
    // autosave does not lose recent earned watts on a restart. No guest mutation.
    // This layout is enabled only for the recognized ROM, after boot completed.
    if (pw->known_firmware && pw->cycles >= PW_CLOCK &&
        std::equal(bytes, bytes + 8, reinterpret_cast<const uint8_t*>("nintendo"))) {
        for (unsigned base : {0x156u, 0x256u}) {
            uint8_t checksum = 1;
            for (unsigned i = 0; i < 24; i++) {
                bytes[base+i] = pw->soc->memory->Read8(0xf780+i);
                checksum += bytes[base+i];
            }
            bytes[base+24] = checksum;
        }
    }
}
template <class T> static size_t drain(std::deque<T> &queue, T *out, size_t capacity) {
    size_t count = std::min(queue.size(), capacity);
    for (size_t i = 0; i < count; i++) { out[i] = queue.front(); queue.pop_front(); }
    return count;
}
size_t pw_audio(PWEmulator *pw, int16_t *out, size_t n) { return drain(pw->audio, out, n); }
size_t pw_ir_transmit(PWEmulator *pw, uint8_t *out, size_t n) {
    // Publish a complete UART burst after 250 us of emulated idle time. The ROM
    // uses a 125 us gap to detect packet ends; forwarding individual host-frame
    // fragments would manufacture gaps inside its packets.
    if (pw->cycles - pw->last_ir_cycle < PW_CLOCK / 4000) return 0;
    return drain(pw->ir, out, n);
}
bool pw_ir_receive(PWEmulator *pw, const uint8_t *bytes, size_t n) {
    try { for (size_t i = 0; i < n; i++) pw->soc->sci3->ReceiveIR(bytes[i]); return true; }
    catch (const std::exception &e) { pw->error = e.what(); return false; }
}
void pw_ir_clear(PWEmulator *pw) { pw->ir.clear(); pw->soc->sci3->ClearReceiveIR(); }
uint16_t pw_pc(const PWEmulator *pw) { return pw->soc->cpu->reg.PC; }
uint32_t pw_steps(PWEmulator *pw) { return pw->soc->memory->Read32(0xf79c); }
uint16_t pw_watts(PWEmulator *pw) { return pw->soc->memory->Read16(0xf78e); }
void pw_clock_origin(PWEmulator *pw, int64_t origin) { pw->clock_origin = origin; }
