#pragma once

/// @file s16x16x4.h
/// SIMD 4-wide s16x16 fixed-point vector type

#include "fl/stl/simd.h"
#include "fl/stl/fixed_point/s16x16.h"
#include "fl/math/sin32.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/align.h"

namespace fl {

// Forward declaration for cross-type operations
struct s0x32x4;

/// 4-wide s16x16 vector (general fixed-point)
/// Backed by 128-bit SIMD register (4× i32 in Q16 format)
struct s16x16x4 {
    simd::simd_u32x4 raw;  // 4× i32 values in Q16 format

    // ---- Construction ------------------------------------------------------

    static FASTLED_FORCE_INLINE s16x16x4 from_raw(simd::simd_u32x4 r) {
        s16x16x4 result;
        result.raw = r;
        return result;
    }

    // Load 4 s16x16 values from memory (unaligned access supported)
    static FASTLED_FORCE_INLINE s16x16x4 load(const s16x16* ptr) {
        return from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(ptr))); // ok reinterpret cast
    }

    // Store 4 s16x16 values to memory (unaligned access supported)
    FASTLED_FORCE_INLINE void store(s16x16* ptr) const {
        simd::platforms::store_u32_4(reinterpret_cast<u32*>(ptr), raw); // ok reinterpret cast
    }

    // Broadcast single s16x16 value to all 4 lanes
    static FASTLED_FORCE_INLINE s16x16x4 set1(s16x16 value) {
        return from_raw(simd::platforms::set1_u32_4(static_cast<u32>(value.raw())));
    }

    // ---- SIMD arithmetic (s16x16x4 OP s16x16x4 → s16x16x4) -----------------

    FASTLED_FORCE_INLINE s16x16x4 operator+(s16x16x4 b) const {
        return from_raw(simd::add_i32_4(raw, b.raw));
    }

    FASTLED_FORCE_INLINE s16x16x4 operator-(s16x16x4 b) const {
        return from_raw(simd::sub_i32_4(raw, b.raw));
    }

    FASTLED_FORCE_INLINE s16x16x4 operator*(s16x16x4 b) const {
        // Q16 × Q16 = Q32 → shift right 16 → Q16
        return from_raw(simd::mulhi_i32_4(raw, b.raw));
    }

    FASTLED_FORCE_INLINE s16x16x4 operator-() const {
        // Unary negation: -x = 0 - x
        auto zero = simd::set1_u32_4(0);
        return from_raw(simd::sub_i32_4(zero, raw));
    }

    FASTLED_FORCE_INLINE s16x16x4 operator>>(int shift) const {
        return from_raw(simd::sra_i32_4(raw, shift));
    }

    FASTLED_FORCE_INLINE s16x16x4 operator<<(int shift) const {
        return from_raw(simd::sll_u32_4(raw, shift));
    }

    // Cross-type multiply: s16x16x4 × s0x32x4 → s16x16x4 (commutative)
    // Implemented after s0x32x4 is defined
    FASTLED_FORCE_INLINE s16x16x4 operator*(s0x32x4 b) const;

    // ---- Math functions -------------------------------------------------------

    /// Absolute value: branchless via mask and xor
    FASTLED_FORCE_INLINE s16x16x4 abs() const {
        // mask = -1 if negative (sign extended), 0 if positive
        auto mask = simd::sra_i32_4(raw, 31);
        // flip bits if negative, then add 1 (two's complement)
        auto flipped = simd::xor_u32_4(raw, mask);
        return from_raw(simd::sub_i32_4(flipped, mask));
    }

    /// Element-wise minimum
    FASTLED_FORCE_INLINE s16x16x4 min(s16x16x4 b) const {
        return from_raw(simd::min_i32_4(raw, b.raw));
    }

    /// Element-wise maximum
    FASTLED_FORCE_INLINE s16x16x4 max(s16x16x4 b) const {
        return from_raw(simd::max_i32_4(raw, b.raw));
    }

    /// Clamp to [lo, hi]
    FASTLED_FORCE_INLINE s16x16x4 clamp(s16x16x4 lo, s16x16x4 hi) const {
        return max(lo).min(hi);
    }

    /// Linear interpolation: a + (b - a) * t (using Q16 multiply)
    /// t: s16x16 interpolation factor in [0, 1]
    FASTLED_FORCE_INLINE s16x16x4 lerp(s16x16x4 b, s16x16 t) const {
        auto t_vec = s16x16x4::set1(t);
        auto diff = b - (*this);
        return (*this) + (diff * t_vec);
    }

    /// Compute sin and cos of 4 angles (in radians)
    /// Results written to out_sin and out_cos
    FASTLED_FORCE_INLINE void sincos(s16x16x4& out_sin, s16x16x4& out_cos) const {
        // Convert radians to 24-bit angle units (same as scalar s16x16)
        // RAD_TO_24 = 2^24 / (2π) in Q16
        static constexpr i32 RAD_TO_24 = 2670177;  // from s16x16.h

        // Convert 4 angles: mulhi_i32_4 does (i64*i64) >> 16
        auto angles_u32 = simd::mulhi_su32_4(raw, simd::set1_u32_4(static_cast<u32>(RAD_TO_24)));

        // Call vectorized sincos
        auto sc = sincos32_simd(angles_u32);

        // Shift results right by 15 to convert from raw sin32 output to Q16.16
        out_sin = from_raw(simd::sra_i32_4(sc.sin_vals, 15));
        out_cos = from_raw(simd::sra_i32_4(sc.cos_vals, 15));
    }

    /// Compute sine of 4 angles (in radians)
    FASTLED_FORCE_INLINE s16x16x4 sin() const {
        s16x16x4 sin_out, cos_out;
        sincos(sin_out, cos_out);
        return sin_out;
    }

    /// Compute cosine of 4 angles (in radians)
    FASTLED_FORCE_INLINE s16x16x4 cos() const {
        s16x16x4 sin_out, cos_out;
        sincos(sin_out, cos_out);
        return cos_out;
    }
};

// Include simd_ops.h to implement cross-type operations
// Must come after all types are defined
#include "fl/stl/fixed_point/simd_ops.h"  // allow-include-after-namespace

} // namespace fl
