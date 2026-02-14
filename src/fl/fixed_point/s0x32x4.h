#pragma once

/// @file s0x32x4.h
/// SIMD 4-wide s0x32 fixed-point vector type

#include "fl/simd.h"
#include "fl/fixed_point/s0x32.h"
#include "fl/force_inline.h"

namespace fl {

// Forward declaration for cross-type operations
struct s16x16x4;

/// 4-wide s0x32 vector (normalized values [-1, 1])
/// Backed by 128-bit SIMD register (4× i32 in Q31 format)
struct s0x32x4 {
    simd::simd_u32x4 raw;  // 4× i32 values in Q31 format

    // ---- Construction ------------------------------------------------------

    static FASTLED_FORCE_INLINE s0x32x4 from_raw(simd::simd_u32x4 r) {
        s0x32x4 result;
        result.raw = r;
        return result;
    }

    // Load 4 s0x32 values from memory (unaligned access supported)
    static FASTLED_FORCE_INLINE s0x32x4 load(const s0x32* ptr) {
        return from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(ptr))); // ok reinterpret cast
    }

    // Store 4 s0x32 values to memory (unaligned access supported)
    FASTLED_FORCE_INLINE void store(s0x32* ptr) const {
        simd::platforms::store_u32_4(reinterpret_cast<u32*>(ptr), raw); // ok reinterpret cast
    }

    // Broadcast single s0x32 value to all 4 lanes
    static FASTLED_FORCE_INLINE s0x32x4 set1(s0x32 value) {
        return from_raw(simd::platforms::set1_u32_4(static_cast<u32>(value.raw())));
    }

    // ---- SIMD multiply: s0x32x4 × s16x16x4 → s16x16x4 ----------------------
    // Math: Q31 × Q16 = Q47 → shift right 31 → Q16
    // Implemented after s16x16x4 is defined
    FASTLED_FORCE_INLINE s16x16x4 operator*(s16x16x4 b) const;
};

} // namespace fl
