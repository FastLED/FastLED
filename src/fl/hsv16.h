#pragma once

#include "fl/stdint.h"
#include "fl/int.h"
#include "crgb.h"
#include "fl/ease.h"

namespace fl {

struct HSV16 {
    u16 h = 0;
    u16 s = 0;
    u16 v = 0;

    HSV16() = default;
    HSV16(u16 h, u16 s, u16 v) : h(h), s(s), v(v) {}
    HSV16(const CRGB& rgb);
    
    // Rule of 5 for POD data
    HSV16(const HSV16 &other) = default;
    HSV16 &operator=(const HSV16 &other) = default;
    HSV16(HSV16 &&other) noexcept = default;
    HSV16 &operator=(HSV16 &&other) noexcept = default;
    
    CRGB ToRGB() const;
    
    /// Automatic conversion operator to CRGB
    /// Allows HSV16 to be automatically converted to CRGB
    operator CRGB() const { return ToRGB(); }

    // Are you using WS2812 (or other RGB8 LEDS) to display video?
    // decimate the color? Use colorBoost() to boost the saturation.
    // This works great for WS2812 and any other RGB8 LEDs.
    // Default saturation function is similar to gamma correction.
    CRGB colorBoost(EaseType saturation_function = EASE_IN_QUAD, EaseType luminance_function = EASE_NONE) const;
};

}  // namespace fl
