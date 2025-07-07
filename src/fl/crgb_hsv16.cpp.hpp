/// @file crgb_hsv16.cpp
/// HSV16-dependent methods for CRGB - only linked when HSV16 functionality is used

#define FASTLED_INTERNAL
#include "crgb.h"
#include "fl/hsv16.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

CRGB CRGB::colorBoost(fl::EaseType saturation_function, fl::EaseType luminance_function) const {
    fl::HSV16 hsv(*this);
    return hsv.colorBoost(saturation_function, luminance_function);
}

void CRGB::colorBoost(const CRGB* src, CRGB* dst, size_t count, fl::EaseType saturation_function, fl::EaseType luminance_function) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i].colorBoost(saturation_function, luminance_function);
    }
}

fl::HSV16 CRGB::toHSV16() const {
    return fl::HSV16(*this);
}

// Constructor implementation for HSV16 -> CRGB automatic conversion
CRGB::CRGB(const fl::HSV16& rhs) {
    *this = rhs.ToRGB();
}

FASTLED_NAMESPACE_END
