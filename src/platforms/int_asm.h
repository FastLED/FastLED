#pragma once

/// @file platforms/int_asm.h
/// @brief Platform integer-arithmetic primitives whose *instruction shape*
///        matters, not just their value.
///
/// Dispatch trampoline in the same spirit as platforms/int.h. It is flat today
/// -- every target uses the portable definitions below -- because there is one
/// consumer (the fixed-point MP3 decoder) and three primitives. The shape is
/// here so that a target needing its own spelling is a local `#elif` rather
/// than a hunt through the decoder.
///
/// ## Why these exist at all
///
/// `mulshift32` compiles to a single `mulh` on RV32IM and a single `smull` on
/// ARMv7-M -- but only while the compiler recognises the idiom. That
/// recognition is not a contract: it varies with compiler, version and
/// optimisation level, and when it lapses it lapses *silently*. The expression
/// still computes the right answer, just via a full 32x32->64 multiply, a
/// 64-bit shift and a narrowing -- measured at 40 instructions against 2 on
/// riscv32-esp-elf-g++ -Os. A 20x regression with no diagnostic is not
/// something to leave to a peephole pass, so the idiom lives in one named place
/// with a codegen test pinning it (ci/tests/test_int_asm_codegen.py).
///
/// These are FASTLED_FORCE_INLINE rather than plain `inline` deliberately. A
/// macro always expands; an inline function is a request, and under
/// `-fno-inline` -- which the operation audit builds with -- a plain inline one
/// goes out of line and the call overhead swamps the two-instruction body.
/// `__attribute__((always_inline))` gets the macro's guarantee with a
/// function's type safety and namespacing.
///
/// Namespaced `int_asm` rather than `asm` because `asm` is a reserved keyword.

#include "fl/stl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {
namespace math {
namespace int_asm {

/// High half of a signed 32x32 product: `(i32)(((i64)x * y) >> 32)`.
///
/// Full precision in; only the bits below 2^-32 of the exact product are
/// discarded. This is NOT `(x >> 16) * (y >> 16)`, which additionally throws
/// away the cross terms `(xh*yl + xl*yh) / 2^16` and with them roughly sixteen
/// bits -- measured at a mean absolute error of 12,128 against a result of
/// magnitude 2^30. That distinction is why FastLED#4108's 16-bit experiment
/// scored 44.37 dB, fifteen below the ISO floor, while the Helix decoder holds
/// 102.87 dB using this operation throughout.
///
/// Truncates toward negative infinity. Callers needing round-to-nearest add the
/// rounding term themselves; `mulh` has no free rounding.
FASTLED_FORCE_INLINE i32 mulshift32(i32 x, i32 y) FL_NO_EXCEPT {
    return (i32)(((i64)x * (i64)y) >> 32);
}

/// `(x * y + 2^(shift-1)) >> shift`, narrowed to 32 bits with truncation.
///
/// The fixed-point workhorse: multiply two Q-format values and renormalise,
/// rounding half toward +infinity. `shift` must be a compile-time constant at
/// the call site or the shift becomes a runtime 64-bit variable shift, which is
/// the difference between 10 instructions and 42 on riscv32.
///
/// Spelled out here rather than left as C in a caller because the lowering
/// matters: on riscv32 -Os this is `mul`, `mulh`, a carry-propagating add for
/// the rounding term, and a constant 64-bit shift -- ten instructions. Written
/// inline at a call site the same arithmetic is at the compiler's discretion,
/// and it has already been observed emitting a 42-instruction out-of-line body
/// when `shift` was a parameter rather than a literal.
///
/// **Does not saturate.** The product of two int32 values can exceed int32 after
/// a shift of less than 31, so a caller that can be fed hostile input and needs
/// a bounded result must clamp. Measured on the FastLED corpus that is almost
/// never anyone: across the 83 ISO conformance vectors this narrowing would
/// have clamped 268 times in 40,613,225 calls, every one of them from a single
/// malformed stream (`l3-nonstandard-big-iscf`), and zero times on real music
/// in 21,067,559 calls. Saturating unconditionally costs 15 of 25 instructions
/// on riscv32 for that.
template <int Shift>
FASTLED_FORCE_INLINE i32 mul_shift_round32(i32 x, i32 y) FL_NO_EXCEPT {
    const i64 product = (i64)x * (i64)y + ((i64)1 << (Shift - 1));
    return (i32)(product >> Shift);
}

/// Signed 32-bit add/subtract with defined wraparound.
///
/// Signed overflow is undefined in C; unsigned is specified to wrap. The round
/// trip through `u32` gives the wrapping the hardware does anyway, without
/// telling the optimiser it may assume the overflow cannot happen. One
/// instruction everywhere. See FastLED#4133, where widening to 64 bits instead
/// cost +78% on armv7m, riscv32 and i386 and exactly nothing on x86-64 -- which
/// is also why a 64-bit host cannot be the authority on this file.
FASTLED_FORCE_INLINE i32 wrap_add32(i32 x, i32 y) FL_NO_EXCEPT {
    return (i32)((u32)x + (u32)y);
}

FASTLED_FORCE_INLINE i32 wrap_sub32(i32 x, i32 y) FL_NO_EXCEPT {
    return (i32)((u32)x - (u32)y);
}

} // namespace int_asm
} // namespace math
} // namespace platforms
} // namespace fl
