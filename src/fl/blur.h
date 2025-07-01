#pragma once

#include "fl/stdint.h"

#include "fl/int.h"
#include "crgb.h"
#include "fl/deprecated.h"

namespace fl {

/// @defgroup ColorBlurs Color Blurring Functions
/// Functions for blurring colors
/// @{

/// One-dimensional blur filter.
/// Spreads light to 2 line neighbors.
///   * 0 = no spread at all
///   * 64 = moderate spreading
///   * 172 = maximum smooth, even spreading
///   * 173..255 = wider spreading, but increasing flicker
///
/// Total light is NOT entirely conserved, so many repeated
/// calls to 'blur' will also result in the light fading,
/// eventually all the way to black; this is by design so that
/// it can be used to (slowly) clear the LEDs to black.
/// @param leds a pointer to the LED array to blur
/// @param numLeds the number of LEDs to blur
/// @param blur_amount the amount of blur to apply
void blur1d(CRGB *leds, u16 numLeds, fract8 blur_amount);

/// Two-dimensional blur filter.
/// Spreads light to 8 XY neighbors.
///   * 0 = no spread at all
///   * 64 = moderate spreading
///   * 172 = maximum smooth, even spreading
///   * 173..255 = wider spreading, but increasing flicker
///
/// Total light is NOT entirely conserved, so many repeated
/// calls to 'blur' will also result in the light fading,
/// eventually all the way to black; this is by design so that
/// it can be used to (slowly) clear the LEDs to black.
/// @param leds a pointer to the LED array to blur
/// @param width the width of the matrix
/// @param height the height of the matrix
/// @param blur_amount the amount of blur to apply
void blur2d(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
            const fl::XYMap &xymap);

/// Legacy version of blur2d, which does not require an XYMap but instead
/// implicitly binds to XY() function. If you are hitting a linker error here,
/// then use blur2d(..., const fl::XYMap& xymap) instead.
void blur2d(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount)
    FASTLED_DEPRECATED("Use blur2d(..., const fl::XYMap& xymap) instead");

/// Perform a blur1d() on every row of a rectangular matrix
/// @see blur1d()
/// @param leds a pointer to the LED array to blur
/// @param width the width of the matrix
/// @param height the height of the matrix
/// @param blur_amount the amount of blur to apply
void blurRows(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
              const fl::XYMap &xymap);

/// Perform a blur1d() on every column of a rectangular matrix
/// @copydetails blurRows()
void blurColumns(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
                 const fl::XYMap &xymap);

/// @} ColorBlurs

} // namespace fl
