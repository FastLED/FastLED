#pragma once

// IWYU pragma: private

#include "platforms/math8_config.h"
#include "lib8tion/scale8.h"
#include "lib8tion/lib8static.h"
#include "lib8tion/intmap.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/sketch_macros.h"

namespace fl {

/// @file math8.h
/// C language implementations of 8-bit math functions.
/// This file contains portable C versions of functions from math8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Math_C C Math Implementations
/// Portable C implementations of 8-bit math operations
/// @{

/// Add one byte to another, saturating at 0xFF (C implementation)
FL_ALWAYS_INLINE u8 qadd8(u8 i, u8 j) {
    unsigned int t = i + j;
    if (t > 255)
        t = 255;
    return static_cast<u8>(t);
}

/// Add one byte to another, saturating at 0x7F and -0x80 (C implementation)
FL_ALWAYS_INLINE i8 qadd7(i8 i, i8 j) {
    i16 t = i + j;
    if (t > 127)
        t = 127;
    else if (t < -128)
        t = -128;
    return static_cast<i8>(t);
}

/// Subtract one byte from another, saturating at 0x00 (C implementation)
FL_ALWAYS_INLINE u8 qsub8(u8 i, u8 j) {
    int t = i - j;
    if (t < 0)
        t = 0;
    return static_cast<u8>(t);
}

/// Add one byte to another, with 8-bit result (C implementation)
FL_ALWAYS_INLINE u8 add8(u8 i, u8 j) {
    int t = i + j;
    return static_cast<u8>(t);
}

/// Add one byte to two bytes, with 16-bit result (C implementation)
FL_ALWAYS_INLINE u16 add8to16(u8 i, u16 j) {
    u16 t = i + j;
    return t;
}

/// Subtract one byte from another, 8-bit result (C implementation)
FL_ALWAYS_INLINE u8 sub8(u8 i, u8 j) {
    int t = i - j;
    return static_cast<u8>(t);
}

/// Calculate an integer average of two unsigned 8-bit values (C implementation)
FL_ALWAYS_INLINE u8 avg8(u8 i, u8 j) {
    return (i + j) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values (C implementation)
FL_ALWAYS_INLINE u16 avg16(u16 i, u16 j) {
    u32 tmp = i;
    tmp += j;
    return static_cast<u16>(tmp >> 1);
}

/// Calculate an integer average of two unsigned 8-bit values, rounded up (C implementation)
FL_ALWAYS_INLINE u8 avg8r(u8 i, u8 j) {
    return (i + j + 1) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values, rounded up (C implementation)
FL_ALWAYS_INLINE u16 avg16r(u16 i, u16 j) {
    u32 tmp = i;
    tmp += j;
    tmp += 1;
    return static_cast<u16>(tmp >> 1);
}

/// Calculate an integer average of two signed 7-bit integers (C implementation)
FL_ALWAYS_INLINE i8 avg7(i8 i, i8 j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// Calculate an integer average of two signed 15-bit integers (C implementation)
FL_ALWAYS_INLINE i16 avg15(i16 i, i16 j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// 8x8 bit multiplication, with 8-bit result (C implementation)
FL_ALWAYS_INLINE u8 mul8(u8 i, u8 j) {
    return ((int)i * (int)(j)) & 0xFF;
}

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (C implementation)
FL_ALWAYS_INLINE u8 qmul8(u8 i, u8 j) {
    unsigned p = (unsigned)i * (unsigned)j;
    if (p > 255)
        p = 255;
    return p;
}

/// Take the absolute value of a signed 8-bit int8_t (C implementation)
FL_ALWAYS_INLINE i8 abs8(i8 i) {
    if (i < 0)
        i = -i;
    return i;
}

/// Blend a variable proportion of one byte to another - 8-bit precision (C implementation)
/// Uses Option 1: result = ((a << 8) + (b - a) * M + 0x80) >> 8
/// This provides proper rounding with minimal memory overhead
LIB8STATIC u8 blend8_8bit(u8 a, u8 b, u8 amountOfB) {
    u16 partial;

    // Calculate: (a * 256 + (b - a) * amountOfB + 128) / 256
    // The +128 (0x80) provides proper rounding instead of truncation
    partial = (a << 8);              // a * 256
    partial += (b * amountOfB);       // + b * amountOfB
    partial -= (a * amountOfB);       // - a * amountOfB = a*256 + (b-a)*amountOfB
    partial += 0x80;                  // + 128 for rounding

    return partial >> 8;
}

/// Blend a variable proportion of one byte to another - 16-bit precision (C implementation)
/// Uses Option 2: result = ((a << 16) + (b - a) * M * 257 + 0x8000) >> 16
/// This provides higher accuracy by using 16-bit intermediate values
/// The 257 multiplier (0x101) maps the 0-255 range to a more accurate 0-65535 range
LIB8STATIC u8 blend8_16bit(u8 a, u8 b, u8 amountOfB) {
    u32 partial;
    i16 delta = (i16)b - (i16)a;

    // Calculate: (a * 65536 + (b - a) * amountOfB * 257 + 32768) / 65536
    // The *257 (0x101) scales the blend factor to use the full 16-bit range
    // The +32768 (0x8000) provides proper rounding
    partial = ((u32)a << 16);           // a * 65536
    partial += (u32)delta * amountOfB * 257;  // + (b-a) * amountOfB * 257
    partial += 0x8000;                       // + 32768 for rounding

    return partial >> 16;
}

/// Blend a variable proportion of one byte to another (C implementation)
/// Automatically selects between 8-bit and 16-bit precision based on available memory
#if (SKETCH_HAS_LOTS_OF_MEMORY)
LIB8STATIC u8 blend8(u8 a, u8 b, u8 amountOfB) {
    return blend8_16bit(a, b, amountOfB);
}
#else
LIB8STATIC u8 blend8(u8 a, u8 b, u8 amountOfB) {
    return blend8_8bit(a, b, amountOfB);
}
#endif

/// Calculate the remainder of one unsigned 8-bit
/// value divided by another, aka A % M. (C implementation)
/// Implemented by repeated subtraction, which is
/// very compact, and very fast if A is "probably"
/// less than M.  If A is a large multiple of M,
/// the loop has to execute multiple times.
/// @param a dividend byte
/// @param m divisor byte
/// @returns remainder of a / m (i.e. a % m)
FL_ALWAYS_INLINE u8 mod8(u8 a, u8 m) {
    while (a >= m)
        a -= m;
    return a;
}

/// Add two numbers, and calculate the modulo
/// of the sum and a third number, M. (C implementation)
/// In other words, it returns (A+B) % M.
/// It is designed as a compact mechanism for
/// incrementing a "mode" switch and wrapping
/// around back to "mode 0" when the switch
/// goes past the end of the available range.
/// e.g. if you have seven modes, this switches
/// to the next one and wraps around if needed:
///   @code{.cpp}
///   mode = addmod8( mode, 1, 7);
///   @endcode
/// @param a dividend byte
/// @param b value to add to the dividend
/// @param m divisor byte
/// @returns remainder of (a + b) / m
LIB8STATIC u8 addmod8(u8 a, u8 b, u8 m) {
    a += b;
    while (a >= m)
        a -= m;
    return a;
}

/// Subtract two numbers, and calculate the modulo
/// of the difference and a third number, M. (C implementation)
/// In other words, it returns (A-B) % M.
/// It is designed as a compact mechanism for
/// decrementing a "mode" switch and wrapping
/// around back to "mode 0" when the switch
/// goes past the start of the available range.
/// e.g. if you have seven modes, this switches
/// to the previous one and wraps around if needed:
///   @code{.cpp}
///   mode = submod8( mode, 1, 7);
///   @endcode
/// @param a dividend byte
/// @param b value to subtract from the dividend
/// @param m divisor byte
/// @returns remainder of (a - b) / m
LIB8STATIC u8 submod8(u8 a, u8 b, u8 m) {
    a -= b;
    while (a >= m)
        a -= m;
    return a;
}

/// @} Math_C

}  // namespace fl
