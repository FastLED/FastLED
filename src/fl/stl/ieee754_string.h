#pragma once

///////////////////////////////////////////////////////////////////////////////
// Integer-only string <-> IEEE 754 single-precision codec.
//
// Motivation: on no-FPU ARM (Cortex-M0+ / LPC845 etc.) the libgcc soft-FP
// helpers (__aeabi_d* / __aeabi_f* and their ~7 KB transitive closure) get
// pulled into any link that runs decimal arithmetic through `float` or
// `double` operators. FastLED #3022 (and the gap-tracking issue #3029) ask
// the library to never structurally reach those helpers from the JSON
// parser / serializer or the variant's float-vector storage path.
//
// This codec satisfies that constraint:
//   * Parse: digits feed an integer mantissa, scaled by a precomputed table
//     of `10^k` in 64-bit-mantissa + binary-exponent form. The widening
//     multiply that follows uses only 32x32->64 native ops (no `__aeabi_d*`).
//   * Format: mantissa / exponent are unpacked from the IEEE-754 bit
//     pattern via shifts and masks; decimal digits emerge from integer
//     scaling against the same `10^k` table.
//
// API surface deliberately works on `fl::u32` bit-patterns (not `float`).
// The caller can `fl::bit_cast<float>(bits)` at the API boundary -- that is
// a `memcpy` and never anchors libgcc. Doing the conversion *inside* this
// codec would defeat the point.
//
// Precision target: ~6 significant decimal digits with at-most +/-1 ULP
// drift from `strtof` / `snprintf("%g")` at the edges. Sufficient for
// FastLED's JSON-RPC use case (brightness, ratios, coordinates). Callers
// that need bit-exact round-trip on hosts should use the platform `strtof`
// / `snprintf` directly -- those are *intentionally* not wired through this
// header.
//
// See FastLED #3022 (parent), #3029 (gap), #3027 (superseded gate PR).
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration to avoid pulling fl/stl/string.h here.
class string;

/// @brief Parse a decimal floating-point number into IEEE 754 single-precision bits.
///
/// Accepts the usual decimal syntax: optional sign, integer digits, optional
/// fractional part, optional `e`/`E` exponent. Returns the IEEE 754
/// single-precision bit pattern of the parsed value. Caller bit-casts to
/// `float` via `fl::bit_cast<float>(bits)` if needed.
///
/// No FP arithmetic is performed. The implementation uses only integer
/// shifts, masks, and 32x32->64 widening multiplies -- libgcc soft-FP
/// helpers (`__aeabi_d*`, `__aeabi_f*`) are never reached.
///
/// Failure mode: malformed inputs return `0x00000000` (positive zero) and
/// set `*consumed` to 0. Inputs that overflow IEEE 754 single-precision
/// range (>~3.4e38) clamp to `+inf` / `-inf` bit patterns. Inputs that
/// underflow (>~1.4e-45) clamp to +/-0.
///
/// @param s         Pointer to the decimal text.
/// @param len       Length of the text in bytes (no NUL terminator assumed).
/// @param consumed  If non-null, receives the number of bytes consumed.
///                  Useful for caller-side bookkeeping (e.g. JSON tokenizer).
/// @return          IEEE 754 single-precision bit pattern.
u32 ieee754_parse_decimal(const char* s, fl::size len, fl::size* consumed = nullptr) FL_NOEXCEPT;

/// @brief Format IEEE 754 single-precision bits as decimal text.
///
/// Emits a fixed-point decimal representation with `precision` digits after
/// the point. Negative values emit a leading `-`. Inf / NaN bit patterns
/// emit `"inf"` / `"-inf"` / `"nan"` (JSON does not allow these, but the
/// codec stays defensive).
///
/// No FP arithmetic is performed.
///
/// @param bits       IEEE 754 single-precision bit pattern.
/// @param precision  Digits after the decimal point. Negative values
///                   collapse to `0` (integer form). Clamped to [0, 9].
/// @return           Heap-allocated `fl::string` with the decimal text.
fl::string ieee754_format_decimal(u32 bits, int precision = 6) FL_NOEXCEPT;

} // namespace fl
