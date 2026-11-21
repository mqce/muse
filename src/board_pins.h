#pragma once

#include <stdint.h>

namespace pins {

constexpr int8_t kLcdCs = 12;
constexpr int8_t kLcdSclk = 38;
constexpr int8_t kLcdSdio0 = 4;
constexpr int8_t kLcdSdio1 = 5;
constexpr int8_t kLcdSdio2 = 6;
constexpr int8_t kLcdSdio3 = 7;
constexpr int8_t kLcdReset = 39;

constexpr int8_t kI2cSda = 15;
constexpr int8_t kI2cScl = 14;
constexpr int8_t kTouchInt = 11;
constexpr int8_t kTouchReset = 40;

}  // namespace pins
