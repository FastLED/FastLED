#pragma once

/// @file simd_x86.hpp
/// x86/x64 SIMD implementations using SSE2/AVX intrinsics
///
/// Provides atomic SIMD operations for x86/x64 processors.
/// SSE2 baseline ensures compatibility with all x64 and most modern x86 processors.

#include "fl/stl/stdint.h"
#include "fl/align.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/stl/math.h"  // for sqrtf

// SSE2 intrinsics (baseline for all x64, available on most x86)
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  // Clang's SIMD intrinsic headers use C++14 constexpr features
  // Suppress C++14 extension warnings when including these system headers
  FL_DIAGNOSTIC_PUSH
  FL_DIAGNOSTIC_IGNORE_C14_EXTENSIONS
  #include <emmintrin.h>  // SSE2
  FL_DIAGNOSTIC_POP
  #define FASTLED_X86_HAS_SSE2 1
#else
  #define FASTLED_X86_HAS_SSE2 0
#endif

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platforms {

#if FASTLED_X86_HAS_SSE2

//==============================================================================
// SIMD Register Types (SSE2)
//==============================================================================

// Use native SSE2 __m128i register type
using simd_u8x16 = __m128i;
using simd_u32x4 = __m128i;
using simd_f32x4 = __m128;

//==============================================================================
// Atomic Load/Store Operations (SSE2)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) noexcept {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), vec); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), vec); // ok reinterpret cast
}
FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) noexcept {
    return _mm_loadu_ps(ptr);
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) noexcept {
    _mm_storeu_ps(ptr, vec);
}

//==============================================================================
// Atomic Arithmetic Operations (SSE2)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_adds_epu8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) noexcept {
    if (scale == 255) {
        return vec;  // Identity
    }
    if (scale == 0) {
        return _mm_setzero_si128();
    }

    // SSE2 approach: unpack bytes to 16-bit, multiply, shift, pack
    // For simplicity in initial implementation, use scalar loop
    // TODO: Optimize with proper SSE2 unpack/multiply/pack sequence
    FL_ALIGNAS(16) u8 data[16];
    _mm_store_si128(reinterpret_cast<__m128i*>(data), vec); // ok reinterpret cast

    for (int i = 0; i < 16; ++i) {
        data[i] = static_cast<u8>((static_cast<u16>(data[i]) * scale) >> 8);
    }

    return _mm_load_si128(reinterpret_cast<const __m128i*>(data)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
    return _mm_set1_epi32(static_cast<i32>(value));
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) noexcept {
    return _mm_set1_ps(value);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_add_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_sub_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_mul_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_div_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) noexcept {
    return _mm_sqrt_ps(vec);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_min_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    return _mm_max_ps(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) noexcept {
    // SSE2 implementation: result = a + ((b - a) * amount) >> 8
    // We use mulhi to get the high byte of the 32-bit product directly

    // Unpack a and b to 16-bit (low 8 bytes)
    __m128i a_low_16 = _mm_unpacklo_epi8(a, _mm_setzero_si128());
    __m128i b_low_16 = _mm_unpacklo_epi8(b, _mm_setzero_si128());

    // Unpack a and b to 16-bit (high 8 bytes)
    __m128i a_high_16 = _mm_unpackhi_epi8(a, _mm_setzero_si128());
    __m128i b_high_16 = _mm_unpackhi_epi8(b, _mm_setzero_si128());

    // Compute (b - a) for low and high (as signed 16-bit to handle negative diffs)
    __m128i diff_low = _mm_sub_epi16(b_low_16, a_low_16);
    __m128i diff_high = _mm_sub_epi16(b_high_16, a_high_16);

    // Multiply by amount and extract high byte using mulhi
    // For signed multiply: (diff * amount) >> 8 = mulhi(diff, amount << 8) OR we can use (mullo >> 8) + (mulhi << 8)
    __m128i amount_16 = _mm_set1_epi16(static_cast<i16>(amount));

    // mulhi gives us bits [31:16] of the 32-bit product
    // We want bits [15:8], so: (mullo >> 8) | (mulhi << 8) in 16-bit
    // But actually for 16-bit * 16-bit, we want the result >> 8
    // That's byte 1 of the 32-bit result, which is byte 1 of mullo or byte 0 of (mulhi shifted)
    __m128i mulhi_low = _mm_mulhi_epi16(diff_low, amount_16);
    __m128i mulhi_high = _mm_mulhi_epi16(diff_high, amount_16);
    __m128i mullo_low = _mm_mullo_epi16(diff_low, amount_16);
    __m128i mullo_high = _mm_mullo_epi16(diff_high, amount_16);

    // Extract bits [15:8] from 32-bit product: (mullo >> 8) | (mulhi << 8)
    __m128i scaled_low = _mm_or_si128(_mm_srli_epi16(mullo_low, 8), _mm_slli_epi16(mulhi_low, 8));
    __m128i scaled_high = _mm_or_si128(_mm_srli_epi16(mullo_high, 8), _mm_slli_epi16(mulhi_high, 8));

    // Add back to a
    __m128i result_low = _mm_add_epi16(a_low_16, scaled_low);
    __m128i result_high = _mm_add_epi16(a_high_16, scaled_high);

    // Pack back to 8-bit with saturation
    return _mm_packus_epi16(result_low, result_high);
}


FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_subs_epu8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_avg_epu8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_avg_epu8(a, b);  // SSE2 _mm_avg_epu8 already rounds
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_min_epu8(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_max_epu8(a, b);
}


FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_and_si128(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_or_si128(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_xor_si128(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    return _mm_andnot_si128(a, b);
}

//==============================================================================
// Int32 SIMD Operations (SSE2)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_xor_si128(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_add_epi32(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_sub_epi32(a, b);
}

// Multiply i32 and return high 32 bits (for fixed-point Q16.16 math)
// Result: ((i64)a * (i64)b) >> 16, for each of 4 lanes
// Uses SSE2 unsigned multiply (_mm_mul_epu32) with signed correction:
//   signed_result = unsigned_mulhi(a, b) - (a<0 ? b<<16 : 0) - (b<0 ? a<<16 : 0)
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // Unsigned 32x32->64 multiply for even lanes (0, 2)
    __m128i prod02 = _mm_mul_epu32(a, b);
    // Shift odd lanes (1, 3) into even positions and multiply
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epu32(a_odd, b_odd);

    // Logical right shift 64-bit products by 16
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);

    // Pack low 32 bits of each 64-bit lane into [r0, r1, r2, r3]
    // sh02 as 4x32: [lo0, hi0, lo2, hi2] -> we want lo0, lo2
    // sh13 as 4x32: [lo1, hi1, lo3, hi3] -> we want lo1, lo3
    __m128i p02 = _mm_shuffle_epi32(sh02, _MM_SHUFFLE(2, 0, 2, 0)); // [lo0, lo2, lo0, lo2]
    __m128i p13 = _mm_shuffle_epi32(sh13, _MM_SHUFFLE(2, 0, 2, 0)); // [lo1, lo3, lo1, lo3]
    __m128i unsigned_result = _mm_unpacklo_epi32(p02, p13);          // [lo0, lo1, lo2, lo3]

    // Signed correction: when a < 0, unsigned product has excess b*2^32,
    // which after >>16 becomes b<<16. Same for b < 0.
    __m128i sign_a = _mm_srai_epi32(a, 31); // 0xFFFFFFFF if a<0, else 0
    __m128i sign_b = _mm_srai_epi32(b, 31); // 0xFFFFFFFF if b<0, else 0
    __m128i b_shl16 = _mm_slli_epi32(b, 16);
    __m128i a_shl16 = _mm_slli_epi32(a, 16);
    __m128i corr_a = _mm_and_si128(sign_a, b_shl16); // b<<16 where a<0
    __m128i corr_b = _mm_and_si128(sign_b, a_shl16); // a<<16 where b<0
    __m128i correction = _mm_add_epi32(corr_a, corr_b);

    return _mm_sub_epi32(unsigned_result, correction);
}

// Shift right logical (zero-fill) - for unsigned angle decomposition
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    return _mm_srli_epi32(vec, shift);
}

// Bitwise AND of two u32 vectors
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_and_si128(a, b);
}

#else

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

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
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
// Int32 SIMD Operations
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

// Shift right logical (zero-fill) - for unsigned angle decomposition
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;
    }
    return result;
}

// Bitwise AND of two u32 vectors
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
}

#endif  // FASTLED_X86_HAS_SSE2

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#endif  // x86/x64
