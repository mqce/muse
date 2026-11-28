#pragma once

#include <stdint.h>

enum class MuseIdleState : uint8_t {
    Normal,
    Happy,
    Grumpy,
    Drowsy,
    Sleeping,
    NetworkError,
};

struct MuseParams {
    int8_t happiness;
    int8_t sleepiness;
    int8_t boredom;
};

class MuseMoodMachine {
public:
    void begin(uint32_t nowMs);
    // conversationActive: LISTENING / THINKING / SPEAKING
    // linkFatal: Wi-Fi / Link / hatch の致命状態が継続中
    void update(uint32_t nowMs, bool conversationActive, bool linkFatal);

    void onHeadPet(uint32_t nowMs);
    void onFaceTouch(uint32_t nowMs);
    // SPEAKING 終了後に呼ぶ
    void onConversationEnd(uint32_t nowMs);

    MuseIdleState state() const { return state_; }
    MuseParams params() const { return params_; }

    bool consumeEnteredHappy();
    bool consumeWokeUp();
    // 通常かつたいくつ ≥80 が1分続いたら true（呼び出し側で再生し、間隔は機械側で更新）
    bool takeCallCue(uint32_t nowMs);
    bool selfPlayActive() const;

private:
    void clampParams();
    void applyAwakeTick();
    void applySleepTick();
    void enter(MuseIdleState next);
    void reevaluate(uint32_t nowMs, bool linkFatal);
    void advanceIdle(uint32_t nowMs, bool linkFatal);
    void noteInteraction(uint32_t nowMs);

    static constexpr int8_t kHappinessMin = 0;
    static constexpr int8_t kHappinessMax = 10;
    static constexpr int8_t kMeterMin = 0;
    static constexpr int8_t kMeterMax = 100;

    static constexpr uint32_t kTickMs = 60 * 1000;
    static constexpr uint32_t kIdle10MinMs = 10 * 60 * 1000;
    static constexpr uint32_t kGrumpyHoldMs = 30 * 60 * 1000;
    static constexpr uint32_t kCallSustainMs = 60 * 1000;
    static constexpr uint32_t kCallRepeatMs = 10 * 60 * 1000;
    static constexpr uint32_t kLinkHoldMs = 5 * 1000;

    MuseIdleState state_ = MuseIdleState::Normal;
    MuseParams params_{5, 30, 20};

    uint32_t lastTickAt_ = 0;
    uint32_t lastInteractionAt_ = 0;
    uint32_t nextIdle10At_ = 0;
    uint32_t happinessZeroSince_ = 0;
    bool happinessZeroTracking_ = false;

    uint32_t boredomHighSince_ = 0;
    bool boredomHighTracking_ = false;
    uint32_t nextCallAt_ = 0;

    uint32_t linkFatalSince_ = 0;
    bool linkFatalLatched_ = false;
    bool linkDownWhileSleeping_ = false;

    bool pendingHappy_ = false;
    bool pendingWake_ = false;
};
