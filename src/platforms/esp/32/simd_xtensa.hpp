#pragma once

// IWYU pragma: private

/// @file simd_xtensa.hpp
/// Xtensa-specific SIMD implementations for ESP32 variants (ESP32, S2, S3)
///
/// Provides atomic SIMD operations for Xtensa processors using PIE (Processor Interface Extension).
/// When FL_XTENSA_HAS_PIE=1 (ESP32-S3), uses PIE inline assembly for 128-bit vector operations.
/// Operations without PIE equivalents (i32 arithmetic, float, shifts) use scalar fallback.

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/align.h"  // IWYU pragma: keep

#if defined(__XTENSA__)

#include "fl/stl/has_include.h"
#include "fl/math/math.h"  // for sqrtf

// Xtensa PIE intrinsics (available on ESP32, ESP32-S2, ESP32-S3)
#if FL_HAS_INCLUDE(<xtensa/tie/xt_PIE.h>)
  #include <xtensa/tie/xt_PIE.h>  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
  #define FL_XTENSA_HAS_PIE 1
#else
  #define FL_XTENSA_HAS_PIE 0
#endif

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platforms {

//==============================================================================
// SIMD Register Types
//==============================================================================

// For Xtensa, use simple struct until PIE intrinsics are verified
struct FL_ALIGNAS(16) simd_u8x16 {
    u8 data[16];
};

struct FL_ALIGNAS(16) simd_u16x8 {
    u16 data[8];
};

struct FL_ALIGNAS(16) simd_u32x4 {
    u32 data[4];
};

struct FL_ALIGNAS(16) simd_f32x4 {
    float data[4];
};

//==============================================================================
// Atomic Load/Store Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vld.128.ip / ee.vldbc.8
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vld.128.ip / ee.vldbc.32
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vld.128.ip
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// Atomic Arithmetic Operations
//==============================================================================

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.vadds.u8 q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        u16 sum = static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<u8>(sum);
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) FL_NOEXCEPT {
    simd_u8x16 result;
    FL_ALIGNAS(4) u8 scale_tmp = scale;
    const u8* pv = vec.data;
    const u8* ps = &scale_tmp;
    u8* pr = result.data;
    asm volatile(
        "ee.zero.qacc\n"                  // Clear 320-bit QACC accumulator
        "ee.vld.128.ip q0, %[pv], 0\n"   // q0 = vec (16 x u8)
        "ee.vldbc.8 q1, %[ps]\n"         // q1 = broadcast(scale) to 16 lanes
        "ee.vmulas.u8.qacc q0, q1\n"     // QACC[i] += vec[i] * scale (16-bit per lane)
        "movi a2, 8\n"                    // shift amount
        "wsr.sar a2\n"                    // SAR = 8 (controls srcmb shift)
        "ee.srcmb.s8.qacc q2, q0, 0\n"   // q2 = (QACC >> SAR) packed to 8-bit
        "ee.vst.128.ip q2, %[pr], 0\n"   // store result
        : [pv] "+r"(pv), [ps] "+r"(ps), [pr] "+r"(pr)
        :
        : "memory", "a2"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) FL_NOEXCEPT {
    simd_u32x4 result;
    // Put value on stack so ee.vldbc.32 can broadcast it
    FL_ALIGNAS(4) u32 tmp = value;
    const u32* p = &tmp;
    u32* pr = result.data;
    asm volatile(
        "ee.vldbc.32 q0, %[p]\n"
        "ee.vst.128.ip q0, %[pr], 0\n"
        : [p] "+r"(p), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) FL_NOEXCEPT {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE vector subtract, multiply, shift, and add operations
    // result = a + ((b - a) * amount) / 256
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(b.data[i]) - static_cast<i16>(a.data[i]);
        i16 scaled = (diff * amount) >> 8;
        result.data[i] = static_cast<u8>(a.data[i] + scaled);
    }
    return result;
}

//==============================================================================
// Atomic Bitwise Operations
//==============================================================================

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.andq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.orq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.xorq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    // andnot = ~a & b
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.notq q0, q0\n"
        "ee.andq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else  // !FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (~a.data[i]) & b.data[i];
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE


#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.vsubs.u8 q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? (a.data[i] - b.data[i]) : 0;
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE averaging instructions
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE rounding average instructions
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]) + 1) >> 1);
    }
    return result;
}

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.vmin.u8 q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    const u8* pa = a.data;
    const u8* pb = b.data;
    u8* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.vmax.u8 q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

//==============================================================================
// Float32 SIMD Operations (Xtensa/PIE-ready)
//==============================================================================

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) FL_NOEXCEPT {
    simd_f32x4 result;
    FL_ALIGNAS(4) float tmp = value;
    const float* p = &tmp;
    float* pr = result.data;
    asm volatile(
        "ee.vldbc.32 q0, %[p]\n"
        "ee.vst.128.ip q0, %[pr], 0\n"
        : [p] "+r"(p), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with PIE vector add operation
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with PIE vector subtract operation
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // PIE-ready: Can be replaced with PIE vector multiply operation
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] * b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] / b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = fl::sqrtf(vec.data[i]);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

//==============================================================================
// Int32 SIMD Operations (Scalar Fallback)
//==============================================================================

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    const u32* pa = a.data;
    const u32* pb = b.data;
    u32* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.xorq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i + b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i - b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(a_i) * static_cast<i64>(b_i);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 16));
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;
    }
    return result;
}

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    const u32* pa = a.data;
    const u32* pb = b.data;
    u32* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.andq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) FL_NOEXCEPT {
    simd_u32x4 result;
    const u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    u32* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[p], 0\n"
        "ee.vst.128.ip q0, %[pr], 0\n"
        : [p] "+r"(p), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    const u32* pv = vec.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pv], 0\n"
        "ee.vst.128.ip q0, %[p], 0\n"
        : [pv] "+r"(pv), [p] "+r"(p)
        :
        : "memory"
    );
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) FL_NOEXCEPT {
    const u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = p[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    for (int i = 0; i < 4; ++i) {
        p[i] = vec.data[i];
    }
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
    return result;
}

#if FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    const u32* pa = a.data;
    const u32* pb = b.data;
    u32* pr = result.data;
    asm volatile(
        "ee.vld.128.ip q0, %[pa], 0\n"
        "ee.vld.128.ip q1, %[pb], 0\n"
        "ee.orq q2, q0, q1\n"
        "ee.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory"
    );
    return result;
}

#else

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
    return result;
}

#endif  // FL_XTENSA_HAS_PIE

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sll_u32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] << shift;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sra_i32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 signed_val = static_cast<i32>(vec.data[i]);
        result.data[i] = static_cast<u32>(signed_val >> shift);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        u64 prod = static_cast<u64>(a.data[i]) * static_cast<u64>(b.data[i]);
        result.data[i] = static_cast<u32>(prod >> 16);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    return mulhi_i32_4(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi32_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(ai) * static_cast<i64>(bi);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 32));
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 min_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(ai < bi ? ai : bi);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 max_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(ai > bi ? ai : bi);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM u32 extract_u32_4(simd_u32x4 vec, int lane) FL_NOEXCEPT {
    return vec.data[lane];
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = b.data[0];
    result.data[2] = a.data[1]; result.data[3] = b.data[1];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = b.data[2];
    result.data[2] = a.data[3]; result.data[3] = b.data[3];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpacklo_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a.data[0]; result.data[1] = a.data[1];
    result.data[2] = b.data[0]; result.data[3] = b.data[1];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 unpackhi_u64_as_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a.data[2]; result.data[1] = a.data[3];
    result.data[2] = b.data[2]; result.data[3] = b.data[3];
    return result;
}

//==============================================================================
// u16x8 Operations (Scalar)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 widen_lo_u8_to_u16(simd_u8x16 vec) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i) result.data[i] = vec.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 widen_hi_u8_to_u16(simd_u8x16 vec) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i) result.data[i] = vec.data[i + 8];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 narrow_u16_to_u8(simd_u16x8 lo, simd_u16x8 hi) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 8; ++i)
        result.data[i] = lo.data[i] > 255 ? 255 : static_cast<u8>(lo.data[i]);
    for (int i = 0; i < 8; ++i)
        result.data[i + 8] = hi.data[i] > 255 ? 255 : static_cast<u8>(hi.data[i]);
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 add_u16_8(simd_u16x8 a, simd_u16x8 b) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i) result.data[i] = a.data[i] + b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 mullo_u16_8(simd_u16x8 a, simd_u16x8 b) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i)
        result.data[i] = static_cast<u16>(static_cast<u32>(a.data[i]) * b.data[i]);
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 srli_u16_8(simd_u16x8 vec, int shift) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i) result.data[i] = vec.data[i] >> shift;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 set1_u16_8(u16 value) FL_NOEXCEPT {
    simd_u16x8 result;
    for (int i = 0; i < 8; ++i) result.data[i] = value;
    return result;
}


//==============================================================================
// 256-bit Types and Operations (pair of 128-bit)
//==============================================================================

struct simd_u8x32 {
    simd_u8x16 lo, hi;
};

struct simd_u16x16 {
    simd_u16x8 lo, hi;
};

FASTLED_FORCE_INLINE FL_IRAM simd_u8x32 load_u8_32(const u8* ptr) FL_NOEXCEPT {
    return { load_u8_16(ptr), load_u8_16(ptr + 16) };
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_32(u8* ptr, simd_u8x32 vec) FL_NOEXCEPT {
    store_u8_16(ptr, vec.lo);
    store_u8_16(ptr + 16, vec.hi);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x32 avg_round_u8_32(simd_u8x32 a, simd_u8x32 b) FL_NOEXCEPT {
    return { avg_round_u8_16(a.lo, b.lo), avg_round_u8_16(a.hi, b.hi) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 widen_lo_u8x32_to_u16(simd_u8x32 vec) FL_NOEXCEPT {
    return { widen_lo_u8_to_u16(vec.lo), widen_hi_u8_to_u16(vec.lo) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 widen_hi_u8x32_to_u16(simd_u8x32 vec) FL_NOEXCEPT {
    return { widen_lo_u8_to_u16(vec.hi), widen_hi_u8_to_u16(vec.hi) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x32 narrow_u16x16_to_u8(simd_u16x16 lo, simd_u16x16 hi) FL_NOEXCEPT {
    return { narrow_u16_to_u8(lo.lo, lo.hi), narrow_u16_to_u8(hi.lo, hi.hi) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 add_u16_16(simd_u16x16 a, simd_u16x16 b) FL_NOEXCEPT {
    return { add_u16_8(a.lo, b.lo), add_u16_8(a.hi, b.hi) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 mullo_u16_16(simd_u16x16 a, simd_u16x16 b) FL_NOEXCEPT {
    return { mullo_u16_8(a.lo, b.lo), mullo_u16_8(a.hi, b.hi) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 srli_u16_16(simd_u16x16 vec, int shift) FL_NOEXCEPT {
    return { srli_u16_8(vec.lo, shift), srli_u16_8(vec.hi, shift) };
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x16 set1_u16_16(u16 value) FL_NOEXCEPT {
    auto v = set1_u16_8(value);
    return { v, v };
}

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#endif  // defined(__XTENSA__)
