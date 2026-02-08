#pragma once

// Integer square root utilities for fixed-point types.
// Bit-by-bit algorithm — no float, no division.

#include "fl/stl/stdint.h"

namespace fl {

// Integer square root of a 64-bit value. Returns floor(sqrt(x)).
inline uint32_t isqrt64(uint64_t x) {
    if (x == 0) return 0;
    uint64_t result = 0;
    uint64_t bit = uint64_t(1) << 62;
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
    return static_cast<uint32_t>(result);
}

// Integer square root of a 32-bit value. Returns floor(sqrt(x)).
inline uint16_t isqrt32(uint32_t x) {
    if (x == 0) return 0;
    uint32_t result = 0;
    uint32_t bit = uint32_t(1) << 30;
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
    return static_cast<uint16_t>(result);
}

} // namespace fl
