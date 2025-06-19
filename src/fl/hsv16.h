#pragma once

#include "fl/stdint.h"
#include "crgb.h"

namespace fl {

struct HSV16 {
    uint16_t h = 0;
    uint16_t s = 0;
    uint16_t v = 0;

    HSV16() = default;
    HSV16(uint16_t h, uint16_t s, uint16_t v) : h(h), s(s), v(v) {}
    HSV16(const CRGB& rgb);
    CRGB ToRGB() const;

    // Are you using WS2812 (or other RGB8 LEDS) to display video?
    // Do you want a function to simulate gamma correction but doesn't
    // decimate the color? Use colorBoost() to boost the saturation without
    // decimating the color from 8 bit -> gamma -> 8 bit (leaving only 8 colors for each component).
    CRGB colorBoost() const;
};

}  // namespace fl
