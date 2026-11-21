#pragma once

#include <stdint.h>

namespace display {

constexpr int16_t kWidth = 466;
constexpr int16_t kHeight = 466;

bool begin(uint8_t brightness);
void push(int16_t x, int16_t y, const uint16_t* pixels, int16_t w, int16_t h);

}  // namespace display
