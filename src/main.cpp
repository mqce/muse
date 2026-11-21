#include <Arduino.h>

#include "display.h"
#include "face_anim.h"
#include "face_renderer.h"
#include "generated/face_assets.h"
#include "muse_state.h"
#include "touch.h"

namespace {

constexpr uint8_t kBrightness = 200;

const Expression kFaces[] = {
    {&face_assets::eyes_default, &face_assets::mouth_default},  // Normal
    {&face_assets::eyes_half, &face_assets::mouth_open1},        // Sleepy
    {&face_assets::eyes_closed, &face_assets::mouth_default},    // Sleeping
};

FaceRenderer renderer(face_assets::bg);
FaceAnimator animator(renderer);
MuseStateMachine stateMachine;
MuseState appliedState = MuseState::Normal;

const char* stateName(MuseState state) {
    switch (state) {
        case MuseState::Normal:
            return "normal";
        case MuseState::Sleepy:
            return "sleepy";
        case MuseState::Sleeping:
            return "sleeping";
    }
    return "?";
}

void applyState(MuseState state, uint32_t now) {
    appliedState = state;
    renderer.setExpression(kFaces[static_cast<int>(state)]);
    animator.setState(state, now);
    Serial.printf("state: %s\n", stateName(state));
}

}  // namespace

static_assert(face_assets::kWidth == display::kWidth && face_assets::kHeight == display::kHeight,
              "face assets must match the display resolution");

void setup() {
    Serial.begin(115200);

    if (!display::begin(kBrightness) || !renderer.begin()) {
        Serial.println("face: init failed");
        while (true) {
            delay(1000);
        }
    }

    if (!touch::begin()) {
        Serial.println("touch: init failed");
        while (true) {
            delay(1000);
        }
    }

    const uint32_t now = millis();
    stateMachine.begin(now);
    applyState(stateMachine.state(), now);
}

void loop() {
    const uint32_t now = millis();

    if (touch::wasPressed()) {
        stateMachine.onTouch(now);
    }

    stateMachine.update(now);

    const MuseState state = stateMachine.state();
    if (state != appliedState) {
        applyState(state, now);
    }

    animator.update(now);
    delay(5);
}
