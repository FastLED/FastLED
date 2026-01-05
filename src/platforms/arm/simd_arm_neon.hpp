#pragma once

/// @file simd_arm_neon.hpp
/// ARM NEON SIMD implementations using NEON intrinsics
///
/// Provides atomic SIMD operations for ARM processors with NEON support.
/// NEON is available on most ARM Cortex-A processors and newer Cortex-M processors.

#include "fl/stl/stdint.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/stl/math.h"  // for sqrtf
#include <arm_neon.h>

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platform {

//==============================================================================
// SIMD Register Types (NEON)
//==============================================================================

// Use native NEON vector types
using simd_u8x16 = uint8x16_t;
using simd_u32x4 = uint32x4_t;
using simd_f32x4 = float32x4_t;

//==============================================================================
// Atomic Load/Store Operations (NEON)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const uint8_t* ptr) noexcept {
    return vld1q_u8(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(uint8_t* ptr, simd_u8x16 vec) noexcept {
    vst1q_u8(ptr, vec);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const uint32_t* ptr) noexcept {
    return vld1q_u32(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(uint32_t* ptr, simd_u32x4 vec) noexcept {
    vst1q_u32(ptr, vec);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) noexcept {
    return vld1q_f32(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) noexcept {
    vst1q_f32(ptr, vec);
}

//==============================================================================
// Atomic Arithmetic Operations (NEON)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vqaddq_u8(a, b);  // Saturating add
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, uint8_t scale) noexcept {
    if (scale == 255) {
        return vec;  // Identity
    }
    if (scale == 0) {
        return vdupq_n_u8(0);  // Zero vector
    }

    // NEON approach: split into low/high 8 bytes, widen to u16, multiply, narrow back
    // Extract low and high 8 bytes
    uint8x8_t low = vget_low_u8(vec);
    uint8x8_t high = vget_high_u8(vec);

    // Widen to 16-bit
    uint16x8_t low_16 = vmovl_u8(low);
    uint16x8_t high_16 = vmovl_u8(high);

    // Multiply by scale
    uint16x8_t scale_vec = vdupq_n_u16(scale);
    low_16 = vmulq_u16(low_16, scale_vec);
    high_16 = vmulq_u16(high_16, scale_vec);

    // Shift right by 8 to divide by 256
    low_16 = vshrq_n_u16(low_16, 8);
    high_16 = vshrq_n_u16(high_16, 8);

    // Narrow back to 8-bit
    uint8x8_t low_result = vmovn_u16(low_16);
    uint8x8_t high_result = vmovn_u16(high_16);

    // Combine low and high
    return vcombine_u8(low_result, high_result);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(uint32_t value) noexcept {
    return vdupq_n_u32(value);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, uint8_t amount) noexcept {
    // NEON implementation: result = a + ((b - a) * amount) / 256

    // Extract low and high 8 bytes
    uint8x8_t a_low = vget_low_u8(a);
    uint8x8_t a_high = vget_high_u8(a);
    uint8x8_t b_low = vget_low_u8(b);
    uint8x8_t b_high = vget_high_u8(b);

    // Subtract and widen to signed 16-bit (handles negative differences correctly)
    int16x8_t diff_low = vreinterpretq_s16_u16(vsubl_u8(b_low, a_low));
    int16x8_t diff_high = vreinterpretq_s16_u16(vsubl_u8(b_high, a_high));

    // Multiply by amount
    int16x8_t scaled_low = vmulq_n_s16(diff_low, static_cast<int16_t>(amount));
    int16x8_t scaled_high = vmulq_n_s16(diff_high, static_cast<int16_t>(amount));

    // Shift right by 8 to divide by 256
    scaled_low = vshrq_n_s16(scaled_low, 8);
    scaled_high = vshrq_n_s16(scaled_high, 8);

    // Widen a to 16-bit
    uint16x8_t a_low_16 = vmovl_u8(a_low);
    uint16x8_t a_high_16 = vmovl_u8(a_high);

    // Add scaled difference to a (convert a to signed for addition)
    int16x8_t result_low_16 = vaddq_s16(vreinterpretq_s16_u16(a_low_16), scaled_low);
    int16x8_t result_high_16 = vaddq_s16(vreinterpretq_s16_u16(a_high_16), scaled_high);

    // Narrow back to unsigned 8-bit (with saturation)
    uint8x8_t result_low = vqmovun_s16(result_low_16);
    uint8x8_t result_high = vqmovun_s16(result_high_16);

    // Combine low and high
    return vcombine_u8(result_low, result_high);
}


FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vqsubq_u8(a, b);  // Saturating subtract
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vhaddq_u8(a, b);  // Halving add (rounds down)
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vrhaddq_u8(a, b);  // Rounding halving add
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vminq_u8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vmaxq_u8(a, b);
}



FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vandq_u8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vorrq_u8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return veorq_u8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return vbicq_u8(b, a);  // Note: vbicq_u8(x, y) computes x & ~y, so we swap parameters
}

//==============================================================================
// Float32 SIMD Operations (NEON)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) noexcept {
    return vdupq_n_f32(value);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return vaddq_f32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return vsubq_f32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return vmulq_f32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
    return vdivq_f32(a, b);
#else
    float32x4_t reciprocal = vrecpeq_f32(b);
    reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);
    return vmulq_f32(a, reciprocal);
#endif
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
    return vsqrtq_f32(vec);
#else
    float32x4_t rsqrt = vrsqrteq_f32(vec);
    rsqrt = vmulq_f32(vrsqrtsq_f32(vmulq_f32(vec, rsqrt), rsqrt), rsqrt);
    return vmulq_f32(vec, rsqrt);
#endif
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return vminq_f32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return vmaxq_f32(a, b);
}

}  // namespace platform
}  // namespace simd
}  // namespace fl

#else

//==============================================================================
// Scalar Fallback for Non-NEON ARM Platforms
//==============================================================================

namespace fl {
namespace simd {
namespace platform {

//==============================================================================
// SIMD Register Types (Scalar Fallback)
//==============================================================================

struct simd_u8x16 {
    uint8_t data[16];
};

struct simd_u32x4 {
    uint32_t data[4];
};

struct simd_f32x4 {
    float data[4];
};

//==============================================================================
// Atomic Load/Store Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const uint8_t* ptr) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(uint8_t* ptr, simd_u8x16 vec) noexcept {
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const uint32_t* ptr) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(uint32_t* ptr, simd_u32x4 vec) noexcept {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) noexcept {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// Atomic Arithmetic Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        uint16_t sum = static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<uint8_t>(sum);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, uint8_t scale) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(uint32_t value) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

//==============================================================================
// Float32 SIMD Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] * b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] / b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = fl::sqrtf(vec.data[i]);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}



FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, uint8_t amount) noexcept {
    simd_u8x16 result;
    // result = a + ((b - a) * amount) / 256
    for (int i = 0; i < 16; ++i) {
        int16_t diff = static_cast<int16_t>(b.data[i]) - static_cast<int16_t>(a.data[i]);
        int16_t scaled = (diff * amount) >> 8;
        result.data[i] = static_cast<uint8_t>(a.data[i] + scaled);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        int16_t diff = static_cast<int16_t>(a.data[i]) - static_cast<int16_t>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<uint8_t>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i]) + 1) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

//==============================================================================
// Atomic Bitwise Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (~a.data[i]) & b.data[i];
    }
    return result;
}

}  // namespace platform
}  // namespace simd
}  // namespace fl

#endif  // __ARM_NEON or __ARM_NEON__
