#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MUSE_CLIP_NONE = 0,
    MUSE_CLIP_WAKE_UP,
    MUSE_CLIP_HAPPY,
    MUSE_CLIP_THINKING,
    MUSE_CLIP_CALL,
    MUSE_CLIP_TOUCH,
    MUSE_CLIP_GIGGLE,
    MUSE_CLIP_EHEHE,
} muse_clip_id_t;

// Queue a fixed clip for the voice task. Replaces any pending/playing request.
void muse_clip_play(muse_clip_id_t id);
void muse_clip_cancel(void);

// Called from the voice task while idle / THINKING filler.
bool muse_clip_take(muse_clip_id_t *id);
const int16_t *muse_clip_pcm(muse_clip_id_t id, size_t *frames);

#ifdef __cplusplus
}
#endif
