#pragma once

#include "lib8tion/config.h"
#include "scale8.h"
#include "lib8tion/lib8static.h"
#include "intmap.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

/// @file math8_c.h
/// C language implementations of 8-bit math functions.
/// This file contains portable C versions of functions from math8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Math_C C Math Implementations
/// Portable C implementations of 8-bit math operations
/// @{

/// Add one byte to another, saturating at 0xFF (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t qadd8(uint8_t i, uint8_t j) {
    unsigned int t = i + j;
    if (t > 255)
        t = 255;
    return static_cast<uint8_t>(t);
}

/// Add one byte to another, saturating at 0x7F and -0x80 (C implementation)
LIB8STATIC_ALWAYS_INLINE int8_t qadd7(int8_t i, int8_t j) {
    int16_t t = i + j;
    if (t > 127)
        t = 127;
    else if (t < -128)
        t = -128;
    return static_cast<int8_t>(t);
}

/// Subtract one byte from another, saturating at 0x00 (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t qsub8(uint8_t i, uint8_t j) {
    int t = i - j;
    if (t < 0)
        t = 0;
    return static_cast<uint8_t>(t);
}

/// Add one byte to another, with 8-bit result (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t add8(uint8_t i, uint8_t j) {
    int t = i + j;
    return static_cast<uint8_t>(t);
}

/// Add one byte to two bytes, with 16-bit result (C implementation)
LIB8STATIC_ALWAYS_INLINE uint16_t add8to16(uint8_t i, uint16_t j) {
    uint16_t t = i + j;
    return t;
}

/// Subtract one byte from another, 8-bit result (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t sub8(uint8_t i, uint8_t j) {
    int t = i - j;
    return static_cast<uint8_t>(t);
}

/// Calculate an integer average of two unsigned 8-bit values (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t avg8(uint8_t i, uint8_t j) {
    return (i + j) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values (C implementation)
LIB8STATIC_ALWAYS_INLINE uint16_t avg16(uint16_t i, uint16_t j) {
    uint32_t tmp = i;
    tmp += j;
    return static_cast<uint16_t>(tmp >> 1);
}

/// Calculate an integer average of two unsigned 8-bit values, rounded up (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t avg8r(uint8_t i, uint8_t j) {
    return (i + j + 1) >> 1;
}

/// Calculate an integer average of two unsigned 16-bit values, rounded up (C implementation)
LIB8STATIC_ALWAYS_INLINE uint16_t avg16r(uint16_t i, uint16_t j) {
    uint32_t tmp = i;
    tmp += j;
    tmp += 1;
    return static_cast<uint16_t>(tmp >> 1);
}

/// Calculate an integer average of two signed 7-bit integers (C implementation)
LIB8STATIC_ALWAYS_INLINE int8_t avg7(int8_t i, int8_t j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// Calculate an integer average of two signed 15-bit integers (C implementation)
LIB8STATIC_ALWAYS_INLINE int16_t avg15(int16_t i, int16_t j) {
    return (i >> 1) + (j >> 1) + (i & 0x1);
}

/// 8x8 bit multiplication, with 8-bit result (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
    return ((int)i * (int)(j)) & 0xFF;
}

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (C implementation)
LIB8STATIC_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
    unsigned p = (unsigned)i * (unsigned)j;
    if (p > 255)
        p = 255;
    return p;
}

/// Take the absolute value of a signed 8-bit int8_t (C implementation)
LIB8STATIC_ALWAYS_INLINE int8_t abs8(int8_t i) {
    if (i < 0)
        i = -i;
    return i;
}

/// Blend a variable proportion of one byte to another (C implementation)
#if (FASTLED_BLEND_FIXED == 1)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial;
    uint8_t result;

    partial = (a << 8) | b; // A*256 + B

    // on many platforms this compiles to a single multiply of (B-A) * amountOfB
    partial += (b * amountOfB);
    partial -= (a * amountOfB);

    result = partial >> 8;

    return result;
}
#else
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial;
    uint8_t result;
    uint8_t amountOfA = 255 - amountOfB;

    // on the other hand, this compiles to two multiplies, and gives the "wrong"
    // answer :]
    partial = (a * amountOfA);
    partial += (b * amountOfB);

    result = partial >> 8;

    return result;
}
#endif

/// @} Math_C
/// @} lib8tion

FL_DISABLE_WARNING_POP
