#include "display.h"

extern "C" {
#include "muse_board.h"

esp_err_t esp_lv_adapter_pause(int timeout_ms);
}

namespace {

struct Push {
    const uint16_t* pixels;
    int width;
};

// LVGL draws this panel as RGB565_SWAPPED. Assets are native little-endian RGB565.
void fillSwapped(uint8_t* dst, int stride, int row, int rows, void* arg)
{
    const auto* push = static_cast<const Push*>(arg);
    for (int r = 0; r < rows; ++r) {
        const uint16_t* src = push->pixels + static_cast<size_t>(row + r) * push->width;
        auto* out = reinterpret_cast<uint16_t*>(dst + static_cast<size_t>(r) * stride);
        for (int x = 0; x < push->width; ++x) {
            out[x] = __builtin_bswap16(src[x]);
        }
    }
}

}  // namespace

namespace display {

bool begin(uint8_t brightness)
{
    const muse_board_t* board = muse_board_get();
    if (board == nullptr || board->draw_frame == nullptr) {
        return false;
    }
    // Stop LVGL from painting the official UI. Do not call display_pause:
    // that also puts the touch controller into deep sleep.
    esp_lv_adapter_pause(-1);
    if (board->set_brightness != nullptr) {
        int pct = brightness * 100 / 255;
        if (pct < 10) {
            pct = 10;
        }
        board->set_brightness(pct);
    }
    return true;
}

void push(int16_t x, int16_t y, const uint16_t* pixels, int16_t w, int16_t h)
{
    const muse_board_t* board = muse_board_get();
    if (board == nullptr || board->draw_frame == nullptr || pixels == nullptr || w <= 0 || h <= 0) {
        return;
    }
    Push pushJob{pixels, w};
    board->draw_frame(x, y, x + w, y + h, 8, fillSwapped, &pushJob);
}

}  // namespace display
