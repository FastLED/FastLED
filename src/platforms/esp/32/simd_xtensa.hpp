#pragma once

/// @file simd_xtensa.hpp
/// Xtensa-specific SIMD implementations for ESP32 variants (ESP32, S2, S3)
///
/// Provides atomic SIMD operations for Xtensa processors using PIE (Processor Interface Extension).
/// Currently uses scalar fallback - PIE intrinsics to be added later.

#include "fl/stl/stdint.h"

#if defined(__XTENSA__)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/stl/math.h"  // for sqrtf

// Xtensa PIE intrinsics (available on ESP32, ESP32-S2, ESP32-S3)
#if __has_include(<xtensa/tie/xt_PIE.h>)
  #include <xtensa/tie/xt_PIE.h>
  #define FASTLED_XTENSA_HAS_PIE 1
#else
  #define FASTLED_XTENSA_HAS_PIE 0
#endif

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platform {

//==============================================================================
// SIMD Register Types
//==============================================================================

// For Xtensa, use simple struct until PIE intrinsics are verified
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
// Atomic Load/Store Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const uint8_t* ptr) noexcept {
    // PIE-ready: Can be replaced with ee.vld.128.ip / ee.vldbc.8
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(uint8_t* ptr, simd_u8x16 vec) noexcept {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const uint32_t* ptr) noexcept {
    // PIE-ready: Can be replaced with ee.vld.128.ip / ee.vldbc.32
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(uint32_t* ptr, simd_u32x4 vec) noexcept {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) noexcept {
    // PIE-ready: Can be replaced with ee.vld.128.ip
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) noexcept {
    // PIE-ready: Can be replaced with ee.vst.128.ip
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// Atomic Arithmetic Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with ee.vadds.u8
    for (int i = 0; i < 16; ++i) {
        uint16_t sum = static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<uint8_t>(sum);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, uint8_t scale) noexcept {
    if (scale == 255) {
        return vec;
    }

    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with ee.vmulas.u8.qacc
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(uint32_t value) noexcept {
    // PIE-ready: Can be replaced with ee.vldbc.32 (broadcast load)
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, uint8_t amount) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE vector subtract, multiply, shift, and add operations
    // result = a + ((b - a) * amount) / 256
    for (int i = 0; i < 16; ++i) {
        int16_t diff = static_cast<int16_t>(b.data[i]) - static_cast<int16_t>(a.data[i]);
        int16_t scaled = (diff * amount) >> 8;
        result.data[i] = static_cast<uint8_t>(a.data[i] + scaled);
    }
    return result;
}

//==============================================================================
// Atomic Bitwise Operations
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


FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with ee.vsubs.u8
    for (int i = 0; i < 16; ++i) {
        int16_t diff = static_cast<int16_t>(a.data[i]) - static_cast<int16_t>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<uint8_t>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE averaging instructions
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with PIE rounding average instructions
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i]) + 1) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with ee.vmin.u8
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // PIE-ready: This loop can be replaced with ee.vmax.u8
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

//==============================================================================
// Float32 SIMD Operations (Xtensa/PIE-ready)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) noexcept {
    // PIE-ready: Can be replaced with ee.vldbc.32 (broadcast load)
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // PIE-ready: Can be replaced with PIE vector add operation
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // PIE-ready: Can be replaced with PIE vector subtract operation
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // PIE-ready: Can be replaced with PIE vector multiply operation
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


}  // namespace platform
}  // namespace simd
}  // namespace fl

#endif  // defined(__XTENSA__)
