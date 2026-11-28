#include "voice_clips.h"

#include <atomic>

#include "generated/voice_clips_data.h"

namespace {

std::atomic<int> s_pending{MUSE_CLIP_NONE};

}  // namespace

extern "C" void muse_clip_play(muse_clip_id_t id)
{
    s_pending.store(static_cast<int>(id), std::memory_order_release);
}

extern "C" void muse_clip_cancel(void)
{
    s_pending.store(MUSE_CLIP_NONE, std::memory_order_release);
}

extern "C" bool muse_clip_take(muse_clip_id_t *id)
{
    if (id == nullptr) {
        return false;
    }
    int v = s_pending.exchange(MUSE_CLIP_NONE, std::memory_order_acq_rel);
    if (v == MUSE_CLIP_NONE) {
        return false;
    }
    *id = static_cast<muse_clip_id_t>(v);
    return true;
}

extern "C" const int16_t *muse_clip_pcm(muse_clip_id_t id, size_t *frames)
{
    if (frames == nullptr) {
        return nullptr;
    }
    switch (id) {
        case MUSE_CLIP_WAKE_UP:
            *frames = voice_clips::wake_up_frames;
            return voice_clips::wake_up;
        case MUSE_CLIP_HAPPY:
            *frames = voice_clips::happy_frames;
            return voice_clips::happy;
        case MUSE_CLIP_THINKING:
            *frames = voice_clips::thinking_frames;
            return voice_clips::thinking;
        case MUSE_CLIP_CALL:
            *frames = voice_clips::call_frames;
            return voice_clips::call;
        case MUSE_CLIP_TOUCH:
            *frames = voice_clips::touch_frames;
            return voice_clips::touch;
        case MUSE_CLIP_GIGGLE:
            *frames = voice_clips::giggle_frames;
            return voice_clips::giggle;
        case MUSE_CLIP_EHEHE:
            *frames = voice_clips::ehehe_frames;
            return voice_clips::ehehe;
        default:
            *frames = 0;
            return nullptr;
    }
}
