#pragma once

// Compile-time PROGMEM gamma LUT generator for C++11.
//
// Usage:
//   typedef fl::ProgmemLUT<fl::GammaEval<fl::gamma<fl::u8x24>(2.2f)>, 256> Gamma22;
//   uint8_t corrected = Gamma22::read(pixel);
//
// The gamma table is generated entirely at compile time and stored in
// PROGMEM (flash) on AVR/ESP platforms. No runtime floating-point math
// is involved in lookups.
//
// Why integer template parameters:
//   C++11 does not allow float or class-type non-type template parameters.
//   gamma<u8x24>(2.2f) converts the float to a constexpr uint32_t (the
//   raw fixed-point representation), which IS a valid NTTP in C++11.
//   This is why the helper returns an integer: it's the only way to
//   encode a floating-point gamma value in a template parameter in C++11.
//   This approach is portable across all embedded toolchains.

#include "fl/stl/int.h"
#include "fl/stl/fixed_point/u8x24.h"
#include "fl/stl/type_traits.h"
#include "fastled_progmem.h"

namespace fl {

// ---------- Public API ----------

// Convert a float gamma value to a fixed-point raw integer suitable for
// use as a template parameter. The Fixed type determines the precision.
//
// Example: gamma<u8x24>(2.2f) returns constexpr uint32_t
template <typename Fixed>
constexpr u32 gamma(float g) {
    return Fixed(g).raw();
}

// Forward declarations
template <u32 GammaRaw> struct GammaEval;
template <typename Fn, fl::size N> struct ProgmemLUT;

// Convenience alias: 256-entry gamma table
template <u32 G>
using GammaTable256 = ProgmemLUT<GammaEval<G>, 256>;

// ---------- Constexpr pow implementation (C++11 compatible) ----------
//
// All functions use the single-return-statement form required by C++11
// constexpr. Branching is done via ternary operators, and multi-step
// computations are decomposed into chains of constexpr function calls.

namespace detail {
namespace gamma_constexpr {

constexpr int FRAC = 24;
constexpr u32 SCALE = 1u << FRAC; // 16777216

// ---- Highest-bit finder (constexpr recursive) ----

constexpr int hb_step(u32 v, int r) {
    return (v & 0xFFFF0000u) ? hb_step(v >> 16, r + 16)
         : (v & 0x0000FF00u) ? hb_step(v >> 8,  r + 8)
         : (v & 0x000000F0u) ? hb_step(v >> 4,  r + 4)
         : (v & 0x0000000Cu) ? hb_step(v >> 2,  r + 2)
         : (v & 0x00000002u) ? r + 1
         : r;
}

constexpr int highest_bit(u32 v) {
    return v == 0 ? -1 : hb_step(v, 0);
}

// ---- Fixed-point log2 (signed result, negative for inputs < 1.0) ----
// Uses 4-term minimax polynomial for log2(1+t), t in [0,1).
// Minimax coefficients minimize max error over the full interval,
// unlike Taylor which diverges badly near t=1.

constexpr u32 log2_t(u32 val, int msb) {
    return msb >= FRAC
        ? (val >> (msb - FRAC)) - SCALE
        : (val << (FRAC - msb)) - SCALE;
}

// Horner steps for log2(1+t) polynomial, split into separate
// constexpr functions for C++11 single-return-statement compliance.
constexpr i64 log2_h3(i64 t) {
    return (static_cast<i64>(-1788416LL) * t) >> FRAC;
}
constexpr i64 log2_h2(i64 t) {
    return ((6098176LL + log2_h3(t)) * t) >> FRAC;
}
constexpr i64 log2_h1(i64 t) {
    return ((-11728384LL + log2_h2(t)) * t) >> FRAC;
}
constexpr i64 log2_h0(i64 t) {
    return ((24189248LL + log2_h1(t)) * t) >> FRAC;
}

constexpr i32 log2_with_msb(u32 val, int msb) {
    return static_cast<i32>(
        (static_cast<i64>(msb - FRAC) << FRAC) +
        log2_h0(static_cast<i64>(log2_t(val, msb)))
    );
}

constexpr i32 log2_fp(u32 val) {
    return val == 0 ? static_cast<i32>(0x80000000)
                    : log2_with_msb(val, highest_bit(val));
}

// ---- Fixed-point exp2 (handles both positive and negative exponents) ----
// Uses 4-term minimax polynomial for 2^t - 1, t in [0,1).
// Minimax coefficients minimize max error over the full interval.

constexpr u64 exp2_h3(u64 fr) {
    return (214016ULL * fr) >> FRAC;
}
constexpr u64 exp2_h2(u64 fr) {
    return ((895232ULL + exp2_h3(fr)) * fr) >> FRAC;
}
constexpr u64 exp2_h1(u64 fr) {
    return ((4038400ULL + exp2_h2(fr)) * fr) >> FRAC;
}
constexpr u64 exp2_h0(u64 fr) {
    return ((11629376ULL + exp2_h1(fr)) * fr) >> FRAC;
}
constexpr u64 exp2_frac(u64 fr) {
    return (1ULL << FRAC) + exp2_h0(fr);
}

// exp2 for non-negative 8.24 fixed-point input.
constexpr u32 exp2_pos(u32 x) {
    return (x >> FRAC) >= 8 ? 0xFFFFFFFFu
        : static_cast<u32>(
            (static_cast<u64>(SCALE << (x >> FRAC)) *
             exp2_frac(x & (SCALE - 1))) >> FRAC
          );
}

// exp2 for negative input: 2^(-P) = 2^(1-f) >> (n+1)
// where P = n + f, n = integer part, f = fractional part.
constexpr u32 exp2_neg(u32 pos_val) {
    return ((pos_val >> FRAC) + 1) >= 32 ? 0
        : static_cast<u32>(
            exp2_pos(SCALE - (pos_val & (SCALE - 1)))
            >> ((pos_val >> FRAC) + 1)
          );
}

constexpr u32 exp2_fp(i32 x) {
    return x >= 0 ? exp2_pos(static_cast<u32>(x))
                  : exp2_neg(static_cast<u32>(-x));
}

// ---- Fixed-point pow: base^exp via exp2(exp * log2(base)) ----

constexpr u32 pow_fp(u32 base_raw, u32 exp_raw) {
    return base_raw == 0     ? 0
         : exp_raw == 0      ? SCALE
         : base_raw == SCALE ? SCALE
         : exp2_fp(static_cast<i32>(
               (static_cast<i64>(exp_raw) *
                static_cast<i64>(log2_fp(base_raw))) >> FRAC
           ));
}

// ---- 8-bit gamma correction for a single pixel value ----
// Computes: clamp(round(pow(x/255, gamma) * 255), 0, 255)

constexpr u8 eval(u8 x, u32 gamma_raw) {
    return x == 0   ? static_cast<u8>(0)
         : x == 255 ? static_cast<u8>(255)
         : static_cast<u8>(
               (pow_fp(
                   static_cast<u32>((static_cast<u64>(x) << FRAC) / 255),
                   gamma_raw
               ) * 255ULL + (SCALE >> 1)) >> FRAC
           );
}

// ---- 16-bit gamma correction for a single pixel value ----
// Computes: clamp(round(pow(x/255, gamma) * 65535), 0, 65535)
// Used by five-bit brightness and HD108 pipelines.

constexpr u16 eval16(u8 x, u32 gamma_raw) {
    return x == 0   ? static_cast<u16>(0)
         : x == 255 ? static_cast<u16>(65535)
         : static_cast<u16>(
               (pow_fp(
                   static_cast<u32>((static_cast<u64>(x) << FRAC) / 255),
                   gamma_raw
               ) * 65535ULL + (SCALE >> 1)) >> FRAC
           );
}

} // namespace gamma_constexpr
} // namespace detail

// ---------- GammaEval functors ----------
// Compile-time gamma evaluation functors. GammaRaw is a fixed-point
// raw value obtained from gamma<u8x24>(float_value).

// 8-bit output: input u8 [0,255] → output u8 [0,255]
template <u32 GammaRaw>
struct GammaEval {
    constexpr u8 operator()(u8 x) const {
        return detail::gamma_constexpr::eval(x, GammaRaw);
    }
};

// 16-bit output: input u8 [0,255] → output u16 [0,65535]
// Use this for five-bit brightness and HD108 pipelines.
template <u32 GammaRaw>
struct GammaEval16 {
    constexpr u16 operator()(u8 x) const {
        return detail::gamma_constexpr::eval16(x, GammaRaw);
    }
};

// ---------- ProgmemLUT generators ----------

namespace detail {

template <typename T, fl::size N>
struct LutArray {
    T values[N];
};

template <typename Fn, fl::size... Is>
constexpr LutArray<u8, sizeof...(Is)>
make_lut_u8(fl::index_sequence<Is...>) {
    return {{ Fn()(static_cast<u8>(Is))... }};
}

template <typename Fn, fl::size... Is>
constexpr LutArray<u16, sizeof...(Is)>
make_lut_u16(fl::index_sequence<Is...>) {
    return {{ Fn()(static_cast<u8>(Is))... }};
}

} // namespace detail

// 8-bit PROGMEM lookup table (u8 input → u8 output).
// Fn must be a functor with constexpr operator()(u8) returning u8.
template <typename Fn, fl::size N>
struct ProgmemLUT {
    static u8 read(u8 index) {
        return FL_PGM_READ_BYTE_NEAR(&kData.values[index]);
    }

    static const detail::LutArray<u8, N> kData;
};

template <typename Fn, fl::size N>
const detail::LutArray<u8, N>
ProgmemLUT<Fn, N>::kData FL_PROGMEM =
    detail::make_lut_u8<Fn>(fl::make_index_sequence<N>{});

// 16-bit PROGMEM lookup table (u8 input → u16 output).
// Fn must be a functor with constexpr operator()(u8) returning u16.
// Used by five-bit brightness and HD108 gamma pipelines.
template <typename Fn, fl::size N>
struct ProgmemLUT16 {
    static u16 read(u8 index) {
        return FL_PGM_READ_WORD_NEAR(&kData.values[index]);
    }

    // Direct access to the raw PROGMEM array for hot-loop usage
    // (avoids per-element function call overhead).
    static const u16* data() { return kData.values; }

    static const detail::LutArray<u16, N> kData;
};

template <typename Fn, fl::size N>
FL_ALIGN_PROGMEM(64) const detail::LutArray<u16, N>
ProgmemLUT16<Fn, N>::kData FL_PROGMEM =
    detail::make_lut_u16<Fn>(fl::make_index_sequence<N>{});

// ---------- Convenience aliases ----------

// 256-entry 8-bit gamma table
template <u32 G>
using GammaTable256 = ProgmemLUT<GammaEval<G>, 256>;

// 256-entry 16-bit gamma table (for five-bit brightness, HD108)
template <u32 G>
using GammaTable16_256 = ProgmemLUT16<GammaEval16<G>, 256>;

// ---------- Pre-defined gamma tables ----------

// 8-bit gamma tables (u8 input → u8 output).
// Usage: uint8_t corrected = fl::Gamma22LUT::read(pixel);
typedef ProgmemLUT<GammaEval<gamma<u8x24>(2.2f)>, 256> Gamma22LUT;
typedef ProgmemLUT<GammaEval<gamma<u8x24>(2.8f)>, 256> Gamma28LUT;

// 16-bit gamma tables (u8 input → u16 output).
// Usage: uint16_t val = fl::Gamma22LUT16::read(pixel);
typedef ProgmemLUT16<GammaEval16<gamma<u8x24>(2.2f)>, 256> Gamma22LUT16;
typedef ProgmemLUT16<GammaEval16<gamma<u8x24>(2.8f)>, 256> Gamma28LUT16;

} // namespace fl
