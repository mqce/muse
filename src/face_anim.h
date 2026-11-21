#pragma once

#include <stdint.h>

#include "face_renderer.h"
#include "muse_state.h"

class FaceAnimator {
public:
    FaceAnimator(FaceRenderer& renderer);

    void begin(uint32_t now);
    void setState(MuseState state, uint32_t now);
    void update(uint32_t now);

private:
    enum class Kind : uint8_t {
        None,
        Blink,
        Doze,
    };

    struct Step {
        const FaceLayer* eyes;
        uint32_t durationMs;
    };

    void configure(Kind kind);
    void scheduleNext(uint32_t now);
    void applyStep(uint32_t now);

    FaceRenderer& renderer_;
    Kind kind_ = Kind::None;
    const Step* steps_ = nullptr;
    int stepCount_ = 0;
    int stepIndex_ = -1;
    uint32_t nextChangeAt_ = 0;
    uint32_t minIntervalMs_ = 0;
    uint32_t maxIntervalMs_ = 0;
};
