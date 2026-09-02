#pragma once

/// @file fl/math/int_asm.h
/// @brief Integer arithmetic with a guaranteed instruction shape.
///
/// Public entry point for arithmetic where which instructions get emitted
/// matters as much as the value computed -- currently the fixed-point MP3
/// decoder's inner loops, where a recognised idiom against an unrecognised one
/// measured 2 instructions against 40 on riscv32.
///
/// | function | meaning | riscv32 | armv7-m |
/// |---|---|---|---|
/// | `mulshift32(x,y)` | high half of a signed 32x32 product | `mulh` | `smull` |
/// | `mul_shift_round32<S>(x,y)` | `(x*y + 2^(S-1)) >> S`, truncating narrow | 10 insns | -- |
/// | `wrap_add32(x,y)` | 32-bit add, defined wraparound | `add` | `add` |
/// | `wrap_sub32(x,y)` | 32-bit subtract, defined wraparound | `sub` | `sub` |
///
/// `mulshift32` truncates rather than rounding, and discards nothing above
/// 2^-32 of the exact product. It is emphatically not operand truncation; see
/// platforms/int_asm.h for the measurement that separates them.
///
/// Pinned by ci/tests/test_int_asm_codegen.py, which fails if any of these
/// stops compiling to a single arithmetic instruction on a cross-target.

// platforms/int_asm.h brings fl/stl/int.h, compiler_control.h and noexcept.h.
// Including them again here is not free: fl/stl/int.h has to be reached through
// the platform's type dance on some targets, and pulling it in directly ahead
// of that breaks the riscv32 build with a conflicting size_t typedef.
#include "platforms/int_asm.h"  // IWYU pragma: export

namespace fl {
namespace math {

/// @copydoc fl::platforms::math::int_asm::mulshift32
FASTLED_FORCE_INLINE i32 mulshift32(i32 x, i32 y) FL_NO_EXCEPT {
    return platforms::math::int_asm::mulshift32(x, y);
}

/// @copydoc fl::platforms::math::int_asm::mul_shift_round32
template <int Shift>
FASTLED_FORCE_INLINE i32 mul_shift_round32(i32 x, i32 y) FL_NO_EXCEPT {
    return platforms::math::int_asm::mul_shift_round32<Shift>(x, y);
}

/// @copydoc fl::platforms::math::int_asm::wrap_add32
FASTLED_FORCE_INLINE i32 wrap_add32(i32 x, i32 y) FL_NO_EXCEPT {
    return platforms::math::int_asm::wrap_add32(x, y);
}

/// @copydoc fl::platforms::math::int_asm::wrap_sub32
FASTLED_FORCE_INLINE i32 wrap_sub32(i32 x, i32 y) FL_NO_EXCEPT {
    return platforms::math::int_asm::wrap_sub32(x, y);
}

} // namespace math
} // namespace fl
