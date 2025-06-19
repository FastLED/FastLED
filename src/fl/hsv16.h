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
};

}  // namespace fl
