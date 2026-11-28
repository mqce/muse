#pragma once

#include <stdint.h>

#include "face_renderer.h"

class FaceAnimator {
public:
    FaceAnimator(FaceRenderer& renderer);

    void begin(uint32_t now);
    void setBlinkEnabled(bool enabled, uint32_t now);
    void update(uint32_t now);
    bool inBlink() const { return stepIndex_ >= 0; }

private:
    struct Step {
        const FaceLayer* eyes;
        uint32_t durationMs;
    };

    void scheduleNext(uint32_t now);
    void applyStep(uint32_t now);

    FaceRenderer& renderer_;
    bool enabled_ = false;
    int stepIndex_ = -1;
    uint32_t nextChangeAt_ = 0;
};
