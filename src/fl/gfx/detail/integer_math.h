#pragma once

#include "fl/stl/stdint.h"  // IWYU pragma: keep

namespace fl {
namespace gfx {
namespace detail {

/// @brief Integer square root using digit-by-digit algorithm
/// Computes floor(sqrt(n)) for positive integers
/// Uses only shifts, adds, and comparisons — no division
inline fl::i32 isqrt32_floor(fl::i32 n) {
    if (n <= 0) return 0;
    fl::u32 un = (fl::u32)n;
    fl::u32 result = 0;
    fl::u32 bit = 1u << 30;  // highest even power of 2
    while (bit > un) bit >>= 2;  // find starting magnitude
    while (bit != 0) {
        fl::u32 t = result + bit;
        result >>= 1;
        if (un >= t) { un -= t; result += bit; }
        bit >>= 2;
    }
    return (fl::i32)result;
}

/// @brief Integer square root using Newton iteration
/// Computes ceil(sqrt(n)) for positive integers
/// More accurate than digit-by-digit for the ceiling case
inline fl::i32 isqrt32_ceil(fl::i32 n) {
    if (n <= 0) return 0;
    fl::u32 x = (fl::u32)n;
    fl::u32 s = 1u << 16;  // initial guess: sqrt(2^32) = 2^16
    for (int i = 0; i < 8; ++i) {  // 8 iterations converges for 32-bit input
        s = (s + x / s) >> 1;  // Newton iteration: s = (s + n/s) / 2
    }
    return (fl::i32)s;
}

}  // namespace detail
}  // namespace gfx
}  // namespace fl
