/// @file transpose8x1_noinline.cpp
/// Defines the 8x1 transposition function

#include "fl/stdint.h"

#include "transpose8x1_noinline.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

void transpose8x1_noinline(unsigned char *A, unsigned char *B) {
    uint32_t x, y, t;

    // Load the array and pack it into x and y.
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);

    // pre-transform x
    t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
    t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

    // pre-transform y
    t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
    t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

    // final transform
    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    *((uint32_t*)B) = y;
    *((uint32_t*)(B+4)) = x;
}

#pragma GCC diagnostic pop
