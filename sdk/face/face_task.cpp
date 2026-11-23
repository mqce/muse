extern "C" {
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "muse_board.h"
#include "muse_settings.h"
}

#include "display.h"
#include "face_anim.h"
#include "face_renderer.h"
#include "generated/face_assets.h"

namespace {

constexpr char kTag[] = "muse_face";
constexpr uint8_t kBrightness = 200;
constexpr uint32_t kMouthStepMs = 150;

const Expression kFaces[] = {
    {&face_assets::eyes_default, &face_assets::mouth_default},
    {&face_assets::eyes_half, &face_assets::mouth_open1},
    {&face_assets::eyes_closed, &face_assets::mouth_default},
};

FaceRenderer renderer(face_assets::bg);
FaceAnimator animator(renderer);
MuseStateMachine stateMachine;
MuseState applied = MuseState::Normal;
bool talking = false;
bool mouthOpen = false;
uint32_t mouthAt = 0;
bool touchDown = false;

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

const char* stateName(MuseState state)
{
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

void applyLocal(MuseState state, uint32_t now)
{
    applied = state;
    talking = false;
    renderer.setExpression(kFaces[static_cast<int>(state)]);
    animator.setState(state, now);
    ESP_LOGI(kTag, "state: %s", stateName(state));
}

bool museBusy(muse_mode_t mode)
{
    return mode == MUSE_MODE_LISTENING || mode == MUSE_MODE_THINKING || mode == MUSE_MODE_SPEAKING;
}

bool touchPressed()
{
    const muse_board_t* board = muse_board_get();
    int16_t x = 0;
    int16_t y = 0;
    const bool down = board != nullptr && board->touch_read != nullptr && board->touch_read(&x, &y);
    const bool pressed = down && !touchDown;
    touchDown = down;
    return pressed;
}

void showTalking(uint32_t now)
{
    if (!talking) {
        talking = true;
        mouthOpen = false;
        mouthAt = now;
        renderer.setExpression({&face_assets::eyes_default, &face_assets::mouth_open1});
        ESP_LOGI(kTag, "state: talking");
    }
    if (static_cast<int32_t>(now - mouthAt) < static_cast<int32_t>(kMouthStepMs)) {
        return;
    }
    mouthAt = now;
    mouthOpen = !mouthOpen;
    renderer.setExpression(
        {&face_assets::eyes_default, mouthOpen ? &face_assets::mouth_open2 : &face_assets::mouth_open1});
}

void faceTask(void*)
{
    // The SDK blanks the panel on its own sleep timer and then resumes LVGL.
    muse_settings_set_sleep_s(0);

    if (!display::begin(kBrightness) || !renderer.begin()) {
        ESP_LOGE(kTag, "face: init failed");
        vTaskDelete(nullptr);
        return;
    }

    uint32_t now = nowMs();
    stateMachine.begin(now);
    applyLocal(stateMachine.state(), now);

    for (;;) {
        now = nowMs();
        if (touchPressed()) {
            stateMachine.onTouch(now);
        }

        const muse_mode_t mode = muse_state_mode(nullptr);
        if (museBusy(mode)) {
            showTalking(now);
        } else {
            if (talking) {
                applyLocal(stateMachine.state(), now);
            }
            stateMachine.update(now);
            const MuseState state = stateMachine.state();
            if (state != applied) {
                applyLocal(state, now);
            }
            animator.update(now);
        }
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
