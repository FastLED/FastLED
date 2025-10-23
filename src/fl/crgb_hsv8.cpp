/// @file crgb_hsv8.cpp
/// hsv8-dependent methods for CRGB (CRGB) - provides conversion from HSV to RGB

#define FASTLED_INTERNAL
#include "crgb.h"
#include "../hsv2rgb.h"

// Implementations are in fl namespace since CRGB is defined there
namespace fl {

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

}  // namespace fl
