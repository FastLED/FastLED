#pragma once

/// @file simd_x86.hpp
/// x86/x64 SIMD implementations using SSE2/AVX intrinsics
///
/// Provides atomic SIMD operations for x86/x64 processors.
/// SSE2 baseline ensures compatibility with all x64 and most modern x86 processors.

#include "fl/stl/stdint.h"

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
namespace platform {

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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const uint8_t* ptr) noexcept {
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(uint8_t* ptr, simd_u8x16 vec) noexcept {
    _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), vec); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const uint32_t* ptr) noexcept {
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(uint32_t* ptr, simd_u32x4 vec) noexcept {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, uint8_t scale) noexcept {
    if (scale == 255) {
        return vec;  // Identity
    }
    if (scale == 0) {
        return _mm_setzero_si128();
    }

    // SSE2 approach: unpack bytes to 16-bit, multiply, shift, pack
    // For simplicity in initial implementation, use scalar loop
    // TODO: Optimize with proper SSE2 unpack/multiply/pack sequence
    alignas(16) uint8_t data[16];
    _mm_store_si128(reinterpret_cast<__m128i*>(data), vec); // ok reinterpret cast

    for (int i = 0; i < 16; ++i) {
        data[i] = static_cast<uint8_t>((static_cast<uint16_t>(data[i]) * scale) >> 8);
    }

    return _mm_load_si128(reinterpret_cast<const __m128i*>(data)); // ok reinterpret cast
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(uint32_t value) noexcept {
    return _mm_set1_epi32(static_cast<int32_t>(value));
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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, uint8_t amount) noexcept {
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
    __m128i amount_16 = _mm_set1_epi16(static_cast<int16_t>(amount));

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

#else

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

#endif

}  // namespace platform
}  // namespace simd
}  // namespace fl

#endif  // x86/x64
