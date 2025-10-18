#pragma once

#include "fl/stdint.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION

/// @file trig8_c.h
/// C language implementations of trigonometry functions.
/// This file contains portable C versions of functions from trig8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Trig_C C Trigonometry Implementations
/// Portable C implementations of sin and cos approximations
/// @{

/// Platform-independent alias of the fast sin implementation
#define sin16 sin16_C

/// Fast 16-bit approximation of sin(x) (C implementation)
LIB8STATIC int16_t sin16_C(uint16_t theta) {
    static const uint16_t base[] = {0,     6393,  12539, 18204,
                                    23170, 27245, 30273, 32137};
    static const uint8_t slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

    uint16_t offset = (theta & 0x3FFF) >> 3; // 0..2047
    if (theta & 0x4000)
        offset = 2047 - offset;

    uint8_t section = offset / 256; // 0..7
    uint16_t b = base[section];
    uint8_t m = slope[section];

    uint8_t secoffset8 = (uint8_t)(offset) / 2;

    uint16_t mx = m * secoffset8;
    int16_t y = mx + b;

    if (theta & 0x8000)
        y = -y;

    return y;
}

/// Fast 16-bit approximation of cos(x) (calls sin16)
LIB8STATIC int16_t cos16(uint16_t theta) { return sin16(theta + 16384); }

/// Pre-calculated lookup table used in sin8() and cos8() functions
const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

/// Platform-independent alias of the fast sin implementation
#define sin8 sin8_C

/// Fast 8-bit approximation of sin(x) (C implementation)
LIB8STATIC uint8_t sin8_C(uint8_t theta) {
    uint8_t offset = theta;
    if (theta & 0x40) {
        offset = (uint8_t)255 - offset;
    }
    offset &= 0x3F; // 0..63

    uint8_t secoffset = offset & 0x0F; // 0..15
    if (theta & 0x40)
        ++secoffset;

    uint8_t section = offset >> 4; // 0..3
    uint8_t s2 = section * 2;
    const uint8_t *p = b_m16_interleave;
    p += s2;
    uint8_t b = *p;
    ++p;
    uint8_t m16 = *p;

    uint8_t mx = (m16 * secoffset) >> 4;

    int8_t y = mx + b;
    if (theta & 0x80)
        y = -y;

    y += 128;

    return y;
}

/// Fast 8-bit approximation of cos(x) (calls sin8)
LIB8STATIC uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

/// @} Trig_C
/// @} lib8tion

FL_DISABLE_WARNING_POP
