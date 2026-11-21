#include "muse_state.h"

void MuseStateMachine::begin(uint32_t now) {
    enter(MuseState::Normal, now);
}

void MuseStateMachine::update(uint32_t now) {
    if (static_cast<int32_t>(now - idleSince_) < static_cast<int32_t>(kIdleTimeoutMs)) {
        return;
    }

    switch (state_) {
        case MuseState::Normal:
            enter(MuseState::Sleepy, now);
            break;
        case MuseState::Sleepy:
            enter(MuseState::Sleeping, now);
            break;
        case MuseState::Sleeping:
            enter(MuseState::Sleepy, now);
            break;
    }
}

void MuseStateMachine::onTouch(uint32_t now) {
    if (state_ == MuseState::Normal) {
        idleSince_ = now;
        return;
    }
    enter(MuseState::Normal, now);
}

void MuseStateMachine::enter(MuseState next, uint32_t now) {
    state_ = next;
    idleSince_ = now;
}
