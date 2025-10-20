#pragma once

#include "lib8tion/config.h"
#include "lib8tion/scale8.h"
#include "lib8tion/lib8static.h"
#include "lib8tion/intmap.h"
#include "fl/compiler_control.h"

// Select appropriate implementation based on platform configuration
#if defined(__AVR__)
// AVR is slow - use assembly-optimized version for performance
#include "platforms/avr/math8.h"
#else
// All other processors (ARM, ESP32, etc.) are fast enough - use portable C version
#include "platforms/shared/math8.h"
#endif

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
namespace fl {
/// @file math8.h
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming.

/// @ingroup lib8tion
/// @{

/// @defgroup Math Basic Math Operations
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming.
///
/// Because of the AVR (Arduino) and ARM assembly language
/// implementations provided, using these functions often
/// results in smaller and faster code than the equivalent
/// program using plain "C" arithmetic and logic.
/// @{

// Note: mod8(), addmod8(), and submod8() are now defined in
// platforms/avr/math8.h or platforms/shared/math8.h

/// Square root for 16-bit integers.
/// About three times faster and five times smaller
/// than Arduino's general `sqrt` on AVR.
LIB8STATIC uint8_t sqrt16(uint16_t x) {
    if (x <= 1) {
        return x;
    }

    uint8_t low = 1; // lower bound
    uint8_t hi, mid;

    if (x > 7904) {
        hi = 255;
    } else {
        hi = (x >> 5) + 8; // initial estimate for upper bound
    }

    do {
        mid = (low + hi) >> 1;
        if ((uint16_t)(mid * mid) > x) {
            hi = mid - 1;
        } else {
            if (mid == 255) {
                return 255;
            }
            low = mid + 1;
        }
    } while (hi >= low);

    return low - 1;
}

LIB8STATIC_ALWAYS_INLINE uint8_t sqrt8(uint8_t x) {
    return sqrt16(map8_to_16(x));
}

/// @} Math
/// @} lib8tion
}  // namespace fl
FL_DISABLE_WARNING_POP
