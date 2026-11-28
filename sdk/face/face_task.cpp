extern "C" {
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "muse_board.h"
#include "muse_chat.h"
#include "muse_link.h"
#include "muse_settings.h"
#include "muse_state.h"
#include "muse_wifi.h"
}

#include "display.h"
#include "face_anim.h"
#include "face_renderer.h"
#include "generated/face_assets.h"
#include "muse_mood.h"
#include "voice_clips.h"

namespace {

constexpr char kTag[] = "muse_face";
constexpr uint8_t kBrightness = 200;
constexpr uint32_t kMouthStepMs = 150;
constexpr uint32_t kPetOverlayMs = 1200;
constexpr uint32_t kShortTouchMs = 500;
constexpr uint32_t kPetHoldMs = 500;
constexpr uint32_t kTapWindowMs = 1500;
constexpr int kTapPetCount = 3;
constexpr uint32_t kThinkStepMs = 700;
constexpr uint32_t kListenBlinkMinMs = 2000;
constexpr uint32_t kListenBlinkMaxMs = 4000;
constexpr uint32_t kListenOpenMs = 180;
constexpr uint32_t kLookMinMs = 2500;
constexpr uint32_t kLookMaxMs = 5000;
constexpr uint32_t kLookHoldMs = 450;

FaceRenderer renderer(face_assets::bg);
FaceAnimator animator(renderer);
MuseMoodMachine stateMachine;

MuseIdleState applied = MuseIdleState::Normal;
int appliedEyeBand = -1;
muse_mode_t appliedMode = MUSE_MODE_IDLE;
bool inSdkFace = false;
muse_clip_id_t touchClip = MUSE_CLIP_NONE;
bool touchClipSet = false;

bool petOverlay = false;
uint32_t petUntil = 0;

bool mouthOpen = false;
uint32_t mouthAt = 0;

int thinkIndex = 0;
uint32_t thinkAt = 0;

bool listenOpen = false;
uint32_t listenAt = 0;

bool looking = false;
int lookIndex = 0;
uint32_t lookAt = 0;

bool touchDown = false;
uint32_t touchDownAt = 0;
bool petFired = false;
uint32_t tapTimes[3] = {};
int tapCount = 0;

muse_mode_t prevMode = MUSE_MODE_BOOT;

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint32_t randRange(uint32_t lo, uint32_t hiInclusive)
{
    if (hiInclusive <= lo) {
        return lo;
    }
    return lo + (esp_random() % (hiInclusive - lo + 1));
}

const char* idleName(MuseIdleState state)
{
    switch (state) {
        case MuseIdleState::Normal:
            return "normal";
        case MuseIdleState::Happy:
            return "happy";
        case MuseIdleState::Grumpy:
            return "grumpy";
        case MuseIdleState::Drowsy:
            return "drowsy";
        case MuseIdleState::Sleeping:
            return "sleeping";
        case MuseIdleState::NetworkError:
            return "network_error";
    }
    return "?";
}

bool conversationMode(muse_mode_t mode)
{
    return mode == MUSE_MODE_LISTENING || mode == MUSE_MODE_THINKING || mode == MUSE_MODE_SPEAKING;
}

// ねむいの目は 70–76 が E1（通常と同じ見た目）。帯が変わったときだけ描き直す。
int drowsyEyeBand(int sleepiness)
{
    if (sleepiness >= 84) {
        return 2;
    }
    if (sleepiness >= 77) {
        return 1;
    }
    return 0;
}

bool wantsBlink(MuseIdleState state, int sleepiness)
{
    if (state == MuseIdleState::Normal) {
        return true;
    }
    // E1 のままねむいに入ると、通常顔のまま瞬きが消えて見える
    return state == MuseIdleState::Drowsy && drowsyEyeBand(sleepiness) == 0;
}

void queueTouchClip(muse_clip_id_t id)
{
    touchClip = id;
    touchClipSet = true;
}

void queuePetVoice()
{
    const uint32_t roll = esp_random() % 100;
    if (roll < 50) {
        queueTouchClip(MUSE_CLIP_NONE);
    } else if (roll < 70) {
        queueTouchClip(MUSE_CLIP_GIGGLE);
    } else if (roll < 90) {
        queueTouchClip(MUSE_CLIP_EHEHE);
    } else {
        queueTouchClip(MUSE_CLIP_HAPPY);
    }
}

bool linkFatalNow()
{
    const muse_link_state_t link = muse_link_state();
    if (link == MUSE_LINK_OFFLINE || link == MUSE_LINK_ERROR) {
        return true;
    }

    muse_wifi_status_t wifi{};
    muse_wifi_status(&wifi);
    if (wifi.state == MUSE_WIFI_FAILED || wifi.state == MUSE_WIFI_NO_NETWORK) {
        return true;
    }

    muse_hatch_status_t hatch{};
    muse_hatch_status(&hatch);
    if (hatch.state == MUSE_HATCH_UNREACHABLE) {
        return true;
    }
    return false;
}

Expression idleExpression(MuseIdleState state, int sleepiness)
{
    switch (state) {
        case MuseIdleState::Happy:
            return {&face_assets::eyes_e4_smile, &face_assets::mouth_m4_smile};
        case MuseIdleState::Grumpy:
            return {&face_assets::eyes_e2_half, &face_assets::mouth_frown};
        case MuseIdleState::Drowsy: {
            const FaceLayer* eyes = &face_assets::eyes_e1_open;
            if (sleepiness >= 84) {
                eyes = &face_assets::eyes_e3_closed;
            } else if (sleepiness >= 77) {
                eyes = &face_assets::eyes_e2_half;
            }
            return {eyes, &face_assets::mouth_m1_closed};
        }
        case MuseIdleState::Sleeping:
            return {&face_assets::eyes_e3_closed, &face_assets::mouth_m1_closed};
        case MuseIdleState::NetworkError:
            return {&face_assets::eyes_squeeze, &face_assets::mouth_frown};
        case MuseIdleState::Normal:
        default:
            return {&face_assets::eyes_e1_open, &face_assets::mouth_m1_closed};
    }
}

const FaceLayer* lookEyes(int index)
{
    static const FaceLayer* kLooks[] = {
        &face_assets::eyes_look_up,
        &face_assets::eyes_look_up_l,
        &face_assets::eyes_look_up_r,
    };
    return kLooks[index % 3];
}

void clearPetOverlay()
{
    petOverlay = false;
}

void startPetOverlay(uint32_t now)
{
    petOverlay = true;
    petUntil = now + kPetOverlayMs;
    renderer.setExpression({&face_assets::eyes_squeeze, &face_assets::mouth_m1_closed});
    animator.setBlinkEnabled(false, now);
}

void applyIdle(MuseIdleState state, uint32_t now, bool force)
{
    if (!force && state == applied && !inSdkFace && !petOverlay) {
        // Drowsy eyes may still need refresh from sleepiness.
        if (state != MuseIdleState::Drowsy) {
            return;
        }
    }
    applied = state;
    inSdkFace = false;
    appliedMode = MUSE_MODE_IDLE;
    mouthOpen = false;
    listenOpen = false;
    looking = false;

    if (petOverlay) {
        return;
    }

    const MuseParams p = stateMachine.params();
    appliedEyeBand = state == MuseIdleState::Drowsy ? drowsyEyeBand(p.sleepiness) : -1;
    renderer.setExpression(idleExpression(state, p.sleepiness));
    animator.setBlinkEnabled(wantsBlink(state, p.sleepiness), now);
    ESP_LOGI(kTag, "idle: %s (h=%d s=%d b=%d)", idleName(state), p.happiness, p.sleepiness, p.boredom);
}

void showBootOff(uint32_t now)
{
    inSdkFace = true;
    appliedMode = MUSE_MODE_BOOT;
    animator.setBlinkEnabled(false, now);
    renderer.setExpression({&face_assets::eyes_e3_closed, &face_assets::mouth_m1_closed});
}

void showError(uint32_t now)
{
    inSdkFace = true;
    appliedMode = MUSE_MODE_ERROR;
    animator.setBlinkEnabled(false, now);
    renderer.setExpression({&face_assets::eyes_squeeze, &face_assets::mouth_frown});
}

void showListening(uint32_t now, bool entering)
{
    inSdkFace = true;
    appliedMode = MUSE_MODE_LISTENING;
    animator.setBlinkEnabled(false, now);
    if (entering) {
        listenOpen = false;
        listenAt = now + randRange(kListenBlinkMinMs, kListenBlinkMaxMs);
    }
    renderer.setExpression(
        {listenOpen ? &face_assets::eyes_e1_open : &face_assets::eyes_e3_closed, &face_assets::mouth_m1_closed});
}

void showThinking(uint32_t now, bool entering)
{
    inSdkFace = true;
    appliedMode = MUSE_MODE_THINKING;
    animator.setBlinkEnabled(false, now);
    if (entering) {
        thinkIndex = 0;
        thinkAt = now;
    }
    renderer.setExpression({lookEyes(thinkIndex), &face_assets::mouth_flat});
}

void showSpeaking(uint32_t now, bool entering)
{
    inSdkFace = true;
    appliedMode = MUSE_MODE_SPEAKING;
    animator.setBlinkEnabled(false, now);
    if (entering) {
        mouthOpen = false;
        mouthAt = now;
        muse_clip_cancel();
    }
    renderer.setExpression(
        {&face_assets::eyes_e1_open, mouthOpen ? &face_assets::mouth_m3_big_open : &face_assets::mouth_m2_small_open});
}

void updateSdkAnim(muse_mode_t mode, uint32_t now)
{
    if (mode == MUSE_MODE_SPEAKING) {
        if (static_cast<int32_t>(now - mouthAt) >= static_cast<int32_t>(kMouthStepMs)) {
            mouthAt = now;
            mouthOpen = !mouthOpen;
            renderer.setExpression({&face_assets::eyes_e1_open,
                                    mouthOpen ? &face_assets::mouth_m3_big_open : &face_assets::mouth_m2_small_open});
        }
        return;
    }
    if (mode == MUSE_MODE_THINKING) {
        if (static_cast<int32_t>(now - thinkAt) >= static_cast<int32_t>(kThinkStepMs)) {
            thinkAt = now;
            thinkIndex = (thinkIndex + 1) % 3;
            renderer.setExpression({lookEyes(thinkIndex), &face_assets::mouth_flat});
        }
        return;
    }
    if (mode == MUSE_MODE_LISTENING) {
        if (!listenOpen && static_cast<int32_t>(now - listenAt) >= 0) {
            listenOpen = true;
            listenAt = now + kListenOpenMs;
            renderer.setExpression({&face_assets::eyes_e1_open, &face_assets::mouth_m1_closed});
        } else if (listenOpen && static_cast<int32_t>(now - listenAt) >= 0) {
            listenOpen = false;
            listenAt = now + randRange(kListenBlinkMinMs, kListenBlinkMaxMs);
            renderer.setExpression({&face_assets::eyes_e3_closed, &face_assets::mouth_m1_closed});
        }
    }
}

void updateSelfPlay(uint32_t now)
{
    if (petOverlay || inSdkFace || !stateMachine.selfPlayActive()) {
        if (looking) {
            looking = false;
            if (!animator.inBlink()) {
                renderer.overrideEyes(nullptr);
            }
        }
        return;
    }
    if (animator.inBlink()) {
        return;
    }
    if (!looking) {
        if (lookAt == 0) {
            lookAt = now + randRange(kLookMinMs, kLookMaxMs);
        }
        if (static_cast<int32_t>(now - lookAt) < 0) {
            return;
        }
        looking = true;
        lookIndex = static_cast<int>(esp_random() % 3);
        lookAt = now + kLookHoldMs;
        renderer.overrideEyes(lookEyes(lookIndex));
        ESP_LOGI(kTag, "self-play look / hand flap (servo stub)");
        return;
    }
    if (static_cast<int32_t>(now - lookAt) >= 0) {
        looking = false;
        lookAt = now + randRange(kLookMinMs, kLookMaxMs);
        renderer.overrideEyes(nullptr);
    }
}

void handleTouch(uint32_t now)
{
    const muse_board_t* board = muse_board_get();
    int16_t x = 0;
    int16_t y = 0;
    const bool down = board != nullptr && board->touch_read != nullptr && board->touch_read(&x, &y);

    if (down && !touchDown) {
        touchDown = true;
        touchDownAt = now;
        petFired = false;
        if (tapCount > 0 && static_cast<int32_t>(now - tapTimes[tapCount - 1]) > static_cast<int32_t>(kTapWindowMs)) {
            tapCount = 0;
        }
        if (tapCount < kTapPetCount) {
            tapTimes[tapCount++] = now;
        }
        if (tapCount >= kTapPetCount) {
            tapCount = 0;
            stateMachine.onHeadPet(now);
            queuePetVoice();
            if (!conversationMode(muse_state_mode(nullptr))) {
                startPetOverlay(now);
            }
            if (stateMachine.state() == MuseIdleState::Happy) {
                ESP_LOGI(kTag, "happy pet hand flap (servo stub)");
            }
        }
        return;
    }

    if (down && touchDown && !petFired &&
        static_cast<int32_t>(now - touchDownAt) >= static_cast<int32_t>(kPetHoldMs)) {
        petFired = true;
        tapCount = 0;
        stateMachine.onHeadPet(now);
        queuePetVoice();
        if (!conversationMode(muse_state_mode(nullptr))) {
            startPetOverlay(now);
        }
        if (stateMachine.state() == MuseIdleState::Happy) {
            ESP_LOGI(kTag, "happy pet hand flap (servo stub)");
        }
        return;
    }

    if (!down && touchDown) {
        const uint32_t held = now - touchDownAt;
        touchDown = false;
        if (!petFired && held < kShortTouchMs) {
            stateMachine.onFaceTouch(now);
            queueTouchClip(MUSE_CLIP_TOUCH);
            if (!conversationMode(muse_state_mode(nullptr))) {
                startPetOverlay(now);
            }
        }
    }
}

void playSideEffects(uint32_t now)
{
    if (stateMachine.consumeWokeUp()) {
        muse_clip_play(MUSE_CLIP_WAKE_UP);
    }
    if (stateMachine.consumeEnteredHappy()) {
        muse_clip_play(MUSE_CLIP_HAPPY);
    }
    if (stateMachine.takeCallCue(now)) {
        muse_clip_play(MUSE_CLIP_CALL);
    }
}

void faceTask(void*)
{
    muse_settings_set_sleep_s(0);

    if (!display::begin(kBrightness) || !renderer.begin()) {
        ESP_LOGE(kTag, "face: init failed");
        vTaskDelete(nullptr);
        return;
    }

    uint32_t now = nowMs();
    stateMachine.begin(now);
    applyIdle(stateMachine.state(), now, true);
    prevMode = muse_state_mode(nullptr);

    for (;;) {
        now = nowMs();
        handleTouch(now);

        if (petOverlay && static_cast<int32_t>(now - petUntil) >= 0) {
            clearPetOverlay();
            applyIdle(stateMachine.state(), now, true);
        }

        const muse_mode_t mode = muse_state_mode(nullptr);
        const bool wasConversation = conversationMode(prevMode);
        const bool isConversation = conversationMode(mode);

        if (prevMode == MUSE_MODE_SPEAKING && mode == MUSE_MODE_IDLE) {
            stateMachine.onConversationEnd(now);
        }

        if (mode == MUSE_MODE_OFF || mode == MUSE_MODE_BOOT) {
            if (!inSdkFace || appliedMode != mode) {
                showBootOff(now);
            }
        } else if (mode == MUSE_MODE_ERROR) {
            if (!inSdkFace || appliedMode != MUSE_MODE_ERROR) {
                showError(now);
            }
        } else if (isConversation) {
            if (petOverlay) {
                clearPetOverlay();
            }
            if (mode == MUSE_MODE_LISTENING && (!inSdkFace || appliedMode != MUSE_MODE_LISTENING)) {
                showListening(now, true);
            } else if (mode == MUSE_MODE_THINKING && (!inSdkFace || appliedMode != MUSE_MODE_THINKING)) {
                showThinking(now, true);
            } else if (mode == MUSE_MODE_SPEAKING && (!inSdkFace || appliedMode != MUSE_MODE_SPEAKING)) {
                showSpeaking(now, true);
            } else {
                updateSdkAnim(mode, now);
            }
        } else {
            stateMachine.update(now, false, linkFatalNow());
            playSideEffects(now);
            if (touchClipSet) {
                if (touchClip == MUSE_CLIP_NONE) {
                    muse_clip_cancel();
                } else {
                    muse_clip_play(touchClip);
                }
                touchClipSet = false;
            }
            if (inSdkFace || wasConversation) {
                applyIdle(stateMachine.state(), now, true);
            } else if (!petOverlay) {
                const MuseIdleState st = stateMachine.state();
                const int band = st == MuseIdleState::Drowsy ? drowsyEyeBand(stateMachine.params().sleepiness) : -1;
                if (st != applied || band != appliedEyeBand) {
                    applyIdle(st, now, true);
                }
                animator.update(now);
                updateSelfPlay(now);
            }
        }

        prevMode = mode;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

}  // namespace

extern "C" void muse_face_start(void)
{
    if (xTaskCreate(faceTask, "muse_face", 12288, nullptr, 4, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "face task failed");
    }
}
