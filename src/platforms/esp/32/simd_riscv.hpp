#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"  // IWYU pragma: keep

/// @file simd_riscv.hpp
/// ESP32 RISC-V-specific SIMD implementations
///
/// Provides atomic SIMD operations for ESP32 RISC-V processors (C2, C3, C5, C6, H2, P4).
/// Currently uses scalar fallback - RISC-V vector extensions (RVV) could be added in the future.

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/align.h"  // IWYU pragma: keep

#if defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)

#include "fl/force_inline.h"
#include "fl/compiler_control.h"
#include "fl/assume_aligned.h"
#include "fl/has_include.h"
#include "fl/stl/math.h"  // for sqrtf

// RISC-V vector extensions (RVV) support detection
// Note: Current ESP32 RISC-V variants do not include RVV hardware.
// The header <riscv_vector.h> may exist in the toolchain but will fail to compile
// without the vector extension enabled, so we do not include it.
#define FASTLED_ESP32_RISCV_HAS_RVV 0

//==============================================================================
// Platform Implementation Namespace
//==============================================================================

namespace fl {
namespace simd {
namespace platforms {

//==============================================================================
// SIMD Register Types
//==============================================================================

// For ESP32 RISC-V, use simple struct until RVV intrinsics are needed
// Future optimization: Replace with vint8m1_t / vuint32m1_t when RVV is available
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
// Atomic Load/Store Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vle8.v
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) noexcept {
    // RVV-ready: This loop can be replaced with vse8.v
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) noexcept {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vle32.v
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) noexcept {
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
        u16 sum = static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<u8>(sum);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwmulu.vx + vnsrl.wi (widening multiply + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) noexcept {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vmv.v.x
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with RVV vector subtract, widening multiply, shift, and add
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
        i16 diff = static_cast<i16>(a.data[i]) - static_cast<i16>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<u8>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vnsrl.wi (widening add + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) noexcept {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vadd.vi + vnsrl.wi (widening add + add 1 + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]) + 1) >> 1);
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

//==============================================================================
// Int32 SIMD Operations (Scalar Fallback)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vxor.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vadd.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i + b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vsub.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i - b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vmulh.vv (signed multiply high)
    simd_u32x4 result;
#if __riscv_xlen == 32
    // RV32: Use mul+mulh inline asm to avoid 64-bit widening multiply.
    // Extracts bits [47:16] of each 64-bit product for Q16.16 fixed-point.
    for (int i = 0; i < 4; ++i) {
        i32 lo, hi;
        asm("mul  %0, %2, %3\n\t"
            "mulh %1, %2, %3"
            : "=&r"(lo), "=&r"(hi)
            : "r"(a.data[i]), "r"(b.data[i]));
        result.data[i] = (static_cast<u32>(lo) >> 16) | (static_cast<u32>(hi) << 16);
    }
#else
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(a_i) * static_cast<i64>(b_i);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 16));
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) noexcept {
    // RVV-ready: This loop can be replaced with vsrl.vx
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) noexcept {
    // RVV-ready: This loop can be replaced with vand.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];
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

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) noexcept {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    for (int i = 0; i < 4; ++i) {
        p[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) noexcept {
    simd_u32x4 result;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
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

}  // namespace platforms
}  // namespace simd
}  // namespace fl

#endif  // FL_IS_ESP32_RISCV
