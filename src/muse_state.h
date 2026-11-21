#pragma once

#include <stdint.h>

enum class MuseState : uint8_t {
    Normal,
    Sleepy,
    Sleeping,
};

class MuseStateMachine {
public:
    void begin(uint32_t now);
    void update(uint32_t now);
    void onTouch(uint32_t now);

    MuseState state() const { return state_; }

private:
    void enter(MuseState next, uint32_t now);

    static constexpr uint32_t kIdleTimeoutMs = 60 * 1000;

    MuseState state_ = MuseState::Normal;
    uint32_t idleSince_ = 0;
};
