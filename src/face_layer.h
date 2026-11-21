#pragma once

#include <stdint.h>

struct FaceLayer {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    const uint16_t* pixels;
    // nullptr means fully opaque
    const uint8_t* alpha;
};
