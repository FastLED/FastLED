#pragma once

// IWYU pragma: private

#include "platforms/avr/is_avr.h"
#include "fl/stl/stdint.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

namespace fl {

// Note: This file opens its own namespace fl block

/// @file trig8.h
/// AVR assembly language implementations of trigonometry functions.
/// This file contains optimized AVR-specific versions of functions from trig8.h

/// @defgroup Trig_AVR AVR Trigonometry Implementations
/// AVR-optimized sin and cos approximations
/// @{

/// Fast 16-bit approximation of sin(x) (AVR implementation)
LIB8STATIC i16 sin16_avr(u16 theta) {
    static const u8 data[] = {
        0,           0,           49, 0, 6393 % 256,  6393 / 256,  48, 0,
        12539 % 256, 12539 / 256, 44, 0, 18204 % 256, 18204 / 256, 38, 0,
        23170 % 256, 23170 / 256, 31, 0, 27245 % 256, 27245 / 256, 23, 0,
        30273 % 256, 30273 / 256, 14, 0, 32137 % 256, 32137 / 256, 4 /*,0*/};

    u16 offset = (theta & 0x3FFF);

    // AVR doesn't have a multi-bit shift instruction,
    // so if we say "offset >>= 3", gcc makes a tiny loop.
    // Inserting empty volatile statements between each
    // bit shift forces gcc to unroll the loop.
    offset >>= 1; // 0..8191
    asm volatile("");
    offset >>= 1; // 0..4095
    asm volatile("");
    offset >>= 1; // 0..2047

    if (theta & 0x4000)
        offset = 2047 - offset;

    u8 sectionX4;
    sectionX4 = offset / 256;
    sectionX4 *= 4;

    u8 m;

    union {
        u16 b;
        struct {
            u8 blo;
            u8 bhi;
        };
    } u;

    // in effect u.b = blo + (256 * bhi);
    u.blo = data[sectionX4];
    u.bhi = data[sectionX4 + 1];
    m = data[sectionX4 + 2];

    u8 secoffset8 = (u8)(offset) / 2;

    u16 mx = m * secoffset8;

    i16 y = mx + u.b;
    if (theta & 0x8000)
        y = -y;

    return y;
}

/// Pre-calculated lookup table used in sin8() and cos8() functions
const u8 b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

/// Fast 8-bit approximation of sin(x) (AVR implementation)
#ifndef FL_IS_AVR_ATTINY
LIB8STATIC u8 sin8_avr(u8 theta) {
    u8 offset = theta;

    asm volatile("sbrc %[theta],6         \n\t"
                 "com  %[offset]           \n\t"
                 : [theta] "+r"(theta), [offset] "+r"(offset));

    offset &= 0x3F; // 0..63

    u8 secoffset = offset & 0x0F; // 0..15
    if (theta & 0x40)
        ++secoffset;

    u8 m16;
    u8 b;

    u8 section = offset >> 4; // 0..3
    u8 s2 = section * 2;

    const u8 *p = b_m16_interleave;
    p += s2;
    b = *p;
    ++p;
    m16 = *p;

    u8 mx;
    u8 xr1;
    // Create mask registers outside asm to reduce register pressure
    u8 mask_0F = 0x0F;
    u8 mask_F0 = 0xF0;
    asm volatile("mul %[m16],%[secoffset]   \n\t"
                 "mov %[mx],r0              \n\t"
                 "mov %[xr1],r1             \n\t"
                 "eor  r1, r1               \n\t"
                 "swap %[mx]                \n\t"
                 "and %[mx],%[mask_0F]      \n\t"
                 "swap %[xr1]               \n\t"
                 "and %[xr1],%[mask_F0]     \n\t"
                 "or   %[mx], %[xr1]        \n\t"
                 : [mx] "=r"(mx), [xr1] "=r"(xr1), [mask_0F] "+r"(mask_0F), [mask_F0] "+r"(mask_F0)
                 : [m16] "r"(m16), [secoffset] "r"(secoffset)
                 : "r0", "r1");

    i8 y = mx + b;
    if (theta & 0x80)
        y = -y;

    y += 128;

    return y;
}
#else
// ATtiny fallback - C implementation (no mul instruction)
LIB8STATIC u8 sin8_avr(u8 theta) {
    static const u8 b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
    u8 offset = theta;
    if (theta & 0x40) offset = ~offset;
    offset &= 0x3F;
    u8 secoffset = (offset & 0x0F);
    if (theta & 0x40) ++secoffset;
    u8 section = offset >> 4;
    u8 s2 = section * 2;
    const u8 *p = b_m16_interleave + s2;
    u8 b = *p++;
    u8 m16 = *p;
    // C version of multiply without hardware mul
    u16 mx_full = (u16)m16 * secoffset;
    u8 mx = mx_full >> 4;
    i8 y = mx + b;
    if (theta & 0x80) y = -y;
    return y + 128;
}
#endif // !defined(FL_IS_AVR_ATTINY)

/// Platform-independent alias of the fast sin implementation
LIB8STATIC i16 sin16(u16 theta) { return sin16_avr(theta); }

/// Fast 16-bit approximation of cos(x) (calls sin16)
LIB8STATIC i16 cos16(u16 theta) { return sin16(theta + 16384); }

/// Platform-independent alias of the fast sin implementation
LIB8STATIC u8 sin8(u8 theta) { return sin8_avr(theta); }

/// Fast 8-bit approximation of cos(x) (calls sin8)
LIB8STATIC u8 cos8(u8 theta) { return sin8(theta + 64); }

/// @} Trig_AVR

}  // namespace fl
