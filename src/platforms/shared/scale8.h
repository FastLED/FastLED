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
FL_ALWAYS_INLINE u8 scale8(u8 i, fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    return (((u16)i) * (1 + (u16)(scale))) >> 8;
#else
    return ((u16)i * (u16)(scale)) >> 8;
#endif
}

constexpr u8 scale8_constexpr(u8 i, fract8 scale) {
    return (((u16)i) * (1 + (u16)(scale))) >> 8;
}

/// The "video" version of scale8() (C implementation)
FL_ALWAYS_INLINE u8 scale8_video(u8 i, fract8 scale) {
    u8 j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// This version of scale8() does not clean up the R1 register (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE u8 scale8_LEAVING_R1_DIRTY(u8 i,
                                                         fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    return (((u16)i) * ((u16)(scale) + 1)) >> 8;
#else
    return ((int)i * (int)(scale)) >> 8;
#endif
}

/// In place modifying version of scale8() that does not clean up the R1 (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_LEAVING_R1_DIRTY(u8 &i,
                                                       fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    i = (((u16)i) * ((u16)(scale) + 1)) >> 8;
#else
    i = ((int)i * (int)(scale)) >> 8;
#endif
}

/// This version of scale8_video() does not clean up the R1 register (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE u8 scale8_video_LEAVING_R1_DIRTY(u8 i,
                                                               fract8 scale) {
    u8 j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// In place modifying version of scale8_video() that does not clean up the R1 (C implementation)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_video_LEAVING_R1_DIRTY(u8 &i,
                                                             fract8 scale) {
    i = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
}

/// Clean up the r1 register after a series of *LEAVING_R1_DIRTY calls (C implementation)
FL_ALWAYS_INLINE void cleanup_R1() {
    // No-op for non-AVR platforms
}

/// Scale a 16-bit unsigned value by an 8-bit value (C implementation)
FL_ALWAYS_INLINE u16 scale16by8(u16 i, fract8 scale) {
    if (scale == 0) {
        return 0;
    }
#if FASTLED_SCALE8_FIXED == 1
    u16 result;
    result = (((u32)(i) * (1 + ((u32)scale))) >> 8);
    return result;
#else
    u16 result;
    result = (i * scale) / 256;
    return result;
#endif
}

/// Scale a 16-bit unsigned value by an 16-bit value (C implementation)
LIB8STATIC u16 scale16(u16 i, fract16 scale) {
#if FASTLED_SCALE8_FIXED == 1
    u16 result;
    result = ((u32)(i) * (1 + (u32)(scale))) / 65536;
    return result;
#else
    u16 result;
    result = ((u32)(i) * (u32)(scale)) / 65536;
    return result;
#endif
}

/// Scale a 32-bit unsigned value by an 8-bit value (C implementation)
/// Promotes to 64-bit to prevent overflow during multiplication
FL_ALWAYS_INLINE u32 scale32by8(u32 i, fract8 scale) {
    if (scale == 0) {
        return 0;
    }
#if FASTLED_SCALE8_FIXED == 1
    u32 result;
    result = (((u64)(i) * (1 + ((u64)scale))) >> 8);
    return result;
#else
    u32 result;
    result = (((u64)i * (u64)scale) >> 8);
    return result;
#endif
}

/// @} Scaling_C

}  // namespace fl
