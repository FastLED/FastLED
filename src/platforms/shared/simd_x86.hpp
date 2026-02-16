#pragma once

// IWYU pragma: private

/// @file simd_x86.hpp
/// x86/x64 SIMD implementations using SSE2/AVX intrinsics
///
/// Provides atomic SIMD operations for x86/x64 processors.
/// SSE2 baseline ensures compatibility with all x64 and most modern x86 processors.

// IWYU pragma: no_include "_mingw_mac.h"
#include "fl/stl/stdint.h"
#include "fl/align.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/stl/math.h"  // IWYU pragma: keep (sqrtf used in #else scalar fallback)

// SSE2 intrinsics (baseline for all x64, available on most x86)
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  // Clang's SIMD intrinsic headers use C++14 constexpr features
  // Suppress C++14 extension warnings when including these system headers
  FL_DIAGNOSTIC_PUSH
  FL_DIAGNOSTIC_IGNORE_C14_EXTENSIONS
  // IWYU pragma: begin_keep
  #include <emmintrin.h>  // SSE2
  // IWYU pragma: end_keep
  FL_DIAGNOSTIC_POP
  #define FASTLED_X86_HAS_SSE2 1
#else
  #define FASTLED_X86_HAS_SSE2 0
#endif

// SSE4.1 intrinsics (available on x64 since ~2008: Intel Penryn, AMD Bulldozer)
// __SSE4_1__ is set by GCC/Clang with -msse4.1+; __AVX__ implies SSE4.1 on all compilers
#if defined(__SSE4_1__) || defined(__AVX__)
  FL_DIAGNOSTIC_PUSH
  FL_DIAGNOSTIC_IGNORE_C14_EXTENSIONS
  // IWYU pragma: begin_keep
  #include <smmintrin.h>  // SSE4.1
  // IWYU pragma: end_keep
  FL_DIAGNOSTIC_POP
  #define FASTLED_X86_HAS_SSE41 1
#else
  #define FASTLED_X86_HAS_SSE41 0
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
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
#if FASTLED_X86_HAS_SSE41
    // SSE4.1: signed 32x32->64 multiply eliminates the correction block (8 vs 14 ops)
    __m128i prod02 = _mm_mul_epi32(a, b);
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epi32(a_odd, b_odd);
    // Logical right shift is correct here: we extract low 32 bits of the 64-bit
    // result, so logical vs arithmetic only differs in bits 48-63 (above our window).
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);
    // Pack: sh02=[r0, ?, r2, ?], align sh13 to [0, r1, ?, r3], then blend
    __m128i sh13_aligned = _mm_slli_si128(sh13, 4);
    return _mm_blend_epi16(sh02, sh13_aligned, 0xCC);
#else
    // SSE2 fallback: unsigned multiply with signed correction
    __m128i prod02 = _mm_mul_epu32(a, b);
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epu32(a_odd, b_odd);
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);
    __m128i p02 = _mm_shuffle_epi32(sh02, _MM_SHUFFLE(2, 0, 2, 0));
    __m128i p13 = _mm_shuffle_epi32(sh13, _MM_SHUFFLE(2, 0, 2, 0));
    __m128i unsigned_result = _mm_unpacklo_epi32(p02, p13);
    // Signed correction: unsigned product has excess b*2^32 when a<0 (and vice versa),
    // which after >>16 becomes b<<16.
    __m128i sign_a = _mm_srai_epi32(a, 31);
    __m128i sign_b = _mm_srai_epi32(b, 31);
    __m128i corr_a = _mm_and_si128(sign_a, _mm_slli_epi32(b, 16));
    __m128i corr_b = _mm_and_si128(sign_b, _mm_slli_epi32(a, 16));
    return _mm_sub_epi32(unsigned_result, _mm_add_epi32(corr_a, corr_b));
#endif
}

// Multiply u32 and return high 32 bits (for fixed-point Q16.16 math, unsigned)
// Result: ((u64)a * (u64)b) >> 16, for each of 4 lanes
// On SSE2 this is 8 ops (vs 14 for signed mulhi_i32_4)
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // _mm_mul_epu32 multiplies lanes 0,2 as unsigned 32->64
    __m128i prod02 = _mm_mul_epu32(a, b);
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epu32(a_odd, b_odd);
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);
    __m128i p02 = _mm_shuffle_epi32(sh02, _MM_SHUFFLE(2, 0, 2, 0));
    __m128i p13 = _mm_shuffle_epi32(sh13, _MM_SHUFFLE(2, 0, 2, 0));
    return _mm_unpacklo_epi32(p02, p13);
}

// Multiply signed i32 by unsigned-positive u32, return >> 16 (Q16.16 fixed-point)
// Result: ((i64)(i32)a * (u64)(u32)b) >> 16, for each of 4 lanes
// Optimized for the case where b is known non-negative (e.g., interpolation fraction t).
// On SSE2: 11 ops (vs 14 for mulhi_i32_4) — skips sign correction for b.
// On SSE4.1: 8 ops (same as mulhi_i32_4, uses native signed multiply).
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
#if FASTLED_X86_HAS_SSE41
    // SSE4.1: native signed 32x32->64 handles both signs (8 ops)
    __m128i prod02 = _mm_mul_epi32(a, b);
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epi32(a_odd, b_odd);
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);
    __m128i sh13_aligned = _mm_slli_si128(sh13, 4);
    return _mm_blend_epi16(sh02, sh13_aligned, 0xCC);
#else
    // SSE2: unsigned multiply with one-sided sign correction (11 ops)
    // Since b is non-negative, only a's sign needs correction.
    __m128i prod02 = _mm_mul_epu32(a, b);
    __m128i a_odd = _mm_srli_si128(a, 4);
    __m128i b_odd = _mm_srli_si128(b, 4);
    __m128i prod13 = _mm_mul_epu32(a_odd, b_odd);
    __m128i sh02 = _mm_srli_epi64(prod02, 16);
    __m128i sh13 = _mm_srli_epi64(prod13, 16);
    __m128i p02 = _mm_shuffle_epi32(sh02, _MM_SHUFFLE(2, 0, 2, 0));
    __m128i p13 = _mm_shuffle_epi32(sh13, _MM_SHUFFLE(2, 0, 2, 0));
    __m128i unsigned_result = _mm_unpacklo_epi32(p02, p13);
    // One-sided sign correction: when a < 0, unsigned product has excess b*2^32,
    // which after >>16 becomes b<<16. No correction needed for b (always positive).
    __m128i sign_a = _mm_srai_epi32(a, 31);
    __m128i corr_a = _mm_and_si128(sign_a, _mm_slli_epi32(b, 16));
    return _mm_sub_epi32(unsigned_result, corr_a);
#endif
}

// Shift right logical (zero-fill) - for unsigned angle decomposition
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    return _mm_srli_epi32(vec, shift);
}

// Bitwise AND of two u32 vectors
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_and_si128(a, b);
}

// Extract a single u32 lane from a SIMD vector
FASTLED_FORCE_INLINE FL_IRAM u32 extract_u32_4(simd_u32x4 vec, int lane) noexcept {
    switch (lane) {
    case 0: return static_cast<u32>(_mm_cvtsi128_si32(vec));
    case 1: return static_cast<u32>(_mm_cvtsi128_si32(_mm_shuffle_epi32(vec, _MM_SHUFFLE(1,1,1,1))));
    case 2: return static_cast<u32>(_mm_cvtsi128_si32(_mm_shuffle_epi32(vec, _MM_SHUFFLE(2,2,2,2))));
    case 3: return static_cast<u32>(_mm_cvtsi128_si32(_mm_shuffle_epi32(vec, _MM_SHUFFLE(3,3,3,3))));
    default: return 0;
    }
}

// Interleave low 32-bit elements: {a0, b0, a1, b1}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_unpacklo_epi32(a, b);
}

// Interleave high 32-bit elements: {a2, b2, a3, b3}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_unpackhi_epi32(a, b);
}

// Interleave low 64-bit halves (as u32x4): {a0, a1, b0, b1}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_unpacklo_epi64(a, b);
}

// Interleave high 64-bit halves (as u32x4): {a2, a3, b2, b3}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return _mm_unpackhi_epi64(a, b);
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

// Multiply u32 and return high 32 bits (for fixed-point Q16.16 math, unsigned)
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        u64 prod = static_cast<u64>(a.data[i]) * static_cast<u64>(b.data[i]);
        result.data[i] = static_cast<u32>(prod >> 16);
    }
    return result;
}

// Multiply signed i32 by unsigned-positive u32, return >> 16 (Q16.16 fixed-point)
// Delegates to signed mulhi_i32_4 (scalar fallback has no unsigned advantage)
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    return mulhi_i32_4(a, b);
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

// Extract a single u32 lane from a SIMD vector
FASTLED_FORCE_INLINE FL_IRAM u32 extract_u32_4(simd_u32x4 vec, int lane) noexcept {
    return vec.data[lane];
}

// Interleave low 32-bit elements: {a0, b0, a1, b1}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = b.data[0];
    result.data[2] = a.data[1]; result.data[3] = b.data[1];
    return result;
}

// Interleave high 32-bit elements: {a2, b2, a3, b3}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = b.data[2];
    result.data[2] = a.data[3]; result.data[3] = b.data[3];
    return result;
}

// Interleave low 64-bit halves (as u32x4): {a0, a1, b0, b1}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = a.data[1];
    result.data[2] = b.data[0]; result.data[3] = b.data[1];
    return result;
}

// Interleave high 64-bit halves (as u32x4): {a2, a3, b2, b3}
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = a.data[3];
    result.data[2] = b.data[2]; result.data[3] = b.data[3];
    return result;
}

#endif  // FASTLED_X86_HAS_SSE2

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#endif  // x86/x64
