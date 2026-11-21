#include "touch.h"

#include <Wire.h>

#include "TouchDrvCSTXXX.hpp"
#include "board_pins.h"

namespace touch {

namespace {

constexpr uint8_t kAddress = CST92XX_SLAVE_ADDRESS;
constexpr uint8_t kMaxPoints = 2;

TouchDrvCST92xx driver;
int16_t x_[kMaxPoints] = {};
int16_t y_[kMaxPoints] = {};
bool wasDown_ = false;

}  // namespace

bool begin() {
    driver.setPins(pins::kTouchReset, pins::kTouchInt);
    if (!driver.begin(Wire, kAddress, pins::kI2cSda, pins::kI2cScl)) {
        return false;
    }
    wasDown_ = false;
    return true;
}

bool wasPressed() {
    const uint8_t support = driver.getSupportTouchPoint();
    const uint8_t limit = support < kMaxPoints ? support : kMaxPoints;
    const uint8_t count = driver.getPoint(x_, y_, limit);
    const bool down = count > 0;
    const bool pressed = down && !wasDown_;
    wasDown_ = down;
    return pressed;
}

}  // namespace touch
