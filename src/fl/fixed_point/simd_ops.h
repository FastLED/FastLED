#pragma once

/// @file simd_ops.h
/// Cross-type SIMD fixed-point operations (implemented after all types are defined)

#include "fl/fixed_point/s0x32x4.h"
#include "fl/fixed_point/s16x16x4.h"

namespace fl {

// Implementation of s0x32x4 × s16x16x4 (after s16x16x4 is defined)
FASTLED_FORCE_INLINE s16x16x4 s0x32x4::operator*(s16x16x4 b) const {
    // Scalar fallback: Process each lane separately
    // Q31 × Q16 = Q47 → shift right 31 → Q16
    alignas(16) i32 a_arr[4];
    alignas(16) i32 b_arr[4];
    alignas(16) i32 result[4];

    simd::platforms::store_u32_4(reinterpret_cast<u32*>(a_arr), raw); // ok reinterpret cast
    simd::platforms::store_u32_4(reinterpret_cast<u32*>(b_arr), b.raw); // ok reinterpret cast

    for (int i = 0; i < 4; i++) {
        result[i] = static_cast<i32>(
            (static_cast<i64>(a_arr[i]) * b_arr[i]) >> 31);
    }

    return s16x16x4::from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(result))); // ok reinterpret cast
}

// Implementation of s16x16x4 × s0x32x4 (commutative)
FASTLED_FORCE_INLINE s16x16x4 s16x16x4::operator*(s0x32x4 b) const {
    return b * (*this);  // Delegate to s0x32x4::operator*
}

} // namespace fl
