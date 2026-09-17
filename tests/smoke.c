#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "walker.h"

// Optional local test with user-owned dumps. Never writes either input file.
// Usage: smoke ROM EEPROM [quarters=120] [new-output.ppm] [menu|dowsing|radar]
int main(int argc, char **argv) {
    if (argc < 3 || argc > 6) {
        fprintf(stderr, "Usage: %s ROM EEPROM [quarters] [new-output.ppm] [menu|dowsing|radar]\n", argv[0]);
        return 2;
    }
    char error[2048];
    if (!initWalkerFromFiles(argv[1], argv[2], error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    int quarters = argc > 3 ? atoi(argv[3]) : 120;
    if (quarters < 1 || quarters > 14400) return 2;
    uint64_t cycles = 0;
    uint32_t frame[LCD_WIDTH * LCD_HEIGHT];
    uint32_t hash = 2166136261u;
    unsigned changedFrames = 0;
    for (int tick = 0; tick < quarters; tick++) {
        uint64_t target = (uint64_t)(tick + 1) * (SYSTEM_CLOCK_CYCLES_PER_SECOND / 4);
        // Wake up, then exercise menu navigation only when requested.
        if (tick == 4) setKeys(ENTER);
        if (argc > 5) {
            if (tick == 12) setKeys(RIGHT);
            if (strcmp(argv[5], "dowsing") == 0 && tick == 16) setKeys(RIGHT);
            if (strcmp(argv[5], "menu") != 0 && (tick == 20 || tick == 28 || tick == 40)) setKeys(ENTER);
        }
        while (cycles < target) {
            if (runNextInstruction(&cycles)) {
                fprintf(stderr, "Unsupported operation at PC=0x%04x, tick=%d\n", walkerProgramCounter(), tick);
                return 1;
            }
        }
        quarterRTCInterrupt();
        fillVideoBuffer(frame);
        uint32_t nextHash = 2166136261u;
        for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) nextHash = (nextHash ^ frame[i]) * 16777619u;
        if (nextHash != hash) changedFrames++;
        hash = nextHash;
    }
    if (argc > 4) {
        FILE *output = fopen(argv[4], "wbx"); // Refuse to overwrite any existing file.
        if (!output) { perror(argv[4]); return 1; }
        fprintf(output, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
        for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
            unsigned char rgb[] = {frame[i] >> 16, frame[i] >> 8, frame[i]};
            if (fwrite(rgb, 1, 3, output) != 3) { fclose(output); return 1; }
        }
        if (fclose(output) != 0) return 1;
    }
    printf("PASS: %d quarter-seconds, %llu cycles, %u changed frames, PC=0x%04x, frame=%08x\n",
           quarters, (unsigned long long)cycles, changedFrames, walkerProgramCounter(), hash);
    return 0;
}
