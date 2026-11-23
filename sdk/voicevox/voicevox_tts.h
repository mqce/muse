#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VOICEVOX_IDLE = 0,
    VOICEVOX_RUNNING,
    VOICEVOX_DONE,
    VOICEVOX_FAILED,
} voicevox_state_t;

/* Copies the reply and synthesizes it on a worker task. False if there is nothing to say. */
bool voicevox_tts_start(uint32_t gen, const char *text);
/* PCM at 16 kHz mono. Zero when this generation has nothing queued. */
size_t voicevox_tts_read(uint32_t gen, int16_t *pcm, size_t frames);
voicevox_state_t voicevox_tts_state(uint32_t gen);
void voicevox_tts_cancel(void);
