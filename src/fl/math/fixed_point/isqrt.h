#pragma once

// Integer square root for fixed-point types.
// Bit-by-bit algorithm — no float, no division.
//
// C++11 constexpr functions cannot contain loops, so the algorithm is
// expressed as tail recursion.  In C++14+ this would just be a loop,
// but FastLED targets C++11 for broad platform support (AVR, ESP32,
// etc.).  Without intervention, compilers at low optimization levels
// (-O0, -O1) emit actual recursive calls — stack frames per iteration.
//
// FL_OPTIMIZE_FUNCTION forces per-function optimization to ensure the
// tail recursion is converted to forward iteration (loops) regardless
// of the global optimization level:
//   GCC:   __attribute__((optimize("O3"))) — verified to produce
//          tight iterative loops even at -O0.
//   Clang: __attribute__((hot)) — best-effort hint; Clang converts
//          recursion to loops at -O2+ but not at -O0/-O1.  This only
//          affects the host test build; all embedded targets use GCC.

#include "fl/stl/compiler_control.h" // FL_OPTIMIZE_FUNCTION
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

FL_OPTIMIZE_FUNCTION constexpr inline u64 _isqrt64_start(u64 x, u64 bit) FL_NOEXCEPT {
    return (bit == 0 || bit <= x) ? bit : _isqrt64_start(x, bit >> 2);
}

FL_OPTIMIZE_FUNCTION constexpr inline u32 _isqrt64_step(u64 x, u64 result, u64 bit) FL_NOEXCEPT {
    return bit == 0
        ? static_cast<u32>(result)
        : (x >= result + bit)
            ? _isqrt64_step(x - (result + bit), (result >> 1) + bit, bit >> 2)
            : _isqrt64_step(x, result >> 1, bit >> 2);
}

FL_OPTIMIZE_FUNCTION constexpr inline u32 _isqrt32_start(u32 x, u32 bit) FL_NOEXCEPT {
    return (bit == 0 || bit <= x) ? bit : _isqrt32_start(x, bit >> 2);
}

FL_OPTIMIZE_FUNCTION constexpr inline u16 _isqrt32_step(u32 x, u32 result, u32 bit) FL_NOEXCEPT {
    return bit == 0
        ? static_cast<u16>(result)
        : (x >= result + bit)
            ? _isqrt32_step(x - (result + bit), (result >> 1) + bit, bit >> 2)
            : _isqrt32_step(x, result >> 1, bit >> 2);
}

FL_OPTIMIZE_FUNCTION constexpr inline u16 isqrt32(u32 x) FL_NOEXCEPT {
    return x == 0 ? u16(0)
                  : _isqrt32_step(x, 0, _isqrt32_start(x, u32(1) << 30));
}

FL_OPTIMIZE_FUNCTION constexpr inline u32 isqrt64(u64 x) FL_NOEXCEPT {
    return x == 0 ? u32(0)
                  : _isqrt64_step(x, 0, _isqrt64_start(x, u64(1) << 62));
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
