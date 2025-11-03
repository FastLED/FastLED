#pragma once

#include "platforms/math8_config.h"
#include "crgb.h"
#include "fastled_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

namespace fl {

/// @file scale8.h
/// ATtiny-specific implementations of 8-bit scaling functions (no MUL instruction).
/// This file contains shift-and-add assembly and C implementations for ATtiny.

/// @ingroup lib8tion
/// @{

/// @defgroup Scaling_ATtiny ATtiny Scaling Implementations
/// C and shift-and-add implementations for ATtiny (no MUL instruction)
/// @{

/// Scale one byte by a second one (ATtiny shift-and-add assembly)
/// Using inline instead of FL_ALWAYS_INLINE to reduce register pressure on ATtiny
inline uint8_t scale8(uint8_t i, fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    uint8_t work = i;
#else
    uint8_t work = 0;
#endif
    uint8_t cnt = 0x80;
    asm volatile(
#if (FASTLED_SCALE8_FIXED == 1)
        "  inc %[scale]                 \n\t"
        "  breq DONE_%=                 \n\t"
        "  clr %[work]                  \n\t"
#endif
        "LOOP_%=:                       \n\t"
        "  sbrc %[scale], 0             \n\t"
        "  add %[work], %[i]            \n\t"
        "  ror %[work]                  \n\t"
        "  lsr %[scale]                 \n\t"
        "  lsr %[cnt]                   \n\t"
        "brcc LOOP_%=                   \n\t"
        "DONE_%=:                       \n\t"
        : [work] "+r"(work), [cnt] "+r"(cnt)
        : [scale] "r"(scale), [i] "r"(i)
        :);
    return work;
}

/// The "video" version of scale8() (C implementation for ATtiny)
inline uint8_t scale8_video(uint8_t i, fract8 scale) {
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// This version of scale8() does not clean up the R1 register (C implementation for ATtiny)
/// @warning You **MUST** call cleanup_R1() after using this function!
inline uint8_t scale8_LEAVING_R1_DIRTY(uint8_t i, fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    return (((uint16_t)i) * ((uint16_t)(scale) + 1)) >> 8;
#else
    return ((int)i * (int)(scale)) >> 8;
#endif
}

/// In place modifying version of scale8() that does not clean up the R1 (C implementation for ATtiny)
/// @warning You **MUST** call cleanup_R1() after using this function!
inline void nscale8_LEAVING_R1_DIRTY(uint8_t &i, fract8 scale) {
#if (FASTLED_SCALE8_FIXED == 1)
    i = (((uint16_t)i) * ((uint16_t)(scale) + 1)) >> 8;
#else
    i = ((int)i * (int)(scale)) >> 8;
#endif
}

/// This version of scale8_video() does not clean up the R1 register (C implementation for ATtiny)
/// @warning You **MUST** call cleanup_R1() after using this function!
inline uint8_t scale8_video_LEAVING_R1_DIRTY(uint8_t i, fract8 scale) {
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
    return j;
}

/// In place modifying version of scale8_video() that does not clean up the R1 (C implementation for ATtiny)
/// @warning You **MUST** call cleanup_R1() after using this function!
inline void nscale8_video_LEAVING_R1_DIRTY(uint8_t &i, fract8 scale) {
    i = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
}

/// Clean up the r1 register after a series of *LEAVING_R1_DIRTY calls (no-op for ATtiny C implementations)
inline void cleanup_R1() {
    // No-op for C implementations - no r1 register to clean up
}

/// Scale a 16-bit unsigned value by an 8-bit value (C implementation for ATtiny)
inline uint16_t scale16by8(uint16_t i, fract8 scale) {
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

/// Scale a 16-bit unsigned value by an 16-bit value (C implementation for ATtiny)
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

/// @} Scaling_ATtiny

}  // namespace fl
