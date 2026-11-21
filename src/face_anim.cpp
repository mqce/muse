#include "face_anim.h"

#include <Arduino.h>

#include "generated/face_assets.h"

FaceAnimator::FaceAnimator(FaceRenderer& renderer) : renderer_(renderer) {}

void FaceAnimator::begin(uint32_t now) {
    setState(MuseState::Normal, now);
}

void FaceAnimator::setState(MuseState state, uint32_t now) {
    Kind next = Kind::None;
    switch (state) {
        case MuseState::Normal:
            next = Kind::Blink;
            break;
        case MuseState::Sleepy:
            next = Kind::Doze;
            break;
        case MuseState::Sleeping:
            next = Kind::None;
            break;
    }

    if (kind_ == next && stepIndex_ == -1) {
        return;
    }

    if (stepIndex_ >= 0) {
        renderer_.overrideEyes(nullptr);
    }
    configure(next);
    stepIndex_ = -1;
    scheduleNext(now);
}

void FaceAnimator::update(uint32_t now) {
    if (kind_ == Kind::None) {
        return;
    }
    if (static_cast<int32_t>(now - nextChangeAt_) < 0) {
        return;
    }
    ++stepIndex_;
    applyStep(now);
}

void FaceAnimator::configure(Kind kind) {
    static const Step kBlinkSteps[] = {
        {&face_assets::eyes_half, 50},
        {&face_assets::eyes_closed, 80},
        {&face_assets::eyes_half, 50},
        {nullptr, 0},
    };
    static const Step kDozeSteps[] = {
        {&face_assets::eyes_closed, 400},
        {nullptr, 0},
    };

    kind_ = kind;
    switch (kind) {
        case Kind::Blink:
            steps_ = kBlinkSteps;
            stepCount_ = static_cast<int>(sizeof(kBlinkSteps) / sizeof(kBlinkSteps[0]));
            minIntervalMs_ = 2000;
            maxIntervalMs_ = 6000;
            break;
        case Kind::Doze:
            steps_ = kDozeSteps;
            stepCount_ = static_cast<int>(sizeof(kDozeSteps) / sizeof(kDozeSteps[0]));
            minIntervalMs_ = 1500;
            maxIntervalMs_ = 3500;
            break;
        case Kind::None:
            steps_ = nullptr;
            stepCount_ = 0;
            minIntervalMs_ = 0;
            maxIntervalMs_ = 0;
            break;
    }
}

void FaceAnimator::scheduleNext(uint32_t now) {
    if (kind_ == Kind::None) {
        nextChangeAt_ = now;
        return;
    }
    nextChangeAt_ = now + random(minIntervalMs_, maxIntervalMs_ + 1);
}

void FaceAnimator::applyStep(uint32_t now) {
    const Step& step = steps_[stepIndex_];
    renderer_.overrideEyes(step.eyes);

    if (stepIndex_ == stepCount_ - 1) {
        stepIndex_ = -1;
        scheduleNext(now);
    } else {
        nextChangeAt_ = now + step.durationMs;
    }
}
