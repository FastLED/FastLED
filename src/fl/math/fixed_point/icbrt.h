#pragma once

// Integer cube root for fixed-point types.
// Bit-by-bit algorithm — no float, no division, no multiplication by a
// non-constant.  Companion to isqrt.h; see that file for why the algorithm
// is expressed as tail recursion (C++11 constexpr forbids loops) and why
// FL_OPTIMIZE_FUNCTION is required to keep low optimization levels from
// emitting a stack frame per iteration.
//
// The recurrence carries the running root `y` and its square `y2` so that
// each step needs only shifts and adds:
//
//     y  <- 2y            y2 <- 4y2
//     B  =  3(y2 + y) + 1
//     if (x >> s) >= B  then  x -= B << s,  y += 1,  y2 += 2y + 1
//
// The comparison is written `(x >> s) >= B` rather than `x >= (B << s)`
// because `B << s` overflows u64 on the first steps of a large input, while
// the shifted-right form is exact for integers and never does.  `B << s` is
// then only evaluated on the branch where it is known to be <= x.
//
// 22 steps (s = 63, 60, ... 0) cover the whole u64 domain; the result of
// icbrt64 is therefore always below 2^22.

#include "fl/stl/compiler_control.h" // FL_OPTIMIZE_FUNCTION
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

FL_OPTIMIZE_FUNCTION constexpr inline u32 _icbrt64_step(u64 x, u64 y, u64 y2, int s) FL_NO_EXCEPT {
    return s < 0
        ? static_cast<u32>(y)
        : ((x >> s) >= 3 * ((y2 << 2) + (y << 1)) + 1)
            ? _icbrt64_step(x - ((3 * ((y2 << 2) + (y << 1)) + 1) << s),
                            (y << 1) + 1,
                            (y2 << 2) + (y << 2) + 1,
                            s - 3)
            : _icbrt64_step(x, y << 1, y2 << 2, s - 3);
}

// Largest u32 whose cube does not exceed x.
FL_OPTIMIZE_FUNCTION constexpr inline u32 icbrt64(u64 x) FL_NO_EXCEPT {
    return _icbrt64_step(x, 0, 0, 63);
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
