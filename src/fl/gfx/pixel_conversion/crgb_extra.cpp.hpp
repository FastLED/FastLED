// IWYU pragma: private
// ok no header - public CRGB declarations remain in crgb.h.
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
#include "fl/gfx/hsv16.h"
#include "fl/stl/noexcept.h"

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
CRGB& CRGB::operator=(const hsv8& rhs) FL_NO_EXCEPT {
    CHSV hsv_color(rhs.h, rhs.s, rhs.v);
    CRGB rgb_result;
    hsv2rgb_rainbow(hsv_color, rgb_result);
    r = rgb_result.r;
    g = rgb_result.g;
    b = rgb_result.b;
    return *this;
}

}  // namespace fl
