#pragma once

/// @file fl/math/time_functions.h
/// Timekeeping helper functions — seconds, minutes, hours, bseconds.

#include "fl/stl/int.h"
#include "fl/stl/chrono.h" // fl::millis()
#include "fl/math/lib8static.h"
#include "platforms/is_platform.h" // FL_IS_AVR
#include "fl/stl/noexcept.h"

namespace fl {

/// @addtogroup Timekeeping
/// @{

/// Return the current seconds since boot in a 16-bit value.
LIB8STATIC u16 seconds16() FL_NOEXCEPT {
    u32 ms = fl::millis();
    u16 s16;
    s16 = ms / 1000;
    return s16;
}

/// Return the current minutes since boot in a 16-bit value.
LIB8STATIC u16 minutes16() FL_NOEXCEPT {
    u32 ms = fl::millis();
    u16 m16;
    m16 = (ms / (60000L)) & 0xFFFF;
    return m16;
}

/// Return the current hours since boot in an 8-bit value.
LIB8STATIC u8 hours8() FL_NOEXCEPT {
    u32 ms = fl::millis();
    u8 h8;
    h8 = (ms / (3600000L)) & 0xFF;
    return h8;
}

/// Helper routine to divide a 32-bit value by 1024, returning
/// only the low 16 bits.
/// On AVR, uses optimized assembly (6 shifts vs 40).
/// Used to convert millis to "binary seconds" (1 bsecond == 1024 millis).
LIB8STATIC u16 div1024_32_16(u32 in32) FL_NOEXCEPT {
    u16 out16;
#if defined(FL_IS_AVR)
    asm volatile("  lsr %D[in]  \n\t"
                 "  ror %C[in]  \n\t"
                 "  ror %B[in]  \n\t"
                 "  lsr %D[in]  \n\t"
                 "  ror %C[in]  \n\t"
                 "  ror %B[in]  \n\t"
                 "  mov %B[out],%C[in] \n\t"
                 "  mov %A[out],%B[in] \n\t"
                 : [in] "+r"(in32), [out] "=r"(out16));
#else
    out16 = (in32 >> 10) & 0xFFFF;
#endif
    return out16;
}

/// Returns the current time-since-boot in
/// "binary seconds", which are actually 1024/1000 of a
/// second long.
LIB8STATIC u16 bseconds16() FL_NOEXCEPT {
    u32 ms = fl::millis();
    u16 s16;
    s16 = div1024_32_16(ms);
    return s16;
}

/// @}

} // namespace fl
