#include "display.h"

#include <Arduino_GFX_Library.h>

#include "board_pins.h"

namespace display {

namespace {

constexpr uint8_t kColumnOffset = 6;

Arduino_ESP32QSPI bus(pins::kLcdCs, pins::kLcdSclk, pins::kLcdSdio0, pins::kLcdSdio1,
                      pins::kLcdSdio2, pins::kLcdSdio3);
Arduino_CO5300 panel(&bus, pins::kLcdReset, 0, kWidth, kHeight, kColumnOffset, 0, 0, 0);

}  // namespace

bool begin(uint8_t brightness) {
    if (!panel.begin()) {
        return false;
    }
    panel.fillScreen(RGB565_BLACK);
    panel.setBrightness(brightness);
    return true;
}

void push(int16_t x, int16_t y, const uint16_t* pixels, int16_t w, int16_t h) {
    panel.draw16bitRGBBitmap(x, y, const_cast<uint16_t*>(pixels), w, h);
}

}  // namespace display
