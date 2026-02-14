#pragma once

/// @file sincos32x4.h
/// SIMD sincos32x4: Process 4 angles simultaneously

#include "fl/simd.h"
#include "fl/fixed_point/s0x32x4.h"
#include "fl/fixed_point/simd_ops.h"
#include "fl/sin32.h"
#include "fl/force_inline.h"

namespace fl {

/// Combined sin+cos result for 4 angles
struct SinCos32x4 {
    s0x32x4 sin_vals;  // 4 sin results (normalized [-1, 1])
    s0x32x4 cos_vals;  // 4 cos results (normalized [-1, 1])
};

/// Process 4 angles simultaneously, returning vectorized sin/cos values
/// Scalar fallback implementation (TODO: optimize with SIMD LUT gather)
FASTLED_FORCE_INLINE SinCos32x4 sincos32x4(simd::simd_u32x4 angles) {
    alignas(16) u32 angle_array[4];
    alignas(16) i32 sin_results[4];
    alignas(16) i32 cos_results[4];

    simd::platforms::store_u32_4(angle_array, angles);

    for (int i = 0; i < 4; i++) {
        fl::SinCos32 sc = fl::sincos32(angle_array[i]);
        sin_results[i] = sc.sin_val;
        cos_results[i] = sc.cos_val;
    }

    SinCos32x4 result;
    result.sin_vals = s0x32x4::from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(sin_results))); // ok reinterpret cast
    result.cos_vals = s0x32x4::from_raw(simd::platforms::load_u32_4(reinterpret_cast<const u32*>(cos_results))); // ok reinterpret cast
    return result;
}

} // namespace fl
