// Miscellaneous fixed-point tests extracted from fixed_point.cpp (sub-issue of #3127).
// Covers:
//   - Fixed-point scalar type alignment
//   - Constexpr scalar multiply/divide compile-time tests (s0x32, u0x32)
//   - Integer constructor tests (PR #2197 — int width portability)
//   - pow boundary regression tests (#2969)

#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for fixed_point_misc tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// =============================================================================
// Fixed-Point Scalar Type Alignment Tests
// =============================================================================

FL_TEST_CASE("fixed-point scalar type alignment") {
    FL_SUBCASE("s0x32 alignment") {
        // Scalar fixed-point types don't need special alignment
        FL_CHECK(alignof(s0x32) <= 8);  // Should be natural alignment (4 bytes for i32)
        s0x32 val = s0x32::from_raw(1073741824);  // 0.5 in Q31 format
        (void)val;  // Silence unused warning
    }

    FL_SUBCASE("s16x16 alignment") {
        FL_CHECK(alignof(s16x16) <= 8);
        s16x16 val = s16x16::from_raw(32768);  // 0.5 in Q16.16 format
        (void)val;
    }
}

// ============================================================================
// Constexpr Scalar Multiply/Divide Compile-Time Tests
// ============================================================================
// Validates that scalar operator* and operator/ are usable in constexpr
// contexts (C++11 ternary-based clamping). If these fail to compile, the
// constexpr annotations are broken.

// s0x32 constexpr scalar multiply
static constexpr s0x32 kS0x32Half = s0x32(0.5f);
static constexpr s0x32 kS0x32ScalarMul = kS0x32Half * i32(2);
static constexpr s0x32 kS0x32ScalarMulComm = i32(2) * kS0x32Half;
static constexpr s0x32 kS0x32ScalarDiv = kS0x32Half / i32(2);

// u0x32 constexpr scalar multiply
static constexpr u0x32 kU0x32Quarter = u0x32(0.25f);
static constexpr u0x32 kU0x32ScalarMul = kU0x32Quarter * u32(2);
static constexpr u0x32 kU0x32ScalarMulComm = u32(2) * kU0x32Quarter;
static constexpr u0x32 kU0x32ScalarDiv = kU0x32Quarter / u32(2);

FL_TEST_CASE("constexpr scalar ops - s0x32 and u0x32") {
    // s0x32: 0.5 * 2 should saturate or equal ~1.0
    FL_CHECK(kS0x32ScalarMul.raw() != 0);
    FL_CHECK(kS0x32ScalarMulComm.raw() == kS0x32ScalarMul.raw());
    FL_CHECK(kS0x32ScalarDiv.raw() != 0);

    // u0x32: 0.25 * 2 = 0.5
    FL_CHECK(kU0x32ScalarMul.raw() != 0);
    FL_CHECK(kU0x32ScalarMulComm.raw() == kU0x32ScalarMul.raw());
    FL_CHECK(kU0x32ScalarDiv.raw() != 0);
}

// ============================================================================
// Integer Constructor Tests (PR #2197 - int width portability)
// ============================================================================
// Validates template integer constructors work with various integer types.
// These constructors accept any integer type via SFINAE, avoiding the
// int-width portability issue (int is 16 bits on AVR, 32 bits on ARM/x86).

FL_TEST_CASE("Integer constructor - signed 32-bit storage types") {
    // s16x16: SCALE = 65536
    constexpr s16x16 a(5);
    FL_CHECK_EQ(a.to_int(), 5);
    FL_CHECK_EQ(a.raw(), 5 * 65536);

    constexpr s16x16 b(-3);
    FL_CHECK_EQ(b.to_int(), -3);

    constexpr s16x16 zero_s16(0);
    FL_CHECK_EQ(zero_s16.raw(), 0);

    // s24x8: SCALE = 256
    constexpr s24x8 c(42);
    FL_CHECK_EQ(c.to_int(), 42);
    FL_CHECK_EQ(c.raw(), 42 * 256);

    constexpr s24x8 d(-100);
    FL_CHECK_EQ(d.to_int(), -100);

    // s8x24: SCALE = 16777216
    constexpr s8x24 e(3);
    FL_CHECK_EQ(e.to_int(), 3);
    FL_CHECK_EQ(e.raw(), 3 * 16777216);
}

FL_TEST_CASE("Integer constructor - signed 16-bit storage types") {
    // s12x4: SCALE = 16
    constexpr s12x4 a(10);
    FL_CHECK_EQ(a.to_int(), 10);
    FL_CHECK_EQ(a.raw(), 10 * 16);

    constexpr s12x4 b(-5);
    FL_CHECK_EQ(b.to_int(), -5);

    // s8x8: SCALE = 256
    constexpr s8x8 c(7);
    FL_CHECK_EQ(c.to_int(), 7);
    FL_CHECK_EQ(c.raw(), 7 * 256);

    // s4x12: SCALE = 4096
    constexpr s4x12 d(2);
    FL_CHECK_EQ(d.to_int(), 2);
    FL_CHECK_EQ(d.raw(), 2 * 4096);
}

FL_TEST_CASE("Integer constructor - unsigned 32-bit storage types") {
    // u16x16: SCALE = 65536
    constexpr u16x16 a(5u);
    FL_CHECK_EQ(a.to_int(), 5u);

    // u24x8: SCALE = 256
    constexpr u24x8 b(42u);
    FL_CHECK_EQ(b.to_int(), 42u);
    FL_CHECK_EQ(b.raw(), 42u * 256u);

    // u8x24: SCALE = 16777216
    constexpr u8x24 c(3u);
    FL_CHECK_EQ(c.to_int(), 3u);
}

FL_TEST_CASE("Integer constructor - unsigned 16-bit storage types") {
    // u12x4: SCALE = 16
    constexpr u12x4 a(10u);
    FL_CHECK_EQ(a.to_int(), 10u);
    FL_CHECK_EQ(a.raw(), static_cast<u16>(10u * 16u));

    // u8x8: SCALE = 256
    constexpr u8x8 b(7u);
    FL_CHECK_EQ(b.to_int(), 7u);

    // u4x12: SCALE = 4096
    constexpr u4x12 c(2u);
    FL_CHECK_EQ(c.to_int(), 2u);
}

FL_TEST_CASE("Integer constructor - different integer types") {
    // Verify the template accepts various integer types (not just int)
    constexpr short s = 5;
    constexpr s16x16 from_short(s);
    FL_CHECK_EQ(from_short.to_int(), 5);

    constexpr long l = 10;
    constexpr s24x8 from_long(l);
    FL_CHECK_EQ(from_long.to_int(), 10);

    constexpr i32 i32_val = 3;
    constexpr s16x16 from_i32(i32_val);
    FL_CHECK_EQ(from_i32.to_int(), 3);

    constexpr i16 i16_val = 7;
    constexpr s8x8 from_i16(i16_val);
    FL_CHECK_EQ(from_i16.to_int(), 7);

    // Unsigned integer types
    constexpr unsigned short us = 5;
    constexpr u16x16 from_ushort(us);
    FL_CHECK_EQ(from_ushort.to_int(), 5u);

    constexpr u32 u32_val = 42;
    constexpr u24x8 from_u32(u32_val);
    FL_CHECK_EQ(from_u32.to_int(), 42u);
}

FL_TEST_CASE("Integer constructor - equivalence with float constructor") {
    // Integer constructor should give same result as float for whole numbers
    constexpr s16x16 from_int(5);
    constexpr s16x16 from_float(5.0f);
    FL_CHECK_EQ(from_int.raw(), from_float.raw());

    constexpr s24x8 from_int2(42);
    constexpr s24x8 from_float2(42.0f);
    FL_CHECK_EQ(from_int2.raw(), from_float2.raw());

    constexpr s8x8 from_int3(7);
    constexpr s8x8 from_float3(7.0f);
    FL_CHECK_EQ(from_int3.raw(), from_float3.raw());

    constexpr u16x16 from_uint(5u);
    constexpr u16x16 from_ufloat(5.0f);
    FL_CHECK_EQ(from_uint.raw(), from_ufloat.raw());
}

FL_TEST_CASE("Integer constructor - boundary values at INT_BITS limits") {
    // These constexpr constructions must compile. Values are at the exact
    // boundary of what INT_BITS can represent. If any of these fail to
    // compile, the range check is too tight.

    // s8x24: INT_BITS=8, range [-128, 127]
    constexpr s8x24 s8x24_max(127);
    constexpr s8x24 s8x24_min(-128);
    FL_CHECK_EQ(s8x24_max.to_int(), 127);
    FL_CHECK_EQ(s8x24_min.to_int(), -128);

    // s8x8: INT_BITS=8, range [-128, 127]
    constexpr s8x8 s8x8_max(127);
    constexpr s8x8 s8x8_min(-128);
    FL_CHECK_EQ(s8x8_max.to_int(), 127);
    FL_CHECK_EQ(s8x8_min.to_int(), -128);

    // s4x12: INT_BITS=4, range [-8, 7]
    constexpr s4x12 s4x12_max(7);
    constexpr s4x12 s4x12_min(-8);
    FL_CHECK_EQ(s4x12_max.to_int(), 7);
    FL_CHECK_EQ(s4x12_min.to_int(), -8);

    // s12x4: INT_BITS=12, range [-2048, 2047]
    constexpr s12x4 s12x4_max(2047);
    constexpr s12x4 s12x4_min(-2048);
    FL_CHECK_EQ(s12x4_max.to_int(), 2047);
    FL_CHECK_EQ(s12x4_min.to_int(), -2048);

    // s16x16: INT_BITS=16, range [-32768, 32767]
    constexpr s16x16 s16x16_max(32767);
    constexpr s16x16 s16x16_min(-32768);
    FL_CHECK_EQ(s16x16_max.to_int(), 32767);
    FL_CHECK_EQ(s16x16_min.to_int(), -32768);

    // s24x8: INT_BITS=24, range [-8388608, 8388607]
    constexpr s24x8 s24x8_max(8388607);
    constexpr s24x8 s24x8_min(-8388608);
    FL_CHECK_EQ(s24x8_max.to_int(), 8388607);
    FL_CHECK_EQ(s24x8_min.to_int(), -8388608);

    // u8x8: INT_BITS=8, range [0, 255]
    constexpr u8x8 u8x8_max(255u);
    FL_CHECK_EQ(u8x8_max.to_int(), 255u);

    // u8x24: INT_BITS=8, range [0, 255]
    constexpr u8x24 u8x24_max(255u);
    FL_CHECK_EQ(u8x24_max.to_int(), 255u);

    // u4x12: INT_BITS=4, range [0, 15]
    constexpr u4x12 u4x12_max(15u);
    FL_CHECK_EQ(u4x12_max.to_int(), 15u);

    // u12x4: INT_BITS=12, range [0, 4095]
    constexpr u12x4 u12x4_max(4095u);
    FL_CHECK_EQ(u12x4_max.to_int(), 4095u);

    // u16x16: INT_BITS=16, range [0, 65535]
    constexpr u16x16 u16x16_max(65535u);
    FL_CHECK_EQ(u16x16_max.to_int(), 65535u);

    // u24x8: INT_BITS=24, range [0, 16777215]
    constexpr u24x8 u24x8_max(16777215u);
    FL_CHECK_EQ(u24x8_max.to_int(), 16777215u);

    // Signed integers passed to unsigned types: positive values work
    constexpr int five = 5;
    constexpr u8x8 u8x8_from_signed_int(five);
    FL_CHECK_EQ(u8x8_from_signed_int.to_int(), 5u);

    constexpr int two_hundred = 200;
    constexpr u24x8 u24x8_from_signed_int(two_hundred);
    FL_CHECK_EQ(u24x8_from_signed_int.to_int(), 200u);

    constexpr short three = 3;
    constexpr u16x16 u16x16_from_short(three);
    FL_CHECK_EQ(u16x16_from_short.to_int(), 3u);

    constexpr int zero = 0;
    constexpr u4x12 u4x12_from_zero(zero);
    FL_CHECK_EQ(u4x12_from_zero.to_int(), 0u);

    // Out-of-range values (uncomment any line to verify compile error):
    // constexpr s8x24 too_big(128);      // error: 128 > 127
    // constexpr u8x8 neg(-1);            // error: negative for unsigned type
    // constexpr s8x24 too_small(-129);   // error: -129 < -128
    // constexpr s4x12 too_big_4(8);      // error: 8 > 7
    // constexpr u8x8 too_big_u(256u);    // error: 256 > 255
    // constexpr s8x24 way_too_big(64000); // error: 64000 > 127
}

FL_TEST_CASE("Integer constructor - fixed_point<> wrapper delegation") {
    // sfixed_integer / ufixed_integer are aliases for fixed_point<I,F,Sign>
    // which inherits from the concrete type. Verify integer constructors
    // forward correctly through the wrapper.
    using SFP16 = fl::sfixed_integer<16, 16>;
    using SFP24 = fl::sfixed_integer<24, 8>;
    using UFP16 = fl::ufixed_integer<16, 16>;
    using UFP8  = fl::ufixed_integer<8, 8>;

    constexpr SFP16 a(5);
    FL_CHECK_EQ(a.to_int(), 5);
    FL_CHECK_EQ(a.raw(), 5 * 65536);

    constexpr SFP16 b(-3);
    FL_CHECK_EQ(b.to_int(), -3);

    constexpr SFP24 c(42);
    FL_CHECK_EQ(c.to_int(), 42);

    constexpr UFP16 d(100u);
    FL_CHECK_EQ(d.to_int(), 100);

    constexpr UFP8 e(7u);
    FL_CHECK_EQ(e.to_int(), 7u);

    // Signed int to unsigned wrapper
    constexpr int ten = 10;
    constexpr UFP16 f(ten);
    FL_CHECK_EQ(f.to_int(), 10u);

    // Equivalence: wrapper integer constructor == wrapper float constructor
    constexpr SFP16 from_int(5);
    constexpr SFP16 from_float(5.0f);
    FL_CHECK_EQ(from_int.raw(), from_float.raw());

    // Boundary: sfixed_integer<8,24> max = 127
    using SFP8x24 = fl::sfixed_integer<8, 24>;
    constexpr SFP8x24 max_val(127);
    FL_CHECK_EQ(max_val.to_int(), 127);

    // constexpr SFP8x24 overflow(128);  // error: exceeds INT_BITS range
}

// ============================================================================
// pow boundary regression tests (#2969)
// ============================================================================
//
// Background: `pow(base, exp)` is implemented as `exp2_fp(exp * log2_fp(base))`.
// `log2_fp` uses a 4-term minimax polynomial for `log2(1+t)` on `t in [0, 1)`.
// At the upper endpoint t -> 1 the polynomial evaluates to approx 0.999557
// instead of 1.0 -- a 0.000443 error that, after exp2 + scale-to-u16, drops
// the output by ~50-100 LSB below the expected 65535. The original failure
// mode was `Gamma8Impl::mLut[255]` returning 65478 instead of 65535 for
// gamma=3.2.
//
// Fix: snap `base.mValue` values within 2 ULPs of exactly-1.0 to one before
// calling the polynomial. The tests below pin that contract.

// pow(1.0, exp) must equal exactly 1.0 for any exp -- already true pre-fix
// thanks to the `base == one` short-circuit, but lock it in.
template <typename FP>
static void test_pow_exactly_one_impl() {
    const FP one(1.0f);
    const FP two(2.0f);
    const FP gamma_2_8(2.8f);
    const FP gamma_3_2(3.2f);
    FL_CHECK_EQ(FP::pow(one, two).raw(), one.raw());
    FL_CHECK_EQ(FP::pow(one, gamma_2_8).raw(), one.raw());
    FL_CHECK_EQ(FP::pow(one, gamma_3_2).raw(), one.raw());
}

// pow(base, exp) where base.raw() == SCALE - 1 (one ULP below 1.0). Without
// the snap, the polynomial gives a value ~50-100 LSB below expected. With
// the snap, this must collapse to exactly one.
template <typename FP>
static void test_pow_one_ulp_below_one_impl() {
    const FP just_below = FP::from_raw(static_cast<raw_t<FP>>(FP::SCALE - 1));
    const FP one(1.0f);
    const FP gamma_2_8(2.8f);
    const FP gamma_3_2(3.2f);
    FL_CHECK_EQ(FP::pow(just_below, gamma_2_8).raw(), one.raw());
    FL_CHECK_EQ(FP::pow(just_below, gamma_3_2).raw(), one.raw());
}

// Same as above, two LSBs below 1.0 -- also inside the snap window.
template <typename FP>
static void test_pow_two_ulps_below_one_impl() {
    const FP just_below = FP::from_raw(static_cast<raw_t<FP>>(FP::SCALE - 2));
    const FP one(1.0f);
    const FP gamma_2_8(2.8f);
    const FP gamma_3_2(3.2f);
    FL_CHECK_EQ(FP::pow(just_below, gamma_2_8).raw(), one.raw());
    FL_CHECK_EQ(FP::pow(just_below, gamma_3_2).raw(), one.raw());
}

// For signed types: pow(0.5, 2.0) approx 0.25 -- well below 1.0, snap must NOT
// fire. Excludes u-types since their log2_fp doesn't support base < 1.0.
template <typename FP>
static void test_pow_well_below_one_signed_impl() {
    const FP half(0.5f);
    const FP two(2.0f);
    const FP one(1.0f);
    FL_CHECK(FP::pow(half, two).raw() < one.raw());
}

// For unsigned types: pow(2.0, 0.5) approx 1.414 -- well above 1.0, snap must NOT
// fire (it only triggers on base near 1.0, not on result).
template <typename FP>
static void test_pow_above_one_unsigned_impl() {
    const FP two(2.0f);
    const FP half(0.5f);
    const FP one(1.0f);
    FL_CHECK(FP::pow(two, half).raw() > one.raw());
}

FL_TEST_CASE("pow boundary - exactly 1.0 (#2969)") {
    FL_SUBCASE("s16x16") { test_pow_exactly_one_impl<s16x16>(); }
    FL_SUBCASE("s8x24")  { test_pow_exactly_one_impl<s8x24>();  }
    FL_SUBCASE("u16x16") { test_pow_exactly_one_impl<u16x16>(); }
    FL_SUBCASE("u8x24")  { test_pow_exactly_one_impl<u8x24>();  }
}

FL_TEST_CASE("pow boundary - one ULP below 1.0 snaps to 1.0 (#2969)") {
    FL_SUBCASE("s16x16") { test_pow_one_ulp_below_one_impl<s16x16>(); }
    FL_SUBCASE("s8x24")  { test_pow_one_ulp_below_one_impl<s8x24>();  }
    FL_SUBCASE("u16x16") { test_pow_one_ulp_below_one_impl<u16x16>(); }
    FL_SUBCASE("u8x24")  { test_pow_one_ulp_below_one_impl<u8x24>();  }
}

FL_TEST_CASE("pow boundary - two ULPs below 1.0 snaps to 1.0 (#2969)") {
    FL_SUBCASE("s16x16") { test_pow_two_ulps_below_one_impl<s16x16>(); }
    FL_SUBCASE("s8x24")  { test_pow_two_ulps_below_one_impl<s8x24>();  }
    FL_SUBCASE("u16x16") { test_pow_two_ulps_below_one_impl<u16x16>(); }
    FL_SUBCASE("u8x24")  { test_pow_two_ulps_below_one_impl<u8x24>();  }
}

FL_TEST_CASE("pow boundary - far from 1.0 not snapped (#2969)") {
    FL_SUBCASE("s16x16 pow(0.5, 2)") { test_pow_well_below_one_signed_impl<s16x16>(); }
    FL_SUBCASE("s8x24  pow(0.5, 2)") { test_pow_well_below_one_signed_impl<s8x24>();  }
    FL_SUBCASE("u16x16 pow(2, 0.5)") { test_pow_above_one_unsigned_impl<u16x16>(); }
    FL_SUBCASE("u8x24  pow(2, 0.5)") { test_pow_above_one_unsigned_impl<u8x24>();  }
}

} // anonymous namespace

} // FL_TEST_FILE
