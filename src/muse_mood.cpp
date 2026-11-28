#include "muse_mood.h"

#include <algorithm>

namespace {

int8_t clampi(int v, int lo, int hi)
{
    return static_cast<int8_t>(std::max(lo, std::min(hi, v)));
}

}  // namespace

void MuseMoodMachine::begin(uint32_t nowMs)
{
    state_ = MuseIdleState::Normal;
    params_ = {5, 30, 20};
    lastTickAt_ = nowMs;
    lastInteractionAt_ = nowMs;
    nextIdle10At_ = nowMs + kIdle10MinMs;
    happinessZeroSince_ = 0;
    happinessZeroTracking_ = false;
    boredomHighSince_ = 0;
    boredomHighTracking_ = false;
    nextCallAt_ = 0;
    linkFatalSince_ = 0;
    linkFatalLatched_ = false;
    linkDownWhileSleeping_ = false;
    pendingHappy_ = false;
    pendingWake_ = false;
}

void MuseMoodMachine::clampParams()
{
    params_.happiness = clampi(params_.happiness, kHappinessMin, kHappinessMax);
    params_.sleepiness = clampi(params_.sleepiness, kMeterMin, kMeterMax);
    params_.boredom = clampi(params_.boredom, kMeterMin, kMeterMax);
}

void MuseMoodMachine::noteInteraction(uint32_t nowMs)
{
    lastInteractionAt_ = nowMs;
    nextIdle10At_ = nowMs + kIdle10MinMs;
}

void MuseMoodMachine::enter(MuseIdleState next)
{
    if (state_ == next) {
        return;
    }
    const MuseIdleState prev = state_;
    state_ = next;
    if (next == MuseIdleState::Happy && prev != MuseIdleState::Happy) {
        pendingHappy_ = true;
    }
    if (prev == MuseIdleState::Sleeping && next == MuseIdleState::Normal) {
        pendingWake_ = true;
    }
}

void MuseMoodMachine::applyAwakeTick()
{
    params_.sleepiness = clampi(params_.sleepiness + 6, kMeterMin, kMeterMax);
    params_.boredom = clampi(params_.boredom + 12, kMeterMin, kMeterMax);
}

void MuseMoodMachine::applySleepTick()
{
    params_.sleepiness = clampi(params_.sleepiness - 15, kMeterMin, kMeterMax);
    params_.boredom = clampi(params_.boredom - 15, kMeterMin, kMeterMax);
}

void MuseMoodMachine::reevaluate(uint32_t nowMs, bool linkFatal)
{
    if (params_.sleepiness >= 90) {
        enter(MuseIdleState::Sleeping);
        return;
    }
    if (params_.sleepiness >= 70) {
        enter(MuseIdleState::Drowsy);
        return;
    }
    if (linkFatal || linkDownWhileSleeping_) {
        if (linkFatal) {
            linkDownWhileSleeping_ = false;
            enter(MuseIdleState::NetworkError);
            return;
        }
    }
    if (params_.happiness == 0) {
        if (!happinessZeroTracking_) {
            happinessZeroTracking_ = true;
            happinessZeroSince_ = nowMs;
        }
        if (static_cast<int32_t>(nowMs - happinessZeroSince_) >= static_cast<int32_t>(kGrumpyHoldMs)) {
            enter(MuseIdleState::Grumpy);
            return;
        }
    } else {
        happinessZeroTracking_ = false;
    }
    if (params_.happiness >= 8) {
        enter(MuseIdleState::Happy);
        return;
    }
    enter(MuseIdleState::Normal);
}

void MuseMoodMachine::advanceIdle(uint32_t nowMs, bool linkFatal)
{
    if (state_ == MuseIdleState::NetworkError) {
        if (!linkFatal) {
            linkFatalLatched_ = false;
            linkFatalSince_ = 0;
            reevaluate(nowMs, false);
        }
        return;
    }

    if (state_ == MuseIdleState::Sleeping) {
        if (linkFatal) {
            linkDownWhileSleeping_ = true;
        }
        if (params_.sleepiness <= 20) {
            if (linkFatal || linkDownWhileSleeping_) {
                linkDownWhileSleeping_ = false;
                enter(MuseIdleState::NetworkError);
            } else {
                enter(MuseIdleState::Normal);
            }
        }
        return;
    }

    if (linkFatalLatched_ && state_ != MuseIdleState::Sleeping) {
        enter(MuseIdleState::NetworkError);
        return;
    }

    // Sleepiness first.
    if (state_ == MuseIdleState::Drowsy) {
        if (params_.sleepiness >= 90) {
            enter(MuseIdleState::Sleeping);
            return;
        }
        if (params_.sleepiness == 0) {
            reevaluate(nowMs, linkFatal);
            return;
        }
        return;
    }

    if (params_.sleepiness >= 70 &&
        (state_ == MuseIdleState::Normal || state_ == MuseIdleState::Happy || state_ == MuseIdleState::Grumpy)) {
        enter(MuseIdleState::Drowsy);
        return;
    }

    if (state_ == MuseIdleState::Happy && params_.happiness <= 5) {
        enter(MuseIdleState::Normal);
        // may fall through to happy again via chain below
    }

    if (state_ == MuseIdleState::Grumpy && params_.happiness >= 3) {
        enter(MuseIdleState::Normal);
    }

    if (state_ == MuseIdleState::Normal) {
        if (params_.happiness == 0) {
            if (!happinessZeroTracking_) {
                happinessZeroTracking_ = true;
                happinessZeroSince_ = nowMs;
            } else if (static_cast<int32_t>(nowMs - happinessZeroSince_) >= static_cast<int32_t>(kGrumpyHoldMs)) {
                enter(MuseIdleState::Grumpy);
                return;
            }
        } else {
            happinessZeroTracking_ = false;
        }
        if (params_.happiness >= 8) {
            enter(MuseIdleState::Happy);
        }
    }

    // After leaving Grumpy/Happy, chain into Happy if needed.
    if (state_ == MuseIdleState::Normal && params_.happiness >= 8) {
        enter(MuseIdleState::Happy);
    }
    if (state_ != MuseIdleState::Drowsy && state_ != MuseIdleState::Sleeping && params_.sleepiness >= 70) {
        enter(MuseIdleState::Drowsy);
    }
    if (state_ == MuseIdleState::Drowsy && params_.sleepiness >= 90) {
        enter(MuseIdleState::Sleeping);
    }
}

void MuseMoodMachine::update(uint32_t nowMs, bool conversationActive, bool linkFatal)
{
    if (conversationActive) {
        return;
    }

    if (linkFatal) {
        if (linkFatalSince_ == 0) {
            linkFatalSince_ = nowMs;
        } else if (!linkFatalLatched_ &&
                   static_cast<int32_t>(nowMs - linkFatalSince_) >= static_cast<int32_t>(kLinkHoldMs)) {
            linkFatalLatched_ = true;
        }
    } else {
        linkFatalSince_ = 0;
        linkFatalLatched_ = false;
    }

    while (static_cast<int32_t>(nowMs - lastTickAt_) >= static_cast<int32_t>(kTickMs)) {
        lastTickAt_ += kTickMs;
        if (state_ == MuseIdleState::Sleeping) {
            applySleepTick();
        } else {
            // NetworkError でも起きている扱いでパラメータは進む（遷移だけ止まる）
            applyAwakeTick();
        }
    }

    while (static_cast<int32_t>(nowMs - nextIdle10At_) >= 0) {
        nextIdle10At_ += kIdle10MinMs;
        params_.happiness = clampi(params_.happiness - 3, kHappinessMin, kHappinessMax);
    }

    // Call sustain tracking.
    if (state_ == MuseIdleState::Normal && params_.boredom >= 80) {
        if (!boredomHighTracking_) {
            boredomHighTracking_ = true;
            boredomHighSince_ = nowMs;
        }
    } else {
        boredomHighTracking_ = false;
        nextCallAt_ = 0;
    }

    advanceIdle(nowMs, linkFatalLatched_);
}

void MuseMoodMachine::onHeadPet(uint32_t nowMs)
{
    noteInteraction(nowMs);
    params_.happiness = clampi(params_.happiness + 1, kHappinessMin, kHappinessMax);
    params_.boredom = clampi(params_.boredom - 20, kMeterMin, kMeterMax);
    if (params_.happiness > 0) {
        happinessZeroTracking_ = false;
    }
    if (state_ != MuseIdleState::NetworkError && state_ != MuseIdleState::Sleeping) {
        advanceIdle(nowMs, linkFatalLatched_);
    }
}

void MuseMoodMachine::onFaceTouch(uint32_t nowMs)
{
    noteInteraction(nowMs);
    params_.boredom = clampi(params_.boredom - 10, kMeterMin, kMeterMax);
    if (state_ == MuseIdleState::Drowsy) {
        params_.sleepiness = clampi(params_.sleepiness - 10, kMeterMin, kMeterMax);
    }
    if (state_ != MuseIdleState::NetworkError) {
        advanceIdle(nowMs, linkFatalLatched_);
    }
}

void MuseMoodMachine::onConversationEnd(uint32_t nowMs)
{
    noteInteraction(nowMs);
    params_.sleepiness = clampi(params_.sleepiness - 40, kMeterMin, kMeterMax);
    params_.boredom = clampi(params_.boredom - 50, kMeterMin, kMeterMax);
    if (params_.happiness > 0) {
        happinessZeroTracking_ = false;
    }
    reevaluate(nowMs, linkFatalLatched_);
}

bool MuseMoodMachine::consumeEnteredHappy()
{
    if (!pendingHappy_) {
        return false;
    }
    pendingHappy_ = false;
    return true;
}

bool MuseMoodMachine::consumeWokeUp()
{
    if (!pendingWake_) {
        return false;
    }
    pendingWake_ = false;
    return true;
}

bool MuseMoodMachine::takeCallCue(uint32_t nowMs)
{
    if (state_ != MuseIdleState::Normal || params_.boredom < 80 || !boredomHighTracking_) {
        return false;
    }
    if (static_cast<int32_t>(nowMs - boredomHighSince_) < static_cast<int32_t>(kCallSustainMs)) {
        return false;
    }
    if (nextCallAt_ != 0 && static_cast<int32_t>(nowMs - nextCallAt_) < 0) {
        return false;
    }
    nextCallAt_ = nowMs + kCallRepeatMs;
    return true;
}

bool MuseMoodMachine::selfPlayActive() const
{
    return params_.boredom >= 70 &&
           (state_ == MuseIdleState::Normal || state_ == MuseIdleState::Happy);
}
