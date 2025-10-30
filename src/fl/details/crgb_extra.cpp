/// @file crgb_extra.cpp
/// HSV-dependent methods for CRGB - consolidated implementations
///
/// This file contains CRGB methods that depend on HSV types and conversions.
/// Keeping these separate from core CRGB functionality allows the linker to
/// exclude HSV conversion code when not used, reducing binary size on
/// embedded platforms.

#define FASTLED_INTERNAL
#include "crgb.h"
#include "hsv2rgb.h"
#include "fl/hsv16.h"

// Implementations are in fl namespace since CRGB is defined there
namespace fl {

// ============================================================================
// HSV8 Methods
// ============================================================================

/// Constructor from hsv8 - converts HSV color to RGB
CRGB::CRGB(const hsv8& rhs) {
    CHSV hsv_color(rhs.h, rhs.s, rhs.v);
    CRGB rgb_result;
    hsv2rgb_rainbow(hsv_color, rgb_result);
    r = rgb_result.r;
    g = rgb_result.g;
    b = rgb_result.b;
}

/// Assignment operator from hsv8 - converts HSV color to RGB
CRGB& CRGB::operator=(const hsv8& rhs) {
    CHSV hsv_color(rhs.h, rhs.s, rhs.v);
    CRGB rgb_result;
    hsv2rgb_rainbow(hsv_color, rgb_result);
    r = rgb_result.r;
    g = rgb_result.g;
    b = rgb_result.b;
    return *this;
}

// ============================================================================
// SetHSV Methods
// ============================================================================

/// Set HSV values and convert to RGB
CRGB& CRGB::setHSV(u8 hue, u8 sat, u8 val) {
    CHSV hsv_color(hue, sat, val);
    hsv2rgb_rainbow(hsv_color, *this);
    return *this;
}

/// Set hue only (saturation and value set to max) and convert to RGB
CRGB& CRGB::setHue(u8 hue) {
    CHSV hsv_color(hue, 255, 255);
    hsv2rgb_rainbow(hsv_color, *this);
    return *this;
}

// ============================================================================
// HSV16 Methods
// ============================================================================

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
