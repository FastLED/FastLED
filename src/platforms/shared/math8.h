#pragma once

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
FL_ALWAYS_INLINE uint8_t qadd8(uint8_t i, uint8_t j) {
    unsigned int t = i + j;
    if (t > 255)
        t = 255;
    return static_cast<uint8_t>(t);
}

/// Add one byte to another, saturating at 0x7F and -0x80 (C implementation)
FL_ALWAYS_INLINE int8_t qadd7(int8_t i, int8_t j) {
    int16_t t = i + j;
    if (t > 127)
        t = 127;
    else if (t < -128)
        t = -128;
    return static_cast<int8_t>(t);
}

/// Subtract one byte from another, saturating at 0x00 (C implementation)
FL_ALWAYS_INLINE uint8_t qsub8(uint8_t i, uint8_t j) {
    int t = i - j;
    if (t < 0)
        t = 0;
    return static_cast<uint8_t>(t);
}

/// Add one byte to another, with 8-bit result (C implementation)
FL_ALWAYS_INLINE uint8_t add8(uint8_t i, uint8_t j) {
    int t = i + j;
    return static_cast<uint8_t>(t);
}

/// Add one byte to two bytes, with 16-bit result (C implementation)
FL_ALWAYS_INLINE uint16_t add8to16(uint8_t i, uint16_t j) {
    uint16_t t = i + j;
    return t;
}

/// Subtract one byte from another, 8-bit result (C implementation)
FL_ALWAYS_INLINE uint8_t sub8(uint8_t i, uint8_t j) {
    int t = i - j;
    return static_cast<uint8_t>(t);
}

/// Calculate an integer average of two unsigned 8-bit values (C implementation)
FL_ALWAYS_INLINE uint8_t avg8(uint8_t i, uint8_t j) {
    return (i + j) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values (C implementation)
FL_ALWAYS_INLINE uint16_t avg16(uint16_t i, uint16_t j) {
    uint32_t tmp = i;
    tmp += j;
    return static_cast<uint16_t>(tmp >> 1);
}

/// Calculate an integer average of two unsigned 8-bit values, rounded up (C implementation)
FL_ALWAYS_INLINE uint8_t avg8r(uint8_t i, uint8_t j) {
    return (i + j + 1) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values, rounded up (C implementation)
FL_ALWAYS_INLINE uint16_t avg16r(uint16_t i, uint16_t j) {
    uint32_t tmp = i;
    tmp += j;
    tmp += 1;
    return static_cast<uint16_t>(tmp >> 1);
}

/// Calculate an integer average of two signed 7-bit integers (C implementation)
FL_ALWAYS_INLINE int8_t avg7(int8_t i, int8_t j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// Calculate an integer average of two signed 15-bit integers (C implementation)
FL_ALWAYS_INLINE int16_t avg15(int16_t i, int16_t j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// 8x8 bit multiplication, with 8-bit result (C implementation)
FL_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
    return ((int)i * (int)(j)) & 0xFF;
}

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (C implementation)
FL_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
    unsigned p = (unsigned)i * (unsigned)j;
    if (p > 255)
        p = 255;
    return p;
}

/// Take the absolute value of a signed 8-bit int8_t (C implementation)
FL_ALWAYS_INLINE int8_t abs8(int8_t i) {
    if (i < 0)
        i = -i;
    return i;
}

/// Blend a variable proportion of one byte to another - 8-bit precision (C implementation)
/// Uses Option 1: result = ((a << 8) + (b - a) * M + 0x80) >> 8
/// This provides proper rounding with minimal memory overhead
LIB8STATIC uint8_t blend8_8bit(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial;

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
LIB8STATIC uint8_t blend8_16bit(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint32_t partial;
    int16_t delta = (int16_t)b - (int16_t)a;

    // Calculate: (a * 65536 + (b - a) * amountOfB * 257 + 32768) / 65536
    // The *257 (0x101) scales the blend factor to use the full 16-bit range
    // The +32768 (0x8000) provides proper rounding
    partial = ((uint32_t)a << 16);           // a * 65536
    partial += (uint32_t)delta * amountOfB * 257;  // + (b-a) * amountOfB * 257
    partial += 0x8000;                       // + 32768 for rounding

    return partial >> 16;
}

/// Blend a variable proportion of one byte to another (C implementation)
/// Automatically selects between 8-bit and 16-bit precision based on available memory
#if (SKETCH_HAS_LOTS_OF_MEMORY)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    return blend8_16bit(a, b, amountOfB);
}
#else
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
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
FL_ALWAYS_INLINE uint8_t mod8(uint8_t a, uint8_t m) {
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
LIB8STATIC uint8_t addmod8(uint8_t a, uint8_t b, uint8_t m) {
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
LIB8STATIC uint8_t submod8(uint8_t a, uint8_t b, uint8_t m) {
    a -= b;
    while (a >= m)
        a -= m;
    return a;
}

/// @} Math_C

}  // namespace fl
