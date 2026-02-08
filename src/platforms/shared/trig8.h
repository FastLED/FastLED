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
LIB8STATIC i16 sin16_C(u16 theta) {
    static const u16 base[] = {0,     6393,  12539, 18204,
                                    23170, 27245, 30273, 32137};
    static const u8 slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

    u16 offset = (theta & 0x3FFF) >> 3; // 0..2047
    if (theta & 0x4000)
        offset = 2047 - offset;

    u8 section = offset / 256; // 0..7
    u16 b = base[section];
    u8 m = slope[section];

    u8 secoffset8 = (u8)(offset) / 2;

    u16 mx = m * secoffset8;
    i16 y = mx + b;

    if (theta & 0x8000)
        y = -y;

    return y;
}

/// Platform-independent alias of the fast sin implementation
LIB8STATIC i16 sin16(u16 theta) { return sin16_C(theta); }

/// Fast 16-bit approximation of cos(x) (calls sin16)
LIB8STATIC i16 cos16(u16 theta) { return sin16(theta + 16384); }

/// Pre-calculated lookup table used in sin8() and cos8() functions
const u8 b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

/// Fast 8-bit approximation of sin(x) (C implementation)
LIB8STATIC u8 sin8_C(u8 theta) {
    u8 offset = theta;
    if (theta & 0x40) {
        offset = (u8)255 - offset;
    }
    offset &= 0x3F; // 0..63

    u8 secoffset = offset & 0x0F; // 0..15
    if (theta & 0x40)
        ++secoffset;

    u8 section = offset >> 4; // 0..3
    u8 s2 = section * 2;
    const u8 *p = b_m16_interleave;
    p += s2;
    u8 b = *p;
    ++p;
    u8 m16 = *p;

    u8 mx = (m16 * secoffset) >> 4;

    i8 y = mx + b;
    if (theta & 0x80)
        y = -y;

    y += 128;

    return y;
}

/// Platform-independent alias of the fast sin implementation
LIB8STATIC u8 sin8(u8 theta) { return sin8_C(theta); }

/// Fast 8-bit approximation of cos(x) (calls sin8)
LIB8STATIC u8 cos8(u8 theta) { return sin8(theta + 64); }

/// @} Trig_C

}  // namespace fl
