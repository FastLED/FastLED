#pragma once

#include "platforms/math8_config.h"
#include "crgb.h"
#include "fastled_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

// Select appropriate implementation based on platform configuration
#if defined(__AVR__)
#include "platforms/avr/is_avr.h"
#ifdef FL_IS_AVR_ATTINY
// ATtiny has no MUL instruction - use ATtiny-specific shift-and-add and C implementations
#include "platforms/avr/attiny/math/scale8_attiny.h"
#else
// Full AVR with MUL instruction support - use optimized MUL-based assembly
#include "platforms/avr/atmega/common/scale8_avr.h"
#endif
#else
// Portable C implementation for all other platforms (ARM, ESP32, etc.)
#include "platforms/shared/scale8.h"
#endif

namespace fl {
/// @file scale8.h
/// Fast, efficient 8-bit scaling functions specifically
/// designed for high-performance LED programming.

/// @addtogroup lib8tion
/// @{

/// @defgroup Scaling Scaling Functions
/// Fast, efficient 8-bit scaling functions specifically
/// designed for high-performance LED programming.
///
/// Because of the AVR(Arduino) and ARM assembly language
/// implementations provided, using these functions often
/// results in smaller and faster code than the equivalent
/// program using plain "C" arithmetic and logic.
/// @{

/// @defgroup ScalingDirty Scaling Functions that Leave R1 Dirty
/// These functions are more efficient for scaling multiple
/// bytes at once, but require calling cleanup_R1() afterwards.
/// @{

/// @ingroup ScalingDirty
// Note: scale8_LEAVING_R1_DIRTY, nscale8_LEAVING_R1_DIRTY, etc.
// are now defined in platforms/avr/scale8.h, platforms/avr/scale8_attiny.h, or platforms/shared/scale8.h

constexpr CRGB nscale8x3_constexpr(uint8_t r, uint8_t g, uint8_t b, fract8 scale) {
    return CRGB(((int)r * (int)(scale)) >> 8, ((int)g * (int)(scale)) >> 8,
                ((int)b * (int)(scale)) >> 8);
}

/// @} ScalingDirty

/// Scale three one-byte values by a fourth one, which is treated as
/// the numerator of a fraction whose demominator is 256.
///
/// In other words, it computes r,g,b * (scale / 256)
///
/// @warning This function always modifies its arguments in place!
/// @param r first value to scale
/// @param g second value to scale
/// @param b third value to scale
/// @param scale scale factor, in n/256 units
LIB8STATIC void nscale8x3(uint8_t &r, uint8_t &g, uint8_t &b, fract8 scale) {
#if SCALE8_C == 1
#if (FASTLED_SCALE8_FIXED == 1)
    uint16_t scale_fixed = scale + 1;
    r = (((uint16_t)r) * scale_fixed) >> 8;
    g = (((uint16_t)g) * scale_fixed) >> 8;
    b = (((uint16_t)b) * scale_fixed) >> 8;
#else
    r = ((int)r * (int)(scale)) >> 8;
    g = ((int)g * (int)(scale)) >> 8;
    b = ((int)b * (int)(scale)) >> 8;
#endif
#elif SCALE8_AVRASM == 1
    r = scale8_LEAVING_R1_DIRTY(r, scale);
    g = scale8_LEAVING_R1_DIRTY(g, scale);
    b = scale8_LEAVING_R1_DIRTY(b, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x3 available."
#endif
}

/// Scale three one-byte values by a fourth one, which is treated as
/// the numerator of a fraction whose demominator is 256.
///
/// In other words, it computes r,g,b * (scale / 256), ensuring
/// that non-zero values passed in remain non-zero, no matter how low the scale
/// argument.
///
/// @warning This function always modifies its arguments in place!
/// @param r first value to scale
/// @param g second value to scale
/// @param b third value to scale
/// @param scale scale factor, in n/256 units
LIB8STATIC void nscale8x3_video(uint8_t &r, uint8_t &g, uint8_t &b,
                                fract8 scale) {
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scale)) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scale)) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scale)) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    nscale8_video_LEAVING_R1_DIRTY(r, scale);
    nscale8_video_LEAVING_R1_DIRTY(g, scale);
    nscale8_video_LEAVING_R1_DIRTY(b, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x3 available."
#endif
}

/// Scale two one-byte values by a third one, which is treated as
/// the numerator of a fraction whose demominator is 256.
///
/// In other words, it computes i,j * (scale / 256).
///
/// @warning This function always modifies its arguments in place!
/// @param i first value to scale
/// @param j second value to scale
/// @param scale scale factor, in n/256 units
LIB8STATIC void nscale8x2(uint8_t &i, uint8_t &j, fract8 scale) {
#if SCALE8_C == 1
#if FASTLED_SCALE8_FIXED == 1
    uint16_t scale_fixed = scale + 1;
    i = (((uint16_t)i) * scale_fixed) >> 8;
    j = (((uint16_t)j) * scale_fixed) >> 8;
#else
    i = ((uint16_t)i * (uint16_t)(scale)) >> 8;
    j = ((uint16_t)j * (uint16_t)(scale)) >> 8;
#endif
#elif SCALE8_AVRASM == 1
    i = scale8_LEAVING_R1_DIRTY(i, scale);
    j = scale8_LEAVING_R1_DIRTY(j, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x2 available."
#endif
}

/// Scale two one-byte values by a third one, which is treated as
/// the numerator of a fraction whose demominator is 256.
///
/// In other words, it computes i,j * (scale / 256), ensuring
/// that non-zero values passed in remain non zero, no matter how low the scale
/// argument.
///
/// @warning This function always modifies its arguments in place!
/// @param i first value to scale
/// @param j second value to scale
/// @param scale scale factor, in n/256 units
LIB8STATIC void nscale8x2_video(uint8_t &i, uint8_t &j, fract8 scale) {
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    i = (i == 0) ? 0 : (((int)i * (int)(scale)) >> 8) + nonzeroscale;
    j = (j == 0) ? 0 : (((int)j * (int)(scale)) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    nscale8_video_LEAVING_R1_DIRTY(i, scale);
    nscale8_video_LEAVING_R1_DIRTY(j, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x2 available."
#endif
}

// Note: scale16by8() and scale16() are now defined in platforms/avr/scale8.h, platforms/avr/scale8_attiny.h, or platforms/shared/scale8.h
/// @} Scaling

/// @defgroup Dimming Dimming and Brightening Functions
/// Functions to dim or brighten data.
///
/// The eye does not respond in a linear way to light.
/// High speed PWM'd LEDs at 50% duty cycle appear far
/// brighter then the "half as bright" you might expect.
///
/// If you want your midpoint brightness LEDs (128) to
/// appear half as bright as "full" brightness (255), you
/// have to apply a "dimming function".
///
/// @note These are approximations of gamma correction with
///       a gamma value of 2.0.
/// @see @ref GammaFuncs
/// @{

/// Adjust a scaling value for dimming.
/// @see scale8()
LIB8STATIC uint8_t dim8_raw(uint8_t x) { return scale8(x, x); }

/// Adjust a scaling value for dimming for video (value will never go below 1)
/// @see scale8_video()
LIB8STATIC uint8_t dim8_video(uint8_t x) { return scale8_video(x, x); }

/// Linear version of the dimming function that halves for values < 128
LIB8STATIC uint8_t dim8_lin(uint8_t x) {
    if (x & 0x80) {
        x = scale8(x, x);
    } else {
        x += 1;
        x /= 2;
    }
    return x;
}

/// Brighten a value (inverse of dim8_raw())
LIB8STATIC uint8_t brighten8_raw(uint8_t x) {
    uint8_t ix = 255 - x;
    return 255 - scale8(ix, ix);
}

/// Brighten a value (inverse of dim8_video())
LIB8STATIC uint8_t brighten8_video(uint8_t x) {
    uint8_t ix = 255 - x;
    return 255 - scale8_video(ix, ix);
}

/// Brighten a value (inverse of dim8_lin())
LIB8STATIC uint8_t brighten8_lin(uint8_t x) {
    uint8_t ix = 255 - x;
    if (ix & 0x80) {
        ix = scale8(ix, ix);
    } else {
        ix += 1;
        ix /= 2;
    }
    return 255 - ix;
}

/// @} Dimming
/// @} lib8tion
}  // namespace fl
