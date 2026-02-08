#pragma once

// Integer square root utilities for fixed-point types.
// Bit-by-bit algorithm — no float, no division.

#include "fl/int.h"

namespace fl {

// Integer square root of a 64-bit value. Returns floor(sqrt(x)).
inline u32 isqrt64(u64 x) {
    if (x == 0) return 0;
    u64 result = 0;
    u64 bit = u64(1) << 62;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= result + bit) {
            x -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<u32>(result);
}

// Integer square root of a 32-bit value. Returns floor(sqrt(x)).
inline u16 isqrt32(u32 x) {
    if (x == 0) return 0;
    u32 result = 0;
    u32 bit = u32(1) << 30;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= result + bit) {
            x -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<u16>(result);
}

} // namespace fl
