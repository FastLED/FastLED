/// @file simd_fallback.cpp
/// Differential coverage for the scalar SIMD fallback.
///
/// `src/platforms/shared/simd_noop.hpp` is the SIMD backend for AVR, ESP8266,
/// Cortex-M0/M0+/M3, WASM, RP2350 and every part `platforms/simd.h` does not
/// otherwise match. On an x86 host that dispatch selects `simd_x86.hpp`, so
/// the fallback was reached by nothing: an `#error` placed at the top of it
/// does not fire anywhere in the test build. Its 66 operations shipped to
/// those targets with no host test behind them.
///
/// This file includes the fallback under a second namespace and checks every
/// one of those operations against the backend the host actually selected.
/// A differential oracle rather than hand-written expectations, because the
/// point is that the fallback and the accelerated path agree -- that is the
/// promise the dispatch makes, and hand-copied constants would only restate
/// whichever implementation they were copied from.
///
/// Issue #4216 wants the fallback's lane storage rewritten from `u32[4]` to
/// named members, and gave "verifiable on two of many targets" as the reason
/// not to. That rewrite touches all 80 `data[...]` accesses; these cases are
/// what turn it into a refactor with an oracle.

#define FL_SIMD_FALLBACK_NAMESPACE platforms_fallback
#include "platforms/shared/simd_noop.hpp"

#include "fl/math/simd.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

namespace host = fl::simd::platforms;
namespace fallback = fl::simd::platforms_fallback;

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

// u16x8 and u16x16 have no store of their own in the API; both backends reach
// them only through narrow_*, so they are compared by narrowing back down.
#define FL_SAME_U16_8_PAIR(lo_h, hi_h, lo_f, hi_f)                             \
    FL_SAME_U8_16(host::narrow_u16_to_u8((lo_h), (hi_h)),                      \
                  fallback::narrow_u16_to_u8((lo_f), (hi_f)))

#define FL_SAME_U16_16_PAIR(lo_h, hi_h, lo_f, hi_f)                            \
    FL_SAME_U8_32(host::narrow_u16x16_to_u8((lo_h), (hi_h)),                   \
                  fallback::narrow_u16x16_to_u8((lo_f), (hi_f)))

} // namespace

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

} // FL_TEST_FILE
