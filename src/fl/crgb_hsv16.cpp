/// @file crgb_hsv16.cpp
/// HSV16-dependent methods for rgb8 (CRGB) - only linked when HSV16 functionality is used

#define FASTLED_INTERNAL
#include "crgb.h"
#include "fl/hsv16.h"
#include "fl/namespace.h"

// Implementations are in fl namespace since rgb8 is defined there
namespace fl {

rgb8 rgb8::colorBoost(EaseType saturation_function, EaseType luminance_function) const {
    HSV16 hsv(*this);
    return hsv.colorBoost(saturation_function, luminance_function);
}

void rgb8::colorBoost(const rgb8* src, rgb8* dst, size_t count, EaseType saturation_function, EaseType luminance_function) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i].colorBoost(saturation_function, luminance_function);
    }
}

HSV16 rgb8::toHSV16() const {
    return HSV16(*this);
}

// Constructor implementation for HSV16 -> rgb8 automatic conversion
rgb8::rgb8(const HSV16& rhs) {
    *this = rhs.ToRGB();
}

}  // namespace fl
