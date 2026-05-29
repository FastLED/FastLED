#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"  // IWYU pragma: keep
#include "platforms/is_platform.h"  // IWYU pragma: keep  (FL_IS_CLANG)

/// @file simd_riscv.hpp
/// ESP32 RISC-V-specific SIMD implementations
///
/// Provides atomic SIMD operations for ESP32 RISC-V processors (C2, C3, C5, C6, H2, P4).
/// Currently uses scalar fallback - RISC-V vector extensions (RVV) could be added in the future.

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/align.h"  // IWYU pragma: keep

#if defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)

#include "fl/stl/has_include.h"
#include "fl/stl/align.h"
#include "fl/math/math.h"  // for sqrtf
#include "fl/stl/noexcept.h"

// RISC-V vector extensions (RVV) support detection
// Note: Current ESP32 RISC-V variants do not include RVV hardware.
// The header <riscv_vector.h> may exist in the toolchain but will fail to compile
// without the vector extension enabled, so we do not include it.
#define FASTLED_ESP32_RISCV_HAS_RVV 0

// ESP32-P4 PIE (xesppie) 128-bit SIMD coprocessor detection (issue #2536).
// __riscv_xesppie is predefined by GCC when -march=...xesppie is active; the PIE
// inline asm is GCC-only, so clang and the host build fall back to scalar.
// Only the revision-stable LCD ops (load/store + bitwise) are routed through PIE
// here; saturation/rounding/float ops stay scalar (see #2535 for the analysis).
#if defined(__riscv_xesppie) && !defined(FL_IS_CLANG)
#  define FL_RISCV_HAS_PIE 1
#else
#  define FL_RISCV_HAS_PIE 0
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

// For ESP32 RISC-V, use simple struct until RVV intrinsics are needed
// Future optimization: Replace with vint8m1_t / vuint32m1_t when RVV is available
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
// ESP32-P4 PIE (xesppie) 128-bit primitives (issue #2536)
//==============================================================================
#if FL_RISCV_HAS_PIE
// `esp.vld.128.ip` / `esp.vst.128.ip` require their base address register to be
// in the RVC compressed set (x8-x15), so each pointer is pinned to a0/a1/a2 via
// a local register variable; a plain "r" constraint lets GCC pick an illegal
// register (a6/a7/t0/...) and the assembler rejects it. All buffers must be
// 16-byte aligned — the simd_* structs are FL_ALIGNAS(16) and the aligned
// load/store contract guarantees it.

FASTLED_FORCE_INLINE FL_IRAM void pie_and_128(const void* a, const void* b, void* out) FL_NOEXCEPT {
    register const void* pa asm("a0") = a;
    register const void* pb asm("a1") = b;
    register void* pr asm("a2") = out;
    asm volatile(
        "esp.vld.128.ip q0, %[pa], 0\n"
        "esp.vld.128.ip q1, %[pb], 0\n"
        "esp.andq q2, q0, q1\n"
        "esp.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory");
}

FASTLED_FORCE_INLINE FL_IRAM void pie_or_128(const void* a, const void* b, void* out) FL_NOEXCEPT {
    register const void* pa asm("a0") = a;
    register const void* pb asm("a1") = b;
    register void* pr asm("a2") = out;
    asm volatile(
        "esp.vld.128.ip q0, %[pa], 0\n"
        "esp.vld.128.ip q1, %[pb], 0\n"
        "esp.orq q2, q0, q1\n"
        "esp.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory");
}

FASTLED_FORCE_INLINE FL_IRAM void pie_xor_128(const void* a, const void* b, void* out) FL_NOEXCEPT {
    register const void* pa asm("a0") = a;
    register const void* pb asm("a1") = b;
    register void* pr asm("a2") = out;
    asm volatile(
        "esp.vld.128.ip q0, %[pa], 0\n"
        "esp.vld.128.ip q1, %[pb], 0\n"
        "esp.xorq q2, q0, q1\n"
        "esp.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory");
}

FASTLED_FORCE_INLINE FL_IRAM void pie_andnot_128(const void* a, const void* b, void* out) FL_NOEXCEPT {
    // (~a) & b
    register const void* pa asm("a0") = a;
    register const void* pb asm("a1") = b;
    register void* pr asm("a2") = out;
    asm volatile(
        "esp.vld.128.ip q0, %[pa], 0\n"
        "esp.vld.128.ip q1, %[pb], 0\n"
        "esp.notq q0, q0\n"
        "esp.andq q2, q0, q1\n"
        "esp.vst.128.ip q2, %[pr], 0\n"
        : [pa] "+r"(pa), [pb] "+r"(pb), [pr] "+r"(pr)
        :
        : "memory");
}

FASTLED_FORCE_INLINE FL_IRAM void pie_copy_128(const void* src, void* dst) FL_NOEXCEPT {
    register const void* ps asm("a0") = src;
    register void* pd asm("a1") = dst;
    asm volatile(
        "esp.vld.128.ip q0, %[ps], 0\n"
        "esp.vst.128.ip q0, %[pd], 0\n"
        : [ps] "+r"(ps), [pd] "+r"(pd)
        :
        : "memory");
}
#endif  // FL_RISCV_HAS_PIE

//==============================================================================
// Atomic Load/Store Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vle8.v
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vse8.v
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) FL_NOEXCEPT {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vle32.v
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vse32.v
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) FL_NOEXCEPT {
    simd_f32x4 result;
    // RVV-ready: This loop can be replaced with vle32.v
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vse32.v
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// Atomic Arithmetic Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vsaddu.vv (saturating add)
    for (int i = 0; i < 16; ++i) {
        u16 sum = static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]);
        result.data[i] = (sum > 255) ? 255 : static_cast<u8>(sum);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwmulu.vx + vnsrl.wi (widening multiply + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) FL_NOEXCEPT {
    simd_u32x4 result;
    // RVV-ready: This loop can be replaced with vmv.v.x
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) FL_NOEXCEPT {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
#if FL_RISCV_HAS_PIE
    pie_and_128(a.data, b.data, result.data);
#else
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
#if FL_RISCV_HAS_PIE
    pie_or_128(a.data, b.data, result.data);
#else
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
#if FL_RISCV_HAS_PIE
    pie_xor_128(a.data, b.data, result.data);
#else
    for (int i = 0; i < 16; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
#if FL_RISCV_HAS_PIE
    pie_andnot_128(a.data, b.data, result.data);
#else
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (~a.data[i]) & b.data[i];
    }
#endif
    return result;
}


FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vssubu.vv (saturating subtract)
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(a.data[i]) - static_cast<i16>(b.data[i]);
        result.data[i] = (diff < 0) ? 0 : static_cast<u8>(diff);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vnsrl.wi (widening add + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i])) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vwaddu.vv + vadd.vi + vnsrl.wi (widening add + add 1 + narrow shift)
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]) + 1) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 result;
    // RVV-ready: This loop can be replaced with vminu.vv
    for (int i = 0; i < 16; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 max_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
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

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vmv.v.x
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = value;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfadd.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] + b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfsub.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] - b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfmul.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] * b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfdiv.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] / b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfsqrt.v
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = fl::sqrtf(vec.data[i]);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vfmin.vv
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
#if FL_RISCV_HAS_PIE
    pie_xor_128(a.data, b.data, result.data);
#else
    // RVV-ready: This loop can be replaced with vxor.vv
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] ^ b.data[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vadd.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i + b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vsub.vv
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        result.data[i] = static_cast<u32>(a_i - b_i);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
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

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    // RVV-ready: This loop can be replaced with vsrl.vx
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = vec.data[i] >> shift;
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
#if FL_RISCV_HAS_PIE
    pie_and_128(a.data, b.data, result.data);
#else
    // RVV-ready: This loop can be replaced with vand.vv
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] & b.data[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) FL_NOEXCEPT {
    const u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    simd_u32x4 result;
#if FL_RISCV_HAS_PIE
    pie_copy_128(p, result.data);
#else
    for (int i = 0; i < 4; ++i) {
        result.data[i] = p[i];
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
#if FL_RISCV_HAS_PIE
    pie_copy_128(vec.data, p);
#else
    for (int i = 0; i < 4; ++i) {
        p[i] = vec.data[i];
    }
#endif
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
#if FL_RISCV_HAS_PIE
    pie_or_128(a.data, b.data, result.data);
#else
    for (int i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] | b.data[i];
    }
#endif
    return result;
}

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
#if __riscv_xlen == 32
    // RV32: Use mul+mulhu inline asm to avoid 64-bit widening multiply.
    for (int i = 0; i < 4; ++i) {
        u32 lo, hi;
        asm("mul   %0, %2, %3\n\t"
            "mulhu %1, %2, %3"
            : "=&r"(lo), "=&r"(hi)
            : "r"(a.data[i]), "r"(b.data[i]));
        result.data[i] = (lo >> 16) | (hi << 16);
    }
#else
    for (int i = 0; i < 4; ++i) {
        u64 prod = static_cast<u64>(a.data[i]) * static_cast<u64>(b.data[i]);
        result.data[i] = static_cast<u32>(prod >> 16);
    }
#endif
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_su32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    return mulhi_i32_4(a, b);
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi32_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
#if __riscv_xlen == 32
    // RV32: >> 32 means we only need the high word — single mulh instruction.
    for (int i = 0; i < 4; ++i) {
        i32 hi;
        asm("mulh %0, %1, %2"
            : "=r"(hi)
            : "r"(a.data[i]), "r"(b.data[i]));
        result.data[i] = static_cast<u32>(hi);
    }
#else
    for (int i = 0; i < 4; ++i) {
        i32 ai = static_cast<i32>(a.data[i]);
        i32 bi = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(ai) * static_cast<i64>(bi);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 32));
    }
#endif
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

#endif  // FL_IS_ESP32_RISCV
