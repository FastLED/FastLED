#pragma once

/// @file s16x16x4.h
/// SIMD 4-wide s16x16 fixed-point vector type

#include "fl/simd.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/force_inline.h"

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
    // Note: Requires platform implementations of add_u32_4/sub_u32_4
    // These are not yet in fl::simd::platforms, so we'll provide scalar fallback

    FASTLED_FORCE_INLINE s16x16x4 operator+(s16x16x4 b) const {
        // Scalar fallback until platform add_u32_4 is implemented
        alignas(16) i32 a_arr[4];
        alignas(16) i32 b_arr[4];
        alignas(16) i32 result[4];

        simd::platforms::store_u32_4(reinterpret_cast<u32*>(a_arr), raw); // ok reinterpret cast
        simd::platforms::store_u32_4(reinterpret_cast<u32*>(b_arr), b.raw); // ok reinterpret cast

        for (int i = 0; i < 4; i++) {
            result[i] = a_arr[i] + b_arr[i];
        }

        return from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(result))); // ok reinterpret cast
    }

    FASTLED_FORCE_INLINE s16x16x4 operator-(s16x16x4 b) const {
        // Scalar fallback until platform sub_u32_4 is implemented
        alignas(16) i32 a_arr[4];
        alignas(16) i32 b_arr[4];
        alignas(16) i32 result[4];

        simd::platforms::store_u32_4(reinterpret_cast<u32*>(a_arr), raw); // ok reinterpret cast
        simd::platforms::store_u32_4(reinterpret_cast<u32*>(b_arr), b.raw); // ok reinterpret cast

        for (int i = 0; i < 4; i++) {
            result[i] = a_arr[i] - b_arr[i];
        }

        return from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(result))); // ok reinterpret cast
    }

    // Cross-type multiply: s16x16x4 × s0x32x4 → s16x16x4 (commutative)
    // Implemented after s0x32x4 is defined
    FASTLED_FORCE_INLINE s16x16x4 operator*(s0x32x4 b) const;
};

} // namespace fl
