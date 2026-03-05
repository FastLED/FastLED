#pragma once

// Integer square root utilities for fixed-point types.
// Bit-by-bit algorithm — no float, no division.
// Uses recursive helpers for C++11 constexpr compatibility.

#include "fl/int.h"

namespace fl {

// --- 64-bit helpers ---

constexpr inline u64 _isqrt64_start(u64 x, u64 bit) {
    return (bit == 0 || bit <= x) ? bit : _isqrt64_start(x, bit >> 2);
}

constexpr inline u32 _isqrt64_step(u64 x, u64 result, u64 bit) {
    return bit == 0
        ? static_cast<u32>(result)
        : (x >= result + bit)
            ? _isqrt64_step(x - (result + bit), (result >> 1) + bit, bit >> 2)
            : _isqrt64_step(x, result >> 1, bit >> 2);
}

// Integer square root of a 64-bit value. Returns floor(sqrt(x)).
constexpr inline u32 isqrt64(u64 x) {
    return x == 0 ? u32(0)
                  : _isqrt64_step(x, 0, _isqrt64_start(x, u64(1) << 62));
}

// --- 32-bit helpers ---

constexpr inline u32 _isqrt32_start(u32 x, u32 bit) {
    return (bit == 0 || bit <= x) ? bit : _isqrt32_start(x, bit >> 2);
}

constexpr inline u16 _isqrt32_step(u32 x, u32 result, u32 bit) {
    return bit == 0
        ? static_cast<u16>(result)
        : (x >= result + bit)
            ? _isqrt32_step(x - (result + bit), (result >> 1) + bit, bit >> 2)
            : _isqrt32_step(x, result >> 1, bit >> 2);
}

// Integer square root of a 32-bit value. Returns floor(sqrt(x)).
constexpr inline u16 isqrt32(u32 x) {
    return x == 0 ? u16(0)
                  : _isqrt32_step(x, 0, _isqrt32_start(x, u32(1) << 30));
}

} // namespace fl
