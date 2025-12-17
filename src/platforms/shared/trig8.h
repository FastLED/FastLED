#pragma once

#include "fl/stl/stdint.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

namespace fl {

// Note: This file opens its own namespace fl block

/// @file trig8.h
/// C language implementations of trigonometry functions.
/// This file contains portable C versions of functions from trig8.h

/// @defgroup Trig_C C Trigonometry Implementations
/// Portable C implementations of sin and cos approximations
/// @{

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

/// Platform-independent alias of the fast sin implementation
LIB8STATIC int16_t sin16(uint16_t theta) { return sin16_C(theta); }

/// Fast 16-bit approximation of cos(x) (calls sin16)
LIB8STATIC int16_t cos16(uint16_t theta) { return sin16(theta + 16384); }

/// Pre-calculated lookup table used in sin8() and cos8() functions
const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

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

/// Platform-independent alias of the fast sin implementation
LIB8STATIC uint8_t sin8(uint8_t theta) { return sin8_C(theta); }

/// Fast 8-bit approximation of cos(x) (calls sin8)
LIB8STATIC uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

/// @} Trig_C

}  // namespace fl
