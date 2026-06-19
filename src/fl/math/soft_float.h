#pragma once

// fl::soft_float -- integer-only IEEE-754 float<->double conversion.
//
// On no-FPU targets (Cortex-M0+, ATmega, ...) a plain `static_cast<double>(f)`
// or `static_cast<float>(d)` anchors libgcc's soft-FP helpers
// (`__aeabi_f2d`, `__aeabi_d2f`) which transitively pull `__aeabi_dadd /
// dmul / ddiv / dcmpun / scalbn` and similar. That's ~6 KB of flash on
// LPC845 / similar 64 KB-flash parts -- ~10% of the budget.
//
// This module provides bit-level conversions that perform the IEEE-754
// widening / narrowing using only integer arithmetic (shift / mask / add).
// They are correct on every host with IEEE-754 `float` / `double` (which is
// every supported FastLED target), produce bit-exact results, and emit
// zero soft-FP symbols even when called from a no-FPU build.
//
// Symmetric narrowing-vs-widening contract:
//   - Sign bit copied directly.
//   - +/-zero, +/-inf preserved bit-exactly.
//   - NaN canonicalized to a quiet-NaN with the standard payload
//     (matches what the JSON path already did with its narrowing helper).
//   - Subnormal float -> normal double (widening); double subnormal /
//     small normal -> float subnormal (narrowing).
//
// Predecessor: this code lived in `fl/stl/json/types_impl.h` (`json_*`
// names) for the LPC845 JSON-RPC bring-up (FastLED #3038 / #3076 / #3226).
// Moved to `fl/math/` as part of FastLED #3022 so non-JSON callers
// (audio, animation timing, anyone else who needs FP without the libgcc
// soft-FP tax) can use it directly.

#include "fl/stl/bit_cast.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace detail {

// Byte-wise reinterpret. Differs from `fl::bit_cast<To, From>` in that it
// does NOT static-assert on size equality -- it copies
// `min(sizeof(To), sizeof(From))` bytes and zero-pads the rest. Used here
// so the `sizeof(double) == sizeof(u32)` (pathological-host) branch can
// compile alongside the standard 8-byte-double branch without tripping
// `bit_cast`'s assertion.
template <typename To, typename From>
inline To soft_bit_copy(const From& value) FL_NO_EXCEPT {
    To out = 0;
    const char* src = fl::bit_cast_ptr<const char>(&value);
    char* dst = fl::bit_cast_ptr<char>(&out);
    const fl::size n = sizeof(To) < sizeof(From) ? sizeof(To) : sizeof(From);
    for (fl::size i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
    return out;
}

inline u32 soft_round_shift_right_u64(u64 value, int shift) FL_NO_EXCEPT {
    if (shift <= 0) {
        return static_cast<u32>(value);
    }
    if (shift >= 64) {
        return 0;
    }
    const u64 half = u64(1) << (shift - 1);
    const u64 mask = (u64(1) << shift) - 1;
    const u64 truncated = value >> shift;
    const u64 remainder = value & mask;
    const bool round_up = remainder > half || (remainder == half && (truncated & 1));
    return static_cast<u32>(truncated + (round_up ? 1 : 0));
}

inline int soft_floor_log2_u64(u64 value) FL_NO_EXCEPT {
    int bit = -1;
    while (value != 0) {
        value >>= 1;
        ++bit;
    }
    return bit;
}

inline u32 soft_float_bits_from_scaled_u64(
    u32 sign, u64 magnitude, int binary_exp) FL_NO_EXCEPT {
    if (magnitude == 0) {
        return sign;
    }

    const int top_bit = soft_floor_log2_u64(magnitude);
    int exponent = top_bit + binary_exp;
    if (exponent > 127) {
        return sign | 0x7F800000u;
    }

    if (exponent >= -126) {
        const int shift = top_bit - 23;
        u32 significand = shift > 0
            ? soft_round_shift_right_u64(magnitude, shift)
            : static_cast<u32>(magnitude << -shift);
        if (significand >= 0x01000000u) {
            significand >>= 1;
            ++exponent;
            if (exponent > 127) {
                return sign | 0x7F800000u;
            }
        }
        return sign | (static_cast<u32>(exponent + 127) << 23) |
               (significand & 0x007FFFFFu);
    }

    const int subnormal_shift = -(binary_exp + 149);
    u32 mantissa = 0;
    if (subnormal_shift > 0) {
        mantissa = soft_round_shift_right_u64(magnitude, subnormal_shift);
    } else {
        const int left_shift = -subnormal_shift;
        if (left_shift >= 64 || magnitude > (u64(-1) >> left_shift)) {
            return sign | 0x7F800000u;
        }
        const u64 shifted = magnitude << left_shift;
        mantissa = shifted > 0x007FFFFFu ? 0x00800000u : static_cast<u32>(shifted);
    }
    if (mantissa >= 0x00800000u) {
        return sign | 0x00800000u;
    }
    return sign | mantissa;
}

} // namespace detail

// Narrow a 64-bit double's bit pattern to the bit pattern of the closest
// 32-bit float, using only integer arithmetic. Round-to-nearest-even.
inline u32 soft_float_bits_from_double_bits(u64 bits) FL_NO_EXCEPT {
    const u32 sign = (bits >> 63) ? 0x80000000u : 0u;
    const u32 exp_bits = static_cast<u32>((bits >> 52) & 0x7FFu);
    const u64 mantissa_bits = bits & 0x000FFFFFFFFFFFFFull;
    if (exp_bits == 0x7FFu) {
        return mantissa_bits == 0 ? sign | 0x7F800000u : sign | 0x7FC00000u;
    }
    if (exp_bits == 0) {
        return detail::soft_float_bits_from_scaled_u64(sign, mantissa_bits, -1074);
    }
    return detail::soft_float_bits_from_scaled_u64(
        sign, mantissa_bits | 0x0010000000000000ull,
        static_cast<int>(exp_bits) - 1075);
}

// Widen a 32-bit float's bit pattern to the bit pattern of the same value
// as a 64-bit double, using only integer arithmetic. Lossless: every finite
// float has an exact double representation, and the round-trip
// `double_bits_from_float_bits(float_bits_from_double_bits(d))` is the
// identity for any `d` that was a widened float.
inline u64 soft_double_bits_from_float_bits(u32 bits) FL_NO_EXCEPT {
    const u64 sign = (bits >> 31) ? 0x8000000000000000ull : 0ull;
    const u32 exp_bits = (bits >> 23) & 0xFFu;
    const u32 mantissa_bits = bits & 0x007FFFFFu;
    if (exp_bits == 0xFFu) {
        return mantissa_bits == 0 ? sign | 0x7FF0000000000000ull
                                  : sign | 0x7FF8000000000000ull;
    }
    if (exp_bits == 0) {
        if (mantissa_bits == 0) {
            return sign;
        }
        // Subnormal float -- always a normal double. Walk the implicit bit
        // up to position 23, adjust exponent accordingly.
        u32 m = mantissa_bits;
        int shift = 0;
        while ((m & 0x00800000u) == 0) {
            m <<= 1;
            ++shift;
        }
        const int double_exp = 1 - 127 - shift + 1023;
        const u64 mantissa_d = static_cast<u64>(m & 0x007FFFFFu) << 29;
        return sign | (static_cast<u64>(double_exp) << 52) | mantissa_d;
    }
    const int double_exp = static_cast<int>(exp_bits) - 127 + 1023;
    const u64 mantissa_d = static_cast<u64>(mantissa_bits) << 29;
    return sign | (static_cast<u64>(double_exp) << 52) | mantissa_d;
}

// Convenience: narrow a `double` value to a `float` via the integer codec.
inline float soft_double_to_float(double value) FL_NO_EXCEPT {
    if (sizeof(double) == sizeof(u32)) {
        return detail::soft_bit_copy<float>(value);
    }
    const u32 narrowed_bits =
        soft_float_bits_from_double_bits(detail::soft_bit_copy<u64>(value));
    return detail::soft_bit_copy<float>(narrowed_bits);
}

// Convenience: widen a `float` value to a `double` via the integer codec.
inline double soft_float_to_double(float value) FL_NO_EXCEPT {
    if (sizeof(double) == sizeof(u32)) {
        return detail::soft_bit_copy<double>(value);
    }
    const u64 widened_bits =
        soft_double_bits_from_float_bits(detail::soft_bit_copy<u32>(value));
    return detail::soft_bit_copy<double>(widened_bits);
}

} // namespace fl
