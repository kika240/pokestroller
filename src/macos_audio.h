#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct PWAudio PWAudio;
PWAudio *pw_audio_open(void);
void pw_audio_close(PWAudio *);
void pw_audio_submit(PWAudio *, const int16_t *, size_t);
void pw_audio_active(PWAudio *, bool);
void pw_audio_volume(PWAudio *, float);
