#pragma once

// IWYU pragma: private

/// @file simd_arm_neon.hpp
/// ARM NEON SIMD implementations using NEON intrinsics
///
/// Provides atomic SIMD operations for ARM processors with NEON support.
/// NEON is available on most ARM Cortex-A processors and newer Cortex-M processors.

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/math.h"  // for sqrtf
#include "fl/stl/align.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)

// IWYU pragma: begin_keep
#include <arm_neon.h>
// IWYU pragma: end_keep

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platforms {

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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    return vld1q_u8(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) noexcept {
    vst1q_u8(ptr, vec);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    return vld1q_u32(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) noexcept {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
    return vdupq_n_u32(value);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) noexcept {
    u32 tmp[4] = {a, b, c, d};
    return vld1q_u32(tmp);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) noexcept {
    return vld1q_u32(ptr);  // NEON vld1q handles any alignment
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) noexcept {
    vst1q_u32(ptr, vec);  // NEON vst1q handles any alignment
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) noexcept {
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
    int16x8_t scaled_low = vmulq_n_s16(diff_low, static_cast<i16>(amount));
    int16x8_t scaled_high = vmulq_n_s16(diff_high, static_cast<i16>(amount));

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

//==============================================================================
// Int32 SIMD Operations (NEON)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return veorq_u32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return vaddq_u32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return vsubq_u32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // signed (a*b) >> 16
    int32x4_t sa = vreinterpretq_s32_u32(a);
    int32x4_t sb = vreinterpretq_s32_u32(b);
    // Process each lane (NEON lacks a direct 32x32->64 high intrinsic for all 4)
    i64 p0 = static_cast<i64>(vgetq_lane_s32(sa, 0)) * vgetq_lane_s32(sb, 0);
    i64 p1 = static_cast<i64>(vgetq_lane_s32(sa, 1)) * vgetq_lane_s32(sb, 1);
    i64 p2 = static_cast<i64>(vgetq_lane_s32(sa, 2)) * vgetq_lane_s32(sb, 2);
    i64 p3 = static_cast<i64>(vgetq_lane_s32(sa, 3)) * vgetq_lane_s32(sb, 3);
    u32 r[4] = {
        static_cast<u32>(static_cast<i32>(p0 >> 16)),
        static_cast<u32>(static_cast<i32>(p1 >> 16)),
        static_cast<u32>(static_cast<i32>(p2 >> 16)),
        static_cast<u32>(static_cast<i32>(p3 >> 16))
    };
    return vld1q_u32(r);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // unsigned (a*b) >> 16
    u64 p0 = static_cast<u64>(vgetq_lane_u32(a, 0)) * vgetq_lane_u32(b, 0);
    u64 p1 = static_cast<u64>(vgetq_lane_u32(a, 1)) * vgetq_lane_u32(b, 1);
    u64 p2 = static_cast<u64>(vgetq_lane_u32(a, 2)) * vgetq_lane_u32(b, 2);
    u64 p3 = static_cast<u64>(vgetq_lane_u32(a, 3)) * vgetq_lane_u32(b, 3);
    u32 r[4] = {
        static_cast<u32>(p0 >> 16),
        static_cast<u32>(p1 >> 16),
        static_cast<u32>(p2 >> 16),
        static_cast<u32>(p3 >> 16)
    };
    return vld1q_u32(r);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return mulhi_i32_4(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    // NEON vshlq_u32 with negative shift = right shift
    return vshlq_u32(vec, vdupq_n_s32(-shift));
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sll_u32_4(simd_u32x4 vec, int shift) noexcept {
    return vshlq_u32(vec, vdupq_n_s32(shift));
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sra_i32_4(simd_u32x4 vec, int shift) noexcept {
    // Arithmetic right shift on signed reinterpretation
    int32x4_t sv = vreinterpretq_s32_u32(vec);
    sv = vshlq_s32(sv, vdupq_n_s32(-shift));
    return vreinterpretq_u32_s32(sv);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return vandq_u32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return vorrq_u32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 min_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    int32x4_t sa = vreinterpretq_s32_u32(a);
    int32x4_t sb = vreinterpretq_s32_u32(b);
    return vreinterpretq_u32_s32(vminq_s32(sa, sb));
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 max_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    int32x4_t sa = vreinterpretq_s32_u32(a);
    int32x4_t sb = vreinterpretq_s32_u32(b);
    return vreinterpretq_u32_s32(vmaxq_s32(sa, sb));
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi32_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // signed (a*b) >> 32
    int32x4_t sa = vreinterpretq_s32_u32(a);
    int32x4_t sb = vreinterpretq_s32_u32(b);
    i64 p0 = static_cast<i64>(vgetq_lane_s32(sa, 0)) * vgetq_lane_s32(sb, 0);
    i64 p1 = static_cast<i64>(vgetq_lane_s32(sa, 1)) * vgetq_lane_s32(sb, 1);
    i64 p2 = static_cast<i64>(vgetq_lane_s32(sa, 2)) * vgetq_lane_s32(sb, 2);
    i64 p3 = static_cast<i64>(vgetq_lane_s32(sa, 3)) * vgetq_lane_s32(sb, 3);
    u32 r[4] = {
        static_cast<u32>(static_cast<i32>(p0 >> 32)),
        static_cast<u32>(static_cast<i32>(p1 >> 32)),
        static_cast<u32>(static_cast<i32>(p2 >> 32)),
        static_cast<u32>(static_cast<i32>(p3 >> 32))
    };
    return vld1q_u32(r);
}

FASTLED_FORCE_INLINE FL_IRAM u32 extract_u32_4(simd_u32x4 vec, int lane) noexcept {
    switch (lane) {
        case 0: return vgetq_lane_u32(vec, 0);
        case 1: return vgetq_lane_u32(vec, 1);
        case 2: return vgetq_lane_u32(vec, 2);
        default: return vgetq_lane_u32(vec, 3);
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // {a0, b0, a1, b1}
    uint32x2_t a_lo = vget_low_u32(a);
    uint32x2_t b_lo = vget_low_u32(b);
    uint32x2x2_t zipped = vzip_u32(a_lo, b_lo);
    return vcombine_u32(zipped.val[0], zipped.val[1]);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // {a2, b2, a3, b3}
    uint32x2_t a_hi = vget_high_u32(a);
    uint32x2_t b_hi = vget_high_u32(b);
    uint32x2x2_t zipped = vzip_u32(a_hi, b_hi);
    return vcombine_u32(zipped.val[0], zipped.val[1]);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // {a0, a1, b0, b1}
    return vcombine_u32(vget_low_u32(a), vget_low_u32(b));
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // {a2, a3, b2, b3}
    return vcombine_u32(vget_high_u32(a), vget_high_u32(b));
}

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#else

//==============================================================================
// Scalar Fallback for Non-NEON ARM Platforms
//==============================================================================

namespace fl {
namespace simd {
namespace platforms {

//==============================================================================
// SIMD Register Types (Scalar Fallback)
//==============================================================================

struct FL_ALIGNAS(16) simd_u8x16 {
    u8 data[16];
};

struct FL_ALIGNAS(16) simd_u32x4 {
    u32 data[4];
};

struct FL_ALIGNAS(16) simd_f32x4 {
    float data[4];
};

//==============================================================================
// Atomic Load/Store Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) noexcept {
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) noexcept {
    const u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = p[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) noexcept {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    for (int i = 0; i < 4; ++i) {
        p[i] = vec.data[i];
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
        u16 sum = static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<u8>(sum);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
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



FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) noexcept {
    simd_u8x16 result;
    // result = a + ((b - a) * amount) / 256
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(b.data[i]) - static_cast<i16>(a.data[i]);
        i16 scaled = (diff * amount) >> 8;
        result.data[i] = static_cast<u8>(a.data[i] + scaled);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(a.data[i]) - static_cast<i16>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<u8>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]) + 1) >> 1);
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

//==============================================================================
// Int32 SIMD Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i + b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i - b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(a_i) * static_cast<i64>(b_i);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 16));
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sll_u32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] << shift;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sra_i32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 signed_val = static_cast<i32>(vec.data[i]);
        result.data[i] = static_cast<u32>(signed_val >> shift);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        u64 prod = static_cast<u64>(a.data[i]) * static_cast<u64>(b.data[i]);
        result.data[i] = static_cast<u32>(prod >> 16);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return mulhi_i32_4(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi32_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(ai) * static_cast<i64>(bi);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 32));
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 min_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(ai < bi ? ai : bi);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 max_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(ai > bi ? ai : bi);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM u32 extract_u32_4(simd_u32x4 vec, int lane) noexcept {
    return vec.data[lane];
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = b.data[0];
    result.data[2] = a.data[1]; result.data[3] = b.data[1];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = b.data[2];
    result.data[2] = a.data[3]; result.data[3] = b.data[3];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = a.data[1];
    result.data[2] = b.data[0]; result.data[3] = b.data[1];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = a.data[3];
    result.data[2] = b.data[2]; result.data[3] = b.data[3];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) noexcept {
    simd_u32x4 result;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
    return result;
}

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#endif  // __ARM_NEON or __ARM_NEON__
