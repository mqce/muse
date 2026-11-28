#include "face_anim.h"

#include "esp_random.h"
#include "generated/face_assets.h"

static long randomRange(long min, long max)
{
    if (max <= min) {
        return min;
    }
    return min + static_cast<long>(esp_random() % static_cast<uint32_t>(max - min));
}

FaceAnimator::FaceAnimator(FaceRenderer& renderer) : renderer_(renderer) {}

void FaceAnimator::begin(uint32_t now)
{
    setBlinkEnabled(true, now);
}

void FaceAnimator::setBlinkEnabled(bool enabled, uint32_t now)
{
    if (enabled_ == enabled && stepIndex_ == -1) {
        return;
    }
    if (stepIndex_ >= 0) {
        renderer_.overrideEyes(nullptr);
    }
    enabled_ = enabled;
    stepIndex_ = -1;
    if (enabled_) {
        scheduleNext(now);
    } else {
        nextChangeAt_ = now;
    }
}

void FaceAnimator::update(uint32_t now)
{
    if (!enabled_) {
        return;
    }
    if (static_cast<int32_t>(now - nextChangeAt_) < 0) {
        return;
    }
    ++stepIndex_;
    applyStep(now);
}

void FaceAnimator::scheduleNext(uint32_t now)
{
    nextChangeAt_ = now + static_cast<uint32_t>(randomRange(2000, 6001));
}

void FaceAnimator::applyStep(uint32_t now)
{
    static const Step kBlinkSteps[] = {
        {&face_assets::eyes_e2_half, 50},
        {&face_assets::eyes_e3_closed, 80},
        {&face_assets::eyes_e2_half, 50},
        {nullptr, 0},
    };
    constexpr int kCount = static_cast<int>(sizeof(kBlinkSteps) / sizeof(kBlinkSteps[0]));

    if (stepIndex_ < 0 || stepIndex_ >= kCount) {
        stepIndex_ = -1;
        scheduleNext(now);
        return;
    }

    const Step& step = kBlinkSteps[stepIndex_];
    renderer_.overrideEyes(step.eyes);

    if (stepIndex_ == kCount - 1) {
        stepIndex_ = -1;
        scheduleNext(now);
    } else {
        nextChangeAt_ = now + step.durationMs;
    }
}
