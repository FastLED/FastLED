#pragma once

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
LIB8STATIC int16_t sin16_avr(uint16_t theta) {
    static const uint8_t data[] = {
        0,           0,           49, 0, 6393 % 256,  6393 / 256,  48, 0,
        12539 % 256, 12539 / 256, 44, 0, 18204 % 256, 18204 / 256, 38, 0,
        23170 % 256, 23170 / 256, 31, 0, 27245 % 256, 27245 / 256, 23, 0,
        30273 % 256, 30273 / 256, 14, 0, 32137 % 256, 32137 / 256, 4 /*,0*/};

    uint16_t offset = (theta & 0x3FFF);

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

    uint8_t sectionX4;
    sectionX4 = offset / 256;
    sectionX4 *= 4;

    uint8_t m;

    union {
        uint16_t b;
        struct {
            uint8_t blo;
            uint8_t bhi;
        };
    } u;

    // in effect u.b = blo + (256 * bhi);
    u.blo = data[sectionX4];
    u.bhi = data[sectionX4 + 1];
    m = data[sectionX4 + 2];

    uint8_t secoffset8 = (uint8_t)(offset) / 2;

    uint16_t mx = m * secoffset8;

    int16_t y = mx + u.b;
    if (theta & 0x8000)
        y = -y;

    return y;
}

/// Pre-calculated lookup table used in sin8() and cos8() functions
const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

/// Fast 8-bit approximation of sin(x) (AVR implementation)
#ifndef FL_IS_AVR_ATTINY
LIB8STATIC uint8_t sin8_avr(uint8_t theta) {
    uint8_t offset = theta;

    asm volatile("sbrc %[theta],6         \n\t"
                 "com  %[offset]           \n\t"
                 : [theta] "+r"(theta), [offset] "+r"(offset));

    offset &= 0x3F; // 0..63

    uint8_t secoffset = offset & 0x0F; // 0..15
    if (theta & 0x40)
        ++secoffset;

    uint8_t m16;
    uint8_t b;

    uint8_t section = offset >> 4; // 0..3
    uint8_t s2 = section * 2;

    const uint8_t *p = b_m16_interleave;
    p += s2;
    b = *p;
    ++p;
    m16 = *p;

    uint8_t mx;
    uint8_t xr1;
    // Create mask registers outside asm to reduce register pressure
    uint8_t mask_0F = 0x0F;
    uint8_t mask_F0 = 0xF0;
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

    int8_t y = mx + b;
    if (theta & 0x80)
        y = -y;

    y += 128;

    return y;
}
#else
// ATtiny fallback - C implementation (no mul instruction)
LIB8STATIC uint8_t sin8_avr(uint8_t theta) {
    static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
    uint8_t offset = theta;
    if (theta & 0x40) offset = ~offset;
    offset &= 0x3F;
    uint8_t secoffset = (offset & 0x0F);
    if (theta & 0x40) ++secoffset;
    uint8_t section = offset >> 4;
    uint8_t s2 = section * 2;
    const uint8_t *p = b_m16_interleave + s2;
    uint8_t b = *p++;
    uint8_t m16 = *p;
    // C version of multiply without hardware mul
    uint16_t mx_full = (uint16_t)m16 * secoffset;
    uint8_t mx = mx_full >> 4;
    int8_t y = mx + b;
    if (theta & 0x80) y = -y;
    return y + 128;
}
#endif // !defined(FL_IS_AVR_ATTINY)

/// Platform-independent alias of the fast sin implementation
LIB8STATIC int16_t sin16(uint16_t theta) { return sin16_avr(theta); }

/// Fast 16-bit approximation of cos(x) (calls sin16)
LIB8STATIC int16_t cos16(uint16_t theta) { return sin16(theta + 16384); }

/// Platform-independent alias of the fast sin implementation
LIB8STATIC uint8_t sin8(uint8_t theta) { return sin8_avr(theta); }

/// Fast 8-bit approximation of cos(x) (calls sin8)
LIB8STATIC uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

/// @} Trig_AVR

}  // namespace fl
