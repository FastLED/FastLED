#pragma once

/// @file simd_ops.h
/// Cross-type SIMD fixed-point operations (implemented after all types are defined)
///
/// This header is included from within s16x16x4.h INSIDE the fl namespace context.
/// It provides out-of-line implementations for cross-type multiplication operators.
/// NOTE: This file does NOT open/close namespace - it relies on being included
/// within an already-open fl namespace.

#include "fl/stl/fixed_point/s0x32x4.h"
#include "fl/stl/fixed_point/s16x16x4.h"
#include "fl/stl/align.h"

// Implementation of s0x32x4 × s16x16x4 (after s16x16x4 is defined)
FASTLED_FORCE_INLINE s16x16x4 s0x32x4::operator*(s16x16x4 b) const {
    // Q31 × Q16 = Q47 → shift right 31 → Q16
    // Use mulhi32_i32_4 which computes (i64*i64) >> 32, then shift left 1
    // (because >> 31 is equivalent to >> 32 then << 1)
    auto hi = simd::mulhi32_i32_4(raw, b.raw);
    return s16x16x4::from_raw(simd::sll_u32_4(hi, 1));
}

// Implementation of s16x16x4 × s0x32x4 (commutative)
FASTLED_FORCE_INLINE s16x16x4 s16x16x4::operator*(s0x32x4 b) const {
    return b * (*this);  // Delegate to s0x32x4::operator*
}
