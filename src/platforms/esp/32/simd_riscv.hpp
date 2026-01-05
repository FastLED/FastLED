#pragma once

/// @file simd_riscv.hpp
/// ESP32 RISC-V-specific SIMD implementations
///
/// Provides atomic SIMD operations for ESP32 RISC-V processors (C2, C3, C5, C6, H2, P4).
/// Currently uses scalar fallback - RISC-V vector extensions (RVV) could be added in the future.

#include "fl/stl/stdint.h"

#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/stl/math.h"  // for sqrtf

// RISC-V vector extensions (RVV) support detection
// Note: Current ESP32 RISC-V variants do not include RVV hardware
// Future variants may support RVV intrinsics via <riscv_vector.h>
#if __has_include(<riscv_vector.h>)
  #include <riscv_vector.h>
  #define FASTLED_ESP32_RISCV_HAS_RVV 1
#else
  #define FASTLED_ESP32_RISCV_HAS_RVV 0
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

// For ESP32 RISC-V, use simple struct until RVV intrinsics are needed
// Future optimization: Replace with vint8m1_t / vuint32m1_t when RVV is available
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
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vle8.v
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(uint8_t* ptr, simd_u8x16 vec) noexcept {
    // RVV-ready: This loop can be replaced with vse8.v
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const uint32_t* ptr) noexcept {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vle32.v
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(uint32_t* ptr, simd_u32x4 vec) noexcept {
    // RVV-ready: This loop can be replaced with vse32.v
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) noexcept {
    simd_f32x4 result;
    // RVV-ready: This loop can be replaced with vle32.v
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) noexcept {
    // RVV-ready: This loop can be replaced with vse32.v
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// Atomic Arithmetic Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vsaddu.vv (saturating add)
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
    // RVV-ready: This loop can be replaced with vwmulu.vx + vnsrl.wi (widening multiply + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(uint32_t value) noexcept {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vmv.v.x
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, uint8_t amount) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with RVV vector subtract, widening multiply, shift, and add
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
    // RVV-ready: This loop can be replaced with vssubu.vv (saturating subtract)
    for (int i = 0; i < 16; ++i) {
        int16_t diff = static_cast<int16_t>(a.data[i]) - static_cast<int16_t>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<uint8_t>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vnsrl.wi (widening add + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vadd.vi + vnsrl.wi (widening add + add 1 + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(a.data[i]) + static_cast<uint16_t>(b.data[i]) + 1) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vminu.vv
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vmaxu.vv
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

//==============================================================================
// Float32 SIMD Operations (RISC-V/RVV-ready)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) noexcept {
    // RVV-ready: This loop can be replaced with vmv.v.x
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfadd.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfsub.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfmul.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] * b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfdiv.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] / b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) noexcept {
    // RVV-ready: This loop can be replaced with vfsqrt.v
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = fl::sqrtf(vec.data[i]);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfmin.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vfmax.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

}  // namespace platform
}  // namespace simd
}  // namespace fl

#endif  // ESP32 RISC-V variants
