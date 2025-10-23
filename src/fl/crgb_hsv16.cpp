/// @file crgb_hsv16.cpp
/// HSV16-dependent methods for CRGB (CRGB) - only linked when HSV16 functionality is used

#define FASTLED_INTERNAL
#include "crgb.h"
#include "fl/hsv16.h"
// Implementations are in fl namespace since CRGB is defined there
namespace fl {

CRGB CRGB::colorBoost(EaseType saturation_function, EaseType luminance_function) const {
    HSV16 hsv(*this);
    return hsv.colorBoost(saturation_function, luminance_function);
}

void CRGB::colorBoost(const CRGB* src, CRGB* dst, size_t count, EaseType saturation_function, EaseType luminance_function) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i].colorBoost(saturation_function, luminance_function);
    }
}

HSV16 CRGB::toHSV16() const {
    return HSV16(*this);
}

// Constructor implementation for HSV16 -> CRGB automatic conversion
CRGB::CRGB(const HSV16& rhs) {
    *this = rhs.ToRGB();
}

}  // namespace fl
