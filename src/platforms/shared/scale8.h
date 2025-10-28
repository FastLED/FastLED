#pragma once

#include "platforms/math8_config.h"
#include "crgb.h"
#include "fastled_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

namespace fl {

/// @file scale8.h
/// C language implementations of 8-bit scaling functions.
/// This file contains portable C versions of functions from scale8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Scaling_C C Scaling Implementations
/// Portable C implementations of 8-bit scaling operations
/// @{

/// Scale one byte by a second one (C implementation)
FL_ALWAYS_INLINE uint8_t scale8(uint8_t i, fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    return (((uint16_t)i) * (1 + (uint16_t)(scale))) >> 8;
#else
    return ((uint16_t)i * (uint16_t)(scale)) >> 8;
#endif
}

constexpr uint8_t scale8_constexpr(uint8_t i, fract8 scale) {
    return (((uint16_t)i) * (1 + (uint16_t)(scale))) >> 8;
}

/// The "video" version of scale8() (C implementation)
FL_ALWAYS_INLINE uint8_t scale8_video(uint8_t i, fract8 scale) {
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// This version of scale8() does not clean up the R1 register (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE uint8_t scale8_LEAVING_R1_DIRTY(uint8_t i,
                                                         fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    return (((uint16_t)i) * ((uint16_t)(scale) + 1)) >> 8;
#else
    return ((int)i * (int)(scale)) >> 8;
#endif
}

/// In place modifying version of scale8() that does not clean up the R1 (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_LEAVING_R1_DIRTY(uint8_t &i,
                                                       fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    i = (((uint16_t)i) * ((uint16_t)(scale) + 1)) >> 8;
#else
    i = ((int)i * (int)(scale)) >> 8;
#endif
}

/// This version of scale8_video() does not clean up the R1 register (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE uint8_t scale8_video_LEAVING_R1_DIRTY(uint8_t i,
                                                               fract8 scale) {
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// In place modifying version of scale8_video() that does not clean up the R1 (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_video_LEAVING_R1_DIRTY(uint8_t &i,
                                                             fract8 scale) {
    i = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
}

/// Clean up the r1 register after a series of *LEAVING_R1_DIRTY calls (C implementation)
FL_ALWAYS_INLINE void cleanup_R1() {
    // No-op for non-AVR platforms
}

/// Scale a 16-bit unsigned value by an 8-bit value (C implementation)
FL_ALWAYS_INLINE uint16_t scale16by8(uint16_t i, fract8 scale) {
    if (scale == 0) {
        return 0;
    }
#if FASTLED_SCALE8_FIXED == 1
    uint16_t result;
    result = (((uint32_t)(i) * (1 + ((uint32_t)scale))) >> 8);
    return result;
#else
    uint16_t result;
    result = (i * scale) / 256;
    return result;
#endif
}

/// Scale a 16-bit unsigned value by an 16-bit value (C implementation)
LIB8STATIC uint16_t scale16(uint16_t i, fract16 scale) {
#if FASTLED_SCALE8_FIXED == 1
    uint16_t result;
    result = ((uint32_t)(i) * (1 + (uint32_t)(scale))) / 65536;
    return result;
#else
    uint16_t result;
    result = ((uint32_t)(i) * (uint32_t)(scale)) / 65536;
    return result;
#endif
}

/// @} Scaling_C

}  // namespace fl
