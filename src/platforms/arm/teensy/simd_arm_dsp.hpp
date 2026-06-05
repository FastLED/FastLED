// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file simd_arm_dsp.hpp
/// ARM Cortex-M DSP-extension SIMD backend for Teensy 3.x / 4.x
/// (and any other ARMv7E-M target with the DSP extension)
///
/// The ARMv7E-M DSP extension (Cortex-M4 / M4F / M7) exposes a small set of
/// single-cycle 32-bit-register-packed integer ops — 4-lane 8-bit and
/// 2-lane 16-bit arithmetic — accessible via the CMSIS Core_M intrinsics
/// (UQADD8, UQSUB8, UADD8, USUB8, UADD16, USUB16, UQADD16, UQSUB16, ...).
///
/// FastLED's SIMD vectors are 128 bits wide (u8x16, u16x8, u32x4), which
/// is 4 packed 32-bit registers' worth of data. Each Tier-1 byte op becomes
/// a chain of 4 DSP-ext ops, one per 32-bit chunk — giving a 4x throughput
/// win on saturating arithmetic vs the scalar fallback.
///
/// This file is the "minimum useful subset" landing for issue #2628.
/// Currently accelerated (DSP-ext inline assembly):
///   - add_sat_u8_16   (UQADD8 x4)
///   - sub_sat_u8_16   (UQSUB8 x4)
///   - avg_u8_16       (UHADD8 x4)      [floor average]
///   - and_u8_16       (AND x4 — plain 32-bit AND is already optimal)
///   - or_u8_16        (ORR x4)
///   - xor_u8_16       (EOR x4)
///   - andnot_u8_16    (BIC x4)
///   - add_u16_8       (UADD16 x4)
///
/// Everything else (scale, blend, narrow/widen, u32 ops, f32 ops) falls
/// through to scalar reference implementations identical to the noop
/// backend. Future work (tracked in #2628) layers on UQADD16, SMUAD-based
/// scale_u8_16, USAT16-based narrow_u16_to_u8, and a SEL+USUB8-based
/// min/max.
///
/// We deliberately use GCC inline assembly rather than CMSIS intrinsic
/// macros so this header is self-contained — it does not depend on the
/// Teensy core having pulled in <cmsis_gcc.h> / <core_cm4.h>. The DSP-ext
/// instruction encodings are stable ARMv7E-M; the asm syntax accepted here
/// is what both GCC and clang generate when -march=armv7e-m is in effect.

#include "fl/stl/stdint.h"        // IWYU pragma: keep
#include "fl/stl/align.h"          // IWYU pragma: keep
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"       // IWYU pragma: keep
#include "fl/math/math.h"          // for sqrtf (scalar f32 fallback)

// This file is only meaningful when the ARMv7E-M DSP extension is present.
// All Teensy 3.x (Cortex-M4 / M4F) and Teensy 4.x (Cortex-M7) cores have it.
// The macro is set by both GCC and clang when -mcpu / -march selects a
// DSP-enabled core. Apollo3 (Cortex-M4), Adafruit M4 (SAMD51), nRF52, and
// STM32F4/F7 also light this up — they share the same ISA so they
// automatically benefit if/when they route to this header.
#if !defined(__ARM_FEATURE_DSP) || (__ARM_FEATURE_DSP + 0) != 1
    #error "simd_arm_dsp.hpp included on a target without the DSP extension"
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
//
// We store as 16 bytes / 8 halfwords / 4 words; the DSP-ext ops act on 32-bit
// general-purpose registers, so the natural unit for u8x16 is "4 x u32 chunks".
// 16-byte alignment is enforced so callers can land aligned LDM/STM sequences.

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
// DSP-extension inline-asm helpers (scalar 32-bit wrappers)
//==============================================================================

// 4-lane 8-bit unsigned saturating add: result[i] = sat8(a[i] + b[i]) for i in 0..3
FASTLED_FORCE_INLINE FL_IRAM u32 fl_dsp_uqadd8(u32 a, u32 b) FL_NOEXCEPT {
    u32 r;
    __asm__ volatile ("uqadd8 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

// 4-lane 8-bit unsigned saturating sub
FASTLED_FORCE_INLINE FL_IRAM u32 fl_dsp_uqsub8(u32 a, u32 b) FL_NOEXCEPT {
    u32 r;
    __asm__ volatile ("uqsub8 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

// 4-lane 8-bit unsigned halving add: result[i] = (a[i] + b[i]) >> 1
// Exact floor-average — no rounding error from intermediate overflow.
FASTLED_FORCE_INLINE FL_IRAM u32 fl_dsp_uhadd8(u32 a, u32 b) FL_NOEXCEPT {
    u32 r;
    __asm__ volatile ("uhadd8 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

// 2-lane 16-bit unsigned add (modulo, no saturation)
FASTLED_FORCE_INLINE FL_IRAM u32 fl_dsp_uadd16(u32 a, u32 b) FL_NOEXCEPT {
    u32 r;
    __asm__ volatile ("uadd16 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

//==============================================================================
// Load/Store Operations (NOT atomic — no thread-safety / memory-order semantics)
//==============================================================================
//
// We use word-granular loads/stores. The DSP-ext ops want u32-shaped chunks;
// reading byte-at-a-time would defeat the whole point. The base pointer is not
// required to be 4-byte aligned — Cortex-M4/M7 honour unaligned LDR/STR for
// halfword and word loads (CCR.UNALIGN_TRP is off by default in the Teensy
// startup), so a plain memcpy-style copy with the compiler's natural codegen
// is correct and efficient.

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 load_u8_16(const u8* ptr) FL_NOEXCEPT {
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u8_16(u8* ptr, simd_u8x16 vec) FL_NOEXCEPT {
    for (int i = 0; i < 16; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4(const u32* ptr) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 load_u32_4_aligned(const u32* ptr) FL_NOEXCEPT {
    const u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = p[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM void store_u32_4_aligned(u32* ptr, simd_u32x4 vec) FL_NOEXCEPT {
    u32* p = FL_ASSUME_ALIGNED(ptr, 16);
    for (int i = 0; i < 4; ++i) {
        p[i] = vec.data[i];
    }
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 load_f32_4(const float* ptr) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) {
        result.data[i] = ptr[i];
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM void store_f32_4(float* ptr, simd_f32x4 vec) FL_NOEXCEPT {
    for (int i = 0; i < 4; ++i) {
        ptr[i] = vec.data[i];
    }
}

//==============================================================================
// u8x16 Arithmetic — DSP-extension accelerated
//==============================================================================

/// Saturating 16-lane unsigned add via UQADD8 x4.
/// This is the inner loop of fadeBy / nscale8 / blend8 / overlay primitives.
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 add_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    // Reinterpret-cast the byte buffer into u32 chunks. data is 16-byte aligned,
    // so the pointer is at minimum 4-byte aligned for the u32 view.
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = fl_dsp_uqadd8(pa[0], pb[0]);
    pr[1] = fl_dsp_uqadd8(pa[1], pb[1]);
    pr[2] = fl_dsp_uqadd8(pa[2], pb[2]);
    pr[3] = fl_dsp_uqadd8(pa[3], pb[3]);
    return r;
}

/// Saturating 16-lane unsigned sub via UQSUB8 x4.
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 sub_sat_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = fl_dsp_uqsub8(pa[0], pb[0]);
    pr[1] = fl_dsp_uqsub8(pa[1], pb[1]);
    pr[2] = fl_dsp_uqsub8(pa[2], pb[2]);
    pr[3] = fl_dsp_uqsub8(pa[3], pb[3]);
    return r;
}

/// Floor-average via UHADD8 x4. Exact: (a[i] + b[i]) >> 1, no rounding error.
FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = fl_dsp_uhadd8(pa[0], pb[0]);
    pr[1] = fl_dsp_uhadd8(pa[1], pb[1]);
    pr[2] = fl_dsp_uhadd8(pa[2], pb[2]);
    pr[3] = fl_dsp_uhadd8(pa[3], pb[3]);
    return r;
}

//==============================================================================
// u8x16 Arithmetic — scalar fallback (deferred to a follow-up PR)
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 scale_u8_16(simd_u8x16 vec, u8 scale) FL_NOEXCEPT {
    // TODO(#2628 follow-up): replace with PKHBT(scale|scale) + UXTB16 + SMUAD chain.
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(vec.data[i]) * scale) >> 8);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 blend_u8_16(simd_u8x16 a, simd_u8x16 b, u8 amount) FL_NOEXCEPT {
    // TODO(#2628 follow-up): SMUAD-based weighted blend per 4-byte chunk.
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        i16 diff = static_cast<i16>(b.data[i]) - static_cast<i16>(a.data[i]);
        i16 scaled = (diff * amount) >> 8;
        result.data[i] = static_cast<u8>(a.data[i] + scaled);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 avg_round_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    // UHADD8 floors; a "rounding" variant would need UHADD8 + UQADD8(1) pair.
    // Deferred until we wire the full lib8tion DSP-ext path.
    simd_u8x16 result;
    for (int i = 0; i < 16; ++i) {
        result.data[i] = static_cast<u8>((static_cast<u16>(a.data[i]) + static_cast<u16>(b.data[i]) + 1) >> 1);
    }
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 min_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    // TODO(#2628): USUB8 + SEL using the GE flags is the canonical DSP-ext pattern.
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

//==============================================================================
// u8x16 Bitwise — 32-bit-word ops are already optimal on Cortex-M (no DSP needed)
//==============================================================================
//
// AND/ORR/EOR/BIC each consume one 32-bit GP register pair and produce a result
// in one cycle. Operating on 4 word chunks per simd_u8x16 covers all 16 bytes
// in 4 instructions — the compiler will emit exactly this for the loop below.

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 and_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = pa[0] & pb[0];
    pr[1] = pa[1] & pb[1];
    pr[2] = pa[2] & pb[2];
    pr[3] = pa[3] & pb[3];
    return r;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 or_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = pa[0] | pb[0];
    pr[1] = pa[1] | pb[1];
    pr[2] = pa[2] | pb[2];
    pr[3] = pa[3] | pb[3];
    return r;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 xor_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = pa[0] ^ pb[0];
    pr[1] = pa[1] ^ pb[1];
    pr[2] = pa[2] ^ pb[2];
    pr[3] = pa[3] ^ pb[3];
    return r;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u8x16 andnot_u8_16(simd_u8x16 a, simd_u8x16 b) FL_NOEXCEPT {
    // (~a) & b — Cortex-M emits BIC b, a (single cycle).
    simd_u8x16 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = (~pa[0]) & pb[0];
    pr[1] = (~pa[1]) & pb[1];
    pr[2] = (~pa[2]) & pb[2];
    pr[3] = (~pa[3]) & pb[3];
    return r;
}

//==============================================================================
// u16x8 Operations
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 widen_lo_u8_to_u16(simd_u8x16 vec) FL_NOEXCEPT {
    // TODO(#2628 follow-up): UXTB16 x2 packs (b0,b1)->(0,b0,0,b1) etc.
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
    // TODO(#2628 follow-up): USAT16 + PKHBT to pack saturated halfwords back to bytes.
    simd_u8x16 result;
    for (int i = 0; i < 8; ++i)
        result.data[i] = lo.data[i] > 255 ? 255 : static_cast<u8>(lo.data[i]);
    for (int i = 0; i < 8; ++i)
        result.data[i + 8] = hi.data[i] > 255 ? 255 : static_cast<u8>(hi.data[i]);
    return result;
}

/// 8-lane u16 add via UADD16 x4 (each instruction handles 2 lanes).
FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 add_u16_8(simd_u16x8 a, simd_u16x8 b) FL_NOEXCEPT {
    simd_u16x8 r;
    const u32* pa = reinterpret_cast<const u32*>(a.data);  // ok reinterpret cast
    const u32* pb = reinterpret_cast<const u32*>(b.data);  // ok reinterpret cast
    u32* pr = reinterpret_cast<u32*>(r.data);              // ok reinterpret cast
    pr[0] = fl_dsp_uadd16(pa[0], pb[0]);
    pr[1] = fl_dsp_uadd16(pa[1], pb[1]);
    pr[2] = fl_dsp_uadd16(pa[2], pb[2]);
    pr[3] = fl_dsp_uadd16(pa[3], pb[3]);
    return r;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u16x8 mullo_u16_8(simd_u16x8 a, simd_u16x8 b) FL_NOEXCEPT {
    // DSP-ext SMULxy is per-pair-of-halfwords, not vectorised across halfword lanes.
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
// u32x4 / i32x4 Operations — scalar; DSP-ext does not vectorise 32-bit ops
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set1_u32_4(u32 value) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = value;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 set_u32_4(u32 a, u32 b, u32 c, u32 d) FL_NOEXCEPT {
    simd_u32x4 result;
    result.data[0] = a;
    result.data[1] = b;
    result.data[2] = c;
    result.data[3] = d;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 xor_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] ^ b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 add_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] + b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sub_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] - b.data[i];
    return result;
}

// NOTE on `mulhi_*_4` vs `mulhi32_*_4` naming
// --------------------------------------------
// This is the FastLED-wide SIMD API convention (mirrored across simd_noop,
// simd_x86, simd_arm_neon, simd_xtensa, simd_riscv):
//   - mulhi_{i,u}32_4   →  (a * b) >> 16   (low-32 of a 16.16 fixed-point
//                                            multiply — matches the SSE2
//                                            PMULHW-style "high half of a
//                                            16-bit product" semantics that
//                                            FastLED scales/blends rely on)
//   - mulhi32_{i,u}32_4 →  (a * b) >> 32   (true high-32 of the full
//                                            64-bit product — the SMMUL /
//                                            UMMUL semantics)
// Do not rename one without renaming the matching ops in every other backend.
FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 mulhi_i32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    // SMMUL gives (i32 × i32) >> 32 in one cycle; we want >> 16, so use a wide
    // SMULL pair and shift. Scalar long-multiply lets the compiler pick SMULL.
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) {
        i32 a_i = static_cast<i32>(a.data[i]);
        i32 b_i = static_cast<i32>(b.data[i]);
        i64 prod = static_cast<i64>(a_i) * static_cast<i64>(b_i);
        result.data[i] = static_cast<u32>(static_cast<i32>(prod >> 16));
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

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 srl_u32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = vec.data[i] >> shift;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 sll_u32_4(simd_u32x4 vec, int shift) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = vec.data[i] << shift;
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

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 and_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] & b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_u32x4 or_u32_4(simd_u32x4 a, simd_u32x4 b) FL_NOEXCEPT {
    simd_u32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] | b.data[i];
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
// f32x4 Operations — scalar; Cortex-M4F/M7 has scalar VFP, no Helium yet
//==============================================================================

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 set1_f32_4(float value) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = value;
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 add_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] + b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sub_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] - b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 mul_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] * b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 div_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = a.data[i] / b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 sqrt_f32_4(simd_f32x4 vec) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = fl::sqrtf(vec.data[i]);
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 min_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = (a.data[i] < b.data[i]) ? a.data[i] : b.data[i];
    return result;
}

FASTLED_FORCE_INLINE FL_IRAM simd_f32x4 max_f32_4(simd_f32x4 a, simd_f32x4 b) FL_NOEXCEPT {
    simd_f32x4 result;
    for (int i = 0; i < 4; ++i) result.data[i] = (a.data[i] > b.data[i]) ? a.data[i] : b.data[i];
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
