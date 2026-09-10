/// @file simd_fallback.cpp
/// Contract and differential coverage for the scalar SIMD fallback.
///
/// `src/platforms/shared/simd_noop.hpp` is the SIMD backend for AVR, ESP8266,
/// Cortex-M0/M0+/M3, WASM, RP2350, every ARMv8-A part, and anything else
/// `platforms/simd.h` does not match.
///
/// On an **x86** host that dispatch selects `simd_x86.hpp`, so the fallback is
/// reached by nothing: an `#error` placed at the top of it does not fire in
/// the Linux test build. On **Apple Silicon** the opposite holds -- the
/// fallback *is* the selection, and the existing `fl/math/simd.cpp` cases
/// exercise it as the platform backend. So its coverage depended entirely on
/// which machine ran the suite, and the machine most people develop on was
/// the one that skipped it.
///
/// Two kinds of case here, for that reason:
///
/// - **Contract cases** run everywhere. They assert the semantics all six
///   backends agree on, against values written out by hand.
/// - **Differential cases** run only where the host selected something other
///   than the fallback, and check all 66 operations against it. That is the
///   stronger check, because hand-copied constants only restate whichever
///   implementation they were copied from -- but it needs two backends to
///   exist, which `FL_SIMD_BACKEND_IS_FALLBACK` is how to ask.
///
/// Issue #4216 wants the fallback's lane storage rewritten from `u32[4]` to
/// named members, and gave "verifiable on two of many targets" as the reason
/// not to. That rewrite touches all 80 `data[...]` accesses; these cases are
/// what turn it into a refactor with an oracle.

#include "fl/math/simd.h"

#if !FL_SIMD_BACKEND_IS_FALLBACK
// Under a second namespace, so the fallback's types and the selected
// backend's can coexist: `simd_u32x4` is a `u32[4]` here and a `__m128i`
// there, so they cannot share a name in one translation unit.
//
// Only where the host did not already select the fallback. Where it did, the
// test build's precompiled header has included this file already and
// `#pragma once` makes a second include a no-op -- the alias below would then
// name a namespace that was never opened.
#define FL_SIMD_FALLBACK_NAMESPACE platforms_fallback
#include "platforms/shared/simd_noop.hpp"
#endif  // !FL_SIMD_BACKEND_IS_FALLBACK

#include "fl/stl/cstring.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

namespace host = fl::simd::platforms;
#if !FL_SIMD_BACKEND_IS_FALLBACK
namespace fallback = fl::simd::platforms_fallback;
#endif

#if !FL_SIMD_BACKEND_IS_FALLBACK
// Everything below is used only by the differential cases, which the
// same condition guards. Left outside it, an unused corpus fails the
// build under -Werror=unused-const-variable on a host that selects the
// fallback -- which is exactly where the differential does not run.
// Inputs chosen to reach the edges the two implementations are most likely to
// disagree on: saturation limits, sign boundaries for the ops that read the
// lanes as signed, and zero.
const fl::u8 kA8[32] = {0,   1,   2,   127, 128, 129, 254, 255,
                        3,   64,  100, 200, 250, 17,  99,  188,
                        255, 0,   128, 127, 1,   254, 65,  33,
                        7,   240, 15,  90,  180, 45,  210, 5};
const fl::u8 kB8[32] = {255, 254, 1,   128, 127, 2,   1,   255,
                        0,   64,  155, 55,  6,   238, 156, 67,
                        1,   255, 127, 128, 254, 1,   190, 222,
                        248, 15,  240, 165, 75,  210, 45,  250};

const fl::u32 kA32[4] = {0u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
const fl::u32 kB32[4] = {0xFFFFFFFFu, 0x00010000u, 0x7FFFFFFFu, 0x00000002u};
const fl::u32 kC32[4] = {0x12345678u, 0x80000001u, 0x00000003u, 0xCAFEBABEu};

const float kAf[4] = {0.0f, 1.5f, -2.25f, 1e6f};
const float kBf[4] = {4.0f, -0.5f, 2.0f, 0.25f};

// Round-trip helpers. The register types differ between the two backends --
// `simd_u32x4` is a `u32[4]` in the fallback and a `__m128i` on x86 -- so
// everything is compared through memory, which is also the only thing the
// dispatch's callers can observe.
#define FL_SAME_U8_16(expr_host, expr_fallback)                                \
    do {                                                                       \
        fl::u8 out_h[16] = {0}, out_f[16] = {0};                               \
        host::store_u8_16(out_h, (expr_host));                                 \
        fallback::store_u8_16(out_f, (expr_fallback));                         \
        for (int i = 0; i < 16; ++i) {                                         \
            FL_CHECK_EQ(int(out_f[i]), int(out_h[i]));                         \
        }                                                                      \
    } while (0)

#define FL_SAME_U32_4(expr_host, expr_fallback)                                \
    do {                                                                       \
        fl::u32 out_h[4] = {0}, out_f[4] = {0};                                \
        host::store_u32_4(out_h, (expr_host));                                 \
        fallback::store_u32_4(out_f, (expr_fallback));                         \
        for (int i = 0; i < 4; ++i) {                                          \
            FL_CHECK_EQ(out_f[i], out_h[i]);                                   \
        }                                                                      \
    } while (0)

#define FL_SAME_F32_4(expr_host, expr_fallback)                                \
    do {                                                                       \
        float out_h[4] = {0}, out_f[4] = {0};                                  \
        host::store_f32_4(out_h, (expr_host));                                 \
        fallback::store_f32_4(out_f, (expr_fallback));                         \
        for (int i = 0; i < 4; ++i) {                                          \
            FL_CHECK_EQ(out_f[i], out_h[i]);                                   \
        }                                                                      \
    } while (0)

#define FL_SAME_U8_32(expr_host, expr_fallback)                                \
    do {                                                                       \
        fl::u8 out_h[32] = {0}, out_f[32] = {0};                               \
        host::store_u8_32(out_h, (expr_host));                                 \
        fallback::store_u8_32(out_f, (expr_fallback));                         \
        for (int i = 0; i < 32; ++i) {                                         \
            FL_CHECK_EQ(int(out_f[i]), int(out_h[i]));                         \
        }                                                                      \
    } while (0)

// u16x8 and u16x16 have no store of their own in the API, and comparing them
// by narrowing back down does not work on its own: `narrow_*` saturates, so
// every lane above 255 maps to 255 and any two of them compare equal.
// `0x8000` and `0x8100` would have passed identically, and a later
// `srli_u16_8(v, 8)` makes that 128 against 129.
//
// So the lanes are read out of the register directly. Both backends' types
// are trivially-copyable storage of the same size -- a `u16[8]` in the
// fallback, a `__m128i` on x86 -- and `memcpy` is the portable way to look at
// either without an aligned-load requirement or a strict-aliasing violation.
template <typename Vector>
void readLanes(const Vector& vec, fl::u16* out, int count) {
    fl::memcpy(out, &vec, sizeof(fl::u16) * static_cast<fl::size>(count));
}

#define FL_SAME_U16_8(expr_host, expr_fallback)                                \
    do {                                                                       \
        fl::u16 out_h[8] = {0}, out_f[8] = {0};                                \
        readLanes((expr_host), out_h, 8);                                      \
        readLanes((expr_fallback), out_f, 8);                                  \
        for (int i = 0; i < 8; ++i) {                                          \
            FL_CHECK_EQ(int(out_f[i]), int(out_h[i]));                         \
        }                                                                      \
    } while (0)

#define FL_SAME_U16_16(expr_host, expr_fallback)                               \
    do {                                                                       \
        fl::u16 out_h[16] = {0}, out_f[16] = {0};                              \
        readLanes((expr_host), out_h, 16);                                     \
        readLanes((expr_fallback), out_f, 16);                                 \
        for (int i = 0; i < 16; ++i) {                                         \
            FL_CHECK_EQ(int(out_f[i]), int(out_h[i]));                         \
        }                                                                      \
    } while (0)

// The narrowing comparison is kept alongside, because `narrow_*` is itself
// one of the 66 operations and saturation is its documented behaviour -- it
// is just no longer the only thing standing behind the u16 arithmetic.
#define FL_SAME_U16_8_PAIR(lo_h, hi_h, lo_f, hi_f)                             \
    do {                                                                       \
        FL_SAME_U16_8((lo_h), (lo_f));                                         \
        FL_SAME_U16_8((hi_h), (hi_f));                                         \
        FL_SAME_U8_16(host::narrow_u16_to_u8((lo_h), (hi_h)),                  \
                      fallback::narrow_u16_to_u8((lo_f), (hi_f)));             \
    } while (0)

#define FL_SAME_U16_16_PAIR(lo_h, hi_h, lo_f, hi_f)                            \
    do {                                                                       \
        FL_SAME_U16_16((lo_h), (lo_f));                                        \
        FL_SAME_U16_16((hi_h), (hi_f));                                        \
        FL_SAME_U8_32(host::narrow_u16x16_to_u8((lo_h), (hi_h)),               \
                      fallback::narrow_u16x16_to_u8((lo_f), (hi_f)));          \
    } while (0)

#endif  // !FL_SIMD_BACKEND_IS_FALLBACK

} // namespace

// ---------------------------------------------------------------------------
// Contract cases: the semantics every backend agrees on, checked against the
// one this host selected. These run everywhere, including where the selected
// backend *is* the fallback.
// ---------------------------------------------------------------------------

FL_TEST_CASE("SIMD contract: avg_u8_16 truncates and avg_round_u8_16 rounds") {
    // Two operations, not one. x86 answered both with `_mm_avg_epu8`, which
    // is `(a+b+1)>>1`, and was the only backend of six to do so: NEON's
    // `vhaddq_u8`, ARM-DSP's `uhadd8`, the Xtensa, RISC-V and fallback loops
    // -- and the non-SSE2 path inside simd_x86.hpp itself -- all truncate.
    fl::u8 a[16], b[16];
    for (int i = 0; i < 16; ++i) { a[i] = 0; b[i] = 0; }
    // 0+1 is the smallest pair the two answers differ on, and 254+255 the
    // largest that still fits: 254 truncating, 255 rounding.
    a[0] = 0;   b[0] = 1;
    a[1] = 254; b[1] = 255;
    a[2] = 10;  b[2] = 10;   // equal inputs: both answers are 10
    a[3] = 255; b[3] = 255;  // both 255, and neither may overflow

    fl::u8 trunc[16] = {0}, round[16] = {0};
    const auto va = host::load_u8_16(a);
    const auto vb = host::load_u8_16(b);
    host::store_u8_16(trunc, host::avg_u8_16(va, vb));
    host::store_u8_16(round, host::avg_round_u8_16(va, vb));

    FL_CHECK_EQ(int(trunc[0]), 0);    FL_CHECK_EQ(int(round[0]), 1);
    FL_CHECK_EQ(int(trunc[1]), 254);  FL_CHECK_EQ(int(round[1]), 255);
    FL_CHECK_EQ(int(trunc[2]), 10);   FL_CHECK_EQ(int(round[2]), 10);
    FL_CHECK_EQ(int(trunc[3]), 255);  FL_CHECK_EQ(int(round[3]), 255);
}

FL_TEST_CASE("SIMD contract: narrow_u16_to_u8 clamps its lanes as unsigned") {
    // The lanes are u16. x86 packed them with `_mm_packus_epi16`, which
    // saturates on the *signed* reading, so anything above 32767 came out 0
    // where every other backend clamps it to 255.
    fl::u8 src[16] = {0};
    const auto zero = host::load_u8_16(src);
    auto lo = host::widen_lo_u8_to_u16(zero);

    // Build the u16 lanes by arithmetic on widened bytes rather than by a
    // load, because the API has no u16x8 load: 255 * 257 == 65535, which is
    // deep in the range where the two readings disagree.
    for (int i = 0; i < 16; ++i) { src[i] = 255; }
    const auto full = host::load_u8_16(src);
    lo = host::widen_lo_u8_to_u16(full);                     // 255 per lane
    const auto big = host::mullo_u16_8(lo, host::set1_u16_8(257));  // 65535

    fl::u8 out[16] = {0};
    host::store_u8_16(out, host::narrow_u16_to_u8(big, big));
    for (int i = 0; i < 16; ++i) {
        FL_CHECK_EQ(int(out[i]), 255);  // not 0, which the signed reading gives
    }

    // And a value that is unambiguous either way still passes through.
    const auto small = host::mullo_u16_8(lo, host::set1_u16_8(1));  // 255
    host::store_u8_16(out, host::narrow_u16_to_u8(small, small));
    for (int i = 0; i < 16; ++i) {
        FL_CHECK_EQ(int(out[i]), 255);
    }
}

FL_TEST_CASE("SIMD contract: narrow_u16x16_to_u8 clamps its lanes as unsigned") {
    // Same property on the 32-byte path, which x86 packs with the AVX2
    // `_mm256_packus_epi16` and had the same signed reading.
    fl::u8 src[32];
    for (int i = 0; i < 32; ++i) { src[i] = 255; }
    const auto full = host::load_u8_32(src);
    const auto lanes = host::widen_lo_u8x32_to_u16(full);           // 255
    const auto big = host::mullo_u16_16(lanes, host::set1_u16_16(257));  // 65535

    fl::u8 out[32] = {0};
    host::store_u8_32(out, host::narrow_u16x16_to_u8(big, big));
    for (int i = 0; i < 32; ++i) {
        FL_CHECK_EQ(int(out[i]), 255);
    }
}

#if !FL_SIMD_BACKEND_IS_FALLBACK
// ---------------------------------------------------------------------------
// Differential cases: all 66 operations against the backend this host chose.
// Skipped where that backend is the fallback, since the comparison would be
// an identity -- see the file comment.
// ---------------------------------------------------------------------------

FL_TEST_CASE("Fallback u8x16: load, store and the byte-wise operations agree") {
    const auto ah = host::load_u8_16(kA8);
    const auto bh = host::load_u8_16(kB8);
    const auto af = fallback::load_u8_16(kA8);
    const auto bf = fallback::load_u8_16(kB8);

    // load/store round trip first: everything below is read through it.
    FL_SAME_U8_16(ah, af);
    FL_SAME_U8_16(bh, bf);

    FL_SAME_U8_16(host::add_sat_u8_16(ah, bh), fallback::add_sat_u8_16(af, bf));
    FL_SAME_U8_16(host::sub_sat_u8_16(ah, bh), fallback::sub_sat_u8_16(af, bf));
    FL_SAME_U8_16(host::avg_u8_16(ah, bh), fallback::avg_u8_16(af, bf));
    FL_SAME_U8_16(host::avg_round_u8_16(ah, bh), fallback::avg_round_u8_16(af, bf));
    FL_SAME_U8_16(host::min_u8_16(ah, bh), fallback::min_u8_16(af, bf));
    FL_SAME_U8_16(host::max_u8_16(ah, bh), fallback::max_u8_16(af, bf));
    FL_SAME_U8_16(host::and_u8_16(ah, bh), fallback::and_u8_16(af, bf));
    FL_SAME_U8_16(host::or_u8_16(ah, bh), fallback::or_u8_16(af, bf));
    FL_SAME_U8_16(host::xor_u8_16(ah, bh), fallback::xor_u8_16(af, bf));
    FL_SAME_U8_16(host::andnot_u8_16(ah, bh), fallback::andnot_u8_16(af, bf));

    // Scale and blend take a scalar; 0 and 255 are the ends that tend to
    // differ by an off-by-one between a shift and a divide.
    const fl::u8 kScales[] = {0, 1, 64, 128, 200, 254, 255};
    for (fl::u8 s : kScales) {
        FL_SAME_U8_16(host::scale_u8_16(ah, s), fallback::scale_u8_16(af, s));
        FL_SAME_U8_16(host::blend_u8_16(ah, bh, s), fallback::blend_u8_16(af, bf, s));
    }
}

FL_TEST_CASE("Fallback u32x4: construction, arithmetic and bit operations agree") {
    const auto ah = host::load_u32_4(kA32);
    const auto bh = host::load_u32_4(kB32);
    const auto ch = host::load_u32_4(kC32);
    const auto af = fallback::load_u32_4(kA32);
    const auto bf = fallback::load_u32_4(kB32);
    const auto cf = fallback::load_u32_4(kC32);

    FL_SAME_U32_4(ah, af);
    FL_SAME_U32_4(host::set1_u32_4(0xDEADBEEFu), fallback::set1_u32_4(0xDEADBEEFu));
    FL_SAME_U32_4(host::set_u32_4(1u, 2u, 3u, 4u), fallback::set_u32_4(1u, 2u, 3u, 4u));

    FL_SAME_U32_4(host::add_i32_4(ah, bh), fallback::add_i32_4(af, bf));
    FL_SAME_U32_4(host::sub_i32_4(ah, bh), fallback::sub_i32_4(af, bf));
    FL_SAME_U32_4(host::and_u32_4(ah, bh), fallback::and_u32_4(af, bf));
    FL_SAME_U32_4(host::or_u32_4(ah, bh), fallback::or_u32_4(af, bf));
    FL_SAME_U32_4(host::xor_u32_4(ah, bh), fallback::xor_u32_4(af, bf));
    FL_SAME_U32_4(host::min_i32_4(ah, bh), fallback::min_i32_4(af, bf));
    FL_SAME_U32_4(host::max_i32_4(ah, bh), fallback::max_i32_4(af, bf));

    // The multiplies are the operations #4216 is actually about.
    FL_SAME_U32_4(host::mulhi_i32_4(ah, bh), fallback::mulhi_i32_4(af, bf));
    FL_SAME_U32_4(host::mulhi_u32_4(ah, bh), fallback::mulhi_u32_4(af, bf));
    FL_SAME_U32_4(host::mulhi_su32_4(ah, bh), fallback::mulhi_su32_4(af, bf));
    FL_SAME_U32_4(host::mulhi32_i32_4(ah, bh), fallback::mulhi32_i32_4(af, bf));
    FL_SAME_U32_4(host::mulhi_i32_4(ch, bh), fallback::mulhi_i32_4(cf, bf));
    FL_SAME_U32_4(host::mulhi_u32_4(ch, bh), fallback::mulhi_u32_4(cf, bf));
    FL_SAME_U32_4(host::mulhi_su32_4(ch, bh), fallback::mulhi_su32_4(cf, bf));
    FL_SAME_U32_4(host::mulhi32_i32_4(ch, bh), fallback::mulhi32_i32_4(cf, bf));

    FL_SAME_U32_4(host::unpacklo_u32_4(ah, bh), fallback::unpacklo_u32_4(af, bf));
    FL_SAME_U32_4(host::unpackhi_u32_4(ah, bh), fallback::unpackhi_u32_4(af, bf));
    FL_SAME_U32_4(host::unpacklo_u64_as_u32_4(ah, bh),
                  fallback::unpacklo_u64_as_u32_4(af, bf));
    FL_SAME_U32_4(host::unpackhi_u64_as_u32_4(ah, bh),
                  fallback::unpackhi_u64_as_u32_4(af, bf));

    for (int lane = 0; lane < 4; ++lane) {
        FL_CHECK_EQ(fallback::extract_u32_4(af, lane), host::extract_u32_4(ah, lane));
        FL_CHECK_EQ(fallback::extract_u32_4(cf, lane), host::extract_u32_4(ch, lane));
    }
}

FL_TEST_CASE("Fallback u32x4: the shifts agree, including the arithmetic one") {
    const auto ah = host::load_u32_4(kA32);
    const auto af = fallback::load_u32_4(kA32);
    const auto ch = host::load_u32_4(kC32);
    const auto cf = fallback::load_u32_4(kC32);

    // 0 and 31 are the ends; `sra_i32_4` is the one that has to sign-extend,
    // and kA32/kC32 both carry lanes with the top bit set.
    for (int shift = 0; shift <= 31; ++shift) {
        FL_SAME_U32_4(host::srl_u32_4(ah, shift), fallback::srl_u32_4(af, shift));
        FL_SAME_U32_4(host::sll_u32_4(ah, shift), fallback::sll_u32_4(af, shift));
        FL_SAME_U32_4(host::sra_i32_4(ah, shift), fallback::sra_i32_4(af, shift));
        FL_SAME_U32_4(host::sra_i32_4(ch, shift), fallback::sra_i32_4(cf, shift));
    }
}

FL_TEST_CASE("Fallback f32x4: the float operations agree bit for bit") {
    const auto ah = host::load_f32_4(kAf);
    const auto bh = host::load_f32_4(kBf);
    const auto af = fallback::load_f32_4(kAf);
    const auto bf = fallback::load_f32_4(kBf);

    FL_SAME_F32_4(ah, af);
    FL_SAME_F32_4(host::set1_f32_4(3.25f), fallback::set1_f32_4(3.25f));
    FL_SAME_F32_4(host::add_f32_4(ah, bh), fallback::add_f32_4(af, bf));
    FL_SAME_F32_4(host::sub_f32_4(ah, bh), fallback::sub_f32_4(af, bf));
    FL_SAME_F32_4(host::mul_f32_4(ah, bh), fallback::mul_f32_4(af, bf));
    FL_SAME_F32_4(host::div_f32_4(ah, bh), fallback::div_f32_4(af, bf));
    FL_SAME_F32_4(host::min_f32_4(ah, bh), fallback::min_f32_4(af, bf));
    FL_SAME_F32_4(host::max_f32_4(ah, bh), fallback::max_f32_4(af, bf));

    // sqrt of a negative is not compared: the two produce NaNs whose payloads
    // need not match, and NaN != NaN would fail the check for the wrong
    // reason. Non-negative inputs only, which is what the callers pass.
    const float kNonNeg[4] = {0.0f, 1.0f, 2.0f, 1e6f};
    FL_SAME_F32_4(host::sqrt_f32_4(host::load_f32_4(kNonNeg)),
                  fallback::sqrt_f32_4(fallback::load_f32_4(kNonNeg)));
}

FL_TEST_CASE("Fallback u32x4: the aligned load and store agree with the unaligned pair") {
    FL_ALIGNAS(16) fl::u32 aligned_in[4] = {kC32[0], kC32[1], kC32[2], kC32[3]};
    FL_ALIGNAS(16) fl::u32 out_h[4] = {0};
    FL_ALIGNAS(16) fl::u32 out_f[4] = {0};

    host::store_u32_4_aligned(out_h, host::load_u32_4_aligned(aligned_in));
    fallback::store_u32_4_aligned(out_f, fallback::load_u32_4_aligned(aligned_in));
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_EQ(out_f[i], out_h[i]);
        // And the aligned pair is the same value as the unaligned pair, which
        // is the only thing that makes the two interchangeable at a call site.
        FL_CHECK_EQ(out_f[i], aligned_in[i]);
    }
}

FL_TEST_CASE("Fallback u16x8: widen, narrow and the halfword operations agree") {
    const auto ah = host::load_u8_16(kA8);
    const auto bh = host::load_u8_16(kB8);
    const auto af = fallback::load_u8_16(kA8);
    const auto bf = fallback::load_u8_16(kB8);

    // Widening then narrowing must be the identity on both, and must agree.
    FL_SAME_U16_8_PAIR(host::widen_lo_u8_to_u16(ah), host::widen_hi_u8_to_u16(ah),
                       fallback::widen_lo_u8_to_u16(af),
                       fallback::widen_hi_u8_to_u16(af));

    const auto la_h = host::widen_lo_u8_to_u16(ah);
    const auto ha_h = host::widen_hi_u8_to_u16(ah);
    const auto lb_h = host::widen_lo_u8_to_u16(bh);
    const auto hb_h = host::widen_hi_u8_to_u16(bh);
    const auto la_f = fallback::widen_lo_u8_to_u16(af);
    const auto ha_f = fallback::widen_hi_u8_to_u16(af);
    const auto lb_f = fallback::widen_lo_u8_to_u16(bf);
    const auto hb_f = fallback::widen_hi_u8_to_u16(bf);

    FL_SAME_U16_8_PAIR(host::add_u16_8(la_h, lb_h), host::add_u16_8(ha_h, hb_h),
                       fallback::add_u16_8(la_f, lb_f),
                       fallback::add_u16_8(ha_f, hb_f));
    FL_SAME_U16_8_PAIR(host::mullo_u16_8(la_h, lb_h), host::mullo_u16_8(ha_h, hb_h),
                       fallback::mullo_u16_8(la_f, lb_f),
                       fallback::mullo_u16_8(ha_f, hb_f));

    const auto s1_h = host::set1_u16_8(0xBEEFu);
    const auto s1_f = fallback::set1_u16_8(0xBEEFu);
    for (int shift = 0; shift <= 15; ++shift) {
        FL_SAME_U16_8_PAIR(host::srli_u16_8(s1_h, shift),
                           host::srli_u16_8(la_h, shift),
                           fallback::srli_u16_8(s1_f, shift),
                           fallback::srli_u16_8(la_f, shift));
    }
}

FL_TEST_CASE("Fallback u8x32 and u16x16: the wide operations agree") {
    const auto ah = host::load_u8_32(kA8);
    const auto bh = host::load_u8_32(kB8);
    const auto af = fallback::load_u8_32(kA8);
    const auto bf = fallback::load_u8_32(kB8);

    FL_SAME_U8_32(ah, af);
    FL_SAME_U8_32(host::avg_round_u8_32(ah, bh), fallback::avg_round_u8_32(af, bf));

    const auto la_h = host::widen_lo_u8x32_to_u16(ah);
    const auto ha_h = host::widen_hi_u8x32_to_u16(ah);
    const auto lb_h = host::widen_lo_u8x32_to_u16(bh);
    const auto hb_h = host::widen_hi_u8x32_to_u16(bh);
    const auto la_f = fallback::widen_lo_u8x32_to_u16(af);
    const auto ha_f = fallback::widen_hi_u8x32_to_u16(af);
    const auto lb_f = fallback::widen_lo_u8x32_to_u16(bf);
    const auto hb_f = fallback::widen_hi_u8x32_to_u16(bf);

    FL_SAME_U16_16_PAIR(la_h, ha_h, la_f, ha_f);
    FL_SAME_U16_16_PAIR(host::add_u16_16(la_h, lb_h), host::add_u16_16(ha_h, hb_h),
                        fallback::add_u16_16(la_f, lb_f),
                        fallback::add_u16_16(ha_f, hb_f));
    FL_SAME_U16_16_PAIR(host::mullo_u16_16(la_h, lb_h),
                        host::mullo_u16_16(ha_h, hb_h),
                        fallback::mullo_u16_16(la_f, lb_f),
                        fallback::mullo_u16_16(ha_f, hb_f));

    const auto s1_h = host::set1_u16_16(0xBEEFu);
    const auto s1_f = fallback::set1_u16_16(0xBEEFu);
    for (int shift = 0; shift <= 15; ++shift) {
        FL_SAME_U16_16_PAIR(host::srli_u16_16(s1_h, shift),
                            host::srli_u16_16(la_h, shift),
                            fallback::srli_u16_16(s1_f, shift),
                            fallback::srli_u16_16(la_f, shift));
    }
}

#endif  // !FL_SIMD_BACKEND_IS_FALLBACK

} // FL_TEST_FILE
