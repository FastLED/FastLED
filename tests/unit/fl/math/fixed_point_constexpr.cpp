// Comprehensive constexpr validation for all fixed-point operations.
// Extracted from fixed_point.cpp (sub-issue of #3127).
//
// These are compile-time checks: if any operation is not constexpr, the
// static_assert will fail at compile time.
// C++11 constexpr rules: single return statement, no variable declarations.

#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for fixed_point_constexpr tests

// Compile-time check that T::raw() is constexpr.
template <typename T>
constexpr bool constexpr_raw_check(float f) {
    return T(f).raw(), true;
}

// Helper: Verify from_raw is constexpr
template <typename T>
constexpr bool constexpr_from_raw_check() {
    return T::from_raw(42).raw() == 42;
}

// Helper: Verify addition is constexpr
template <typename T>
constexpr bool constexpr_add_check(float a, float b) {
    return (T(a) + T(b)).raw() == (T(a) + T(b)).raw(); // just verify it compiles
}

// Helper: Verify subtraction is constexpr
template <typename T>
constexpr bool constexpr_sub_check(float a, float b) {
    return (T(a) - T(b)).raw() == (T(a) - T(b)).raw();
}

// Helper: Verify unary minus is constexpr
template <typename T>
constexpr bool constexpr_neg_check(float a) {
    return (-T(a)).raw() == (-T(a)).raw();
}

// Helper: Verify multiply is constexpr
template <typename T>
constexpr bool constexpr_mul_check(float a, float b) {
    return (T(a) * T(b)).raw() == (T(a) * T(b)).raw();
}

// Helper: Verify divide is constexpr
template <typename T>
constexpr bool constexpr_div_check(float a, float b) {
    return (T(a) / T(b)).raw() == (T(a) / T(b)).raw();
}

// Helper: Verify right shift is constexpr
template <typename T>
constexpr bool constexpr_rshift_check(float a) {
    return (T(a) >> 1).raw() == (T(a) >> 1).raw();
}

// Helper: Verify comparison operators are constexpr
template <typename T>
constexpr bool constexpr_lt_check(float a, float b) {
    return T(a) < T(b);
}

template <typename T>
constexpr bool constexpr_eq_check(float a) {
    return T(a) == T(a);
}

// Helper: Verify to_int is constexpr
template <typename T>
constexpr auto constexpr_to_int_check(float a) -> decltype(T(a).to_int()) {
    return T(a).to_int();
}

// Helper: Verify to_float is constexpr
template <typename T>
constexpr float constexpr_to_float_check(float a) {
    return T(a).to_float();
}

// Helper: Verify mod is constexpr
template <typename T>
constexpr bool constexpr_mod_check(float a, float b) {
    return T::mod(T(a), T(b)).raw() == T::mod(T(a), T(b)).raw();
}

// Helper: Verify floor is constexpr
template <typename T>
constexpr bool constexpr_floor_check(float a) {
    return T::floor(T(a)).raw() == T::floor(T(a)).raw();
}

// Helper: Verify ceil is constexpr
template <typename T>
constexpr bool constexpr_ceil_check(float a) {
    return T::ceil(T(a)).raw() == T::ceil(T(a)).raw();
}

// Helper: Verify fract is constexpr
template <typename T>
constexpr bool constexpr_fract_check(float a) {
    return T::fract(T(a)).raw() == T::fract(T(a)).raw();
}

// Helper: Verify abs is constexpr
template <typename T>
constexpr bool constexpr_abs_check(float a) {
    return T::abs(T(a)).raw() == T::abs(T(a)).raw();
}

// Helper: Verify sign is constexpr
template <typename T>
constexpr int constexpr_sign_check(float a) {
    return T::sign(T(a));
}

// Helper: Verify lerp is constexpr
template <typename T>
constexpr bool constexpr_lerp_check(float a, float b, float t) {
    return T::lerp(T(a), T(b), T(t)).raw() == T::lerp(T(a), T(b), T(t)).raw();
}

// Helper: Verify clamp is constexpr
template <typename T>
constexpr bool constexpr_clamp_check(float x, float lo, float hi) {
    return T::clamp(T(x), T(lo), T(hi)).raw() == T::clamp(T(x), T(lo), T(hi)).raw();
}

// Helper: Verify step is constexpr
template <typename T>
constexpr bool constexpr_step_check(float edge, float x) {
    return T::step(T(edge), T(x)).raw() == T::step(T(edge), T(x)).raw();
}

// Helper: Verify sqrt is constexpr
template <typename T>
constexpr bool constexpr_sqrt_check(float a) {
    return T::sqrt(T(a)).raw() == T::sqrt(T(a)).raw();
}

// Helper: Verify rsqrt is constexpr
template <typename T>
constexpr bool constexpr_rsqrt_check(float a) {
    return T::rsqrt(T(a)).raw() == T::rsqrt(T(a)).raw();
}

// Verify that raw() is constexpr for all fixed-point types.
// This is a compile-time check: if raw() is not constexpr, static_assert fails.
FL_TEST_CASE("constexpr raw() - all fixed-point types") {
    // Signed types
    FL_STATIC_ASSERT(constexpr_raw_check<s0x32>(0.5f), "s0x32::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s4x12>(1.0f), "s4x12::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s8x8>(1.0f), "s8x8::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s8x24>(1.0f), "s8x24::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s12x4>(1.0f), "s12x4::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s16x16>(1.0f), "s16x16::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<s24x8>(1.0f), "s24x8::raw() must be constexpr");
    // Unsigned types
    FL_STATIC_ASSERT(constexpr_raw_check<u0x32>(0.5f), "u0x32::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u4x12>(1.0f), "u4x12::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u8x8>(1.0f), "u8x8::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u8x24>(1.0f), "u8x24::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u12x4>(1.0f), "u12x4::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u16x16>(1.0f), "u16x16::raw() must be constexpr");
    FL_STATIC_ASSERT(constexpr_raw_check<u24x8>(1.0f), "u24x8::raw() must be constexpr");
}

FL_TEST_CASE("constexpr from_raw - all types") {
    // Signed
    FL_STATIC_ASSERT(constexpr_from_raw_check<s4x12>(), "s4x12::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<s8x8>(), "s8x8::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<s8x24>(), "s8x24::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<s12x4>(), "s12x4::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<s16x16>(), "s16x16::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<s24x8>(), "s24x8::from_raw must be constexpr");
    // Unsigned
    FL_STATIC_ASSERT(constexpr_from_raw_check<u4x12>(), "u4x12::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<u8x8>(), "u8x8::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<u8x24>(), "u8x24::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<u12x4>(), "u12x4::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<u16x16>(), "u16x16::from_raw must be constexpr");
    FL_STATIC_ASSERT(constexpr_from_raw_check<u24x8>(), "u24x8::from_raw must be constexpr");
}

FL_TEST_CASE("constexpr arithmetic operators") {
    // Addition
    FL_STATIC_ASSERT(constexpr_add_check<s8x8>(1.5f, 0.5f), "s8x8 + must be constexpr");
    FL_STATIC_ASSERT(constexpr_add_check<s16x16>(1.5f, 0.5f), "s16x16 + must be constexpr");
    FL_STATIC_ASSERT(constexpr_add_check<u8x8>(1.5f, 0.5f), "u8x8 + must be constexpr");
    FL_STATIC_ASSERT(constexpr_add_check<u16x16>(1.5f, 0.5f), "u16x16 + must be constexpr");
    // Subtraction
    FL_STATIC_ASSERT(constexpr_sub_check<s8x8>(1.5f, 0.5f), "s8x8 - must be constexpr");
    FL_STATIC_ASSERT(constexpr_sub_check<s16x16>(1.5f, 0.5f), "s16x16 - must be constexpr");
    // Unary minus
    FL_STATIC_ASSERT(constexpr_neg_check<s8x8>(1.5f), "s8x8 unary- must be constexpr");
    FL_STATIC_ASSERT(constexpr_neg_check<s16x16>(1.5f), "s16x16 unary- must be constexpr");
    // Multiply
    FL_STATIC_ASSERT(constexpr_mul_check<s8x8>(2.0f, 0.5f), "s8x8 * must be constexpr");
    FL_STATIC_ASSERT(constexpr_mul_check<s16x16>(2.0f, 0.5f), "s16x16 * must be constexpr");
    FL_STATIC_ASSERT(constexpr_mul_check<u8x8>(2.0f, 0.5f), "u8x8 * must be constexpr");
    // Divide
    FL_STATIC_ASSERT(constexpr_div_check<s8x8>(2.0f, 0.5f), "s8x8 / must be constexpr");
    FL_STATIC_ASSERT(constexpr_div_check<s16x16>(2.0f, 0.5f), "s16x16 / must be constexpr");
    // Right shift
    FL_STATIC_ASSERT(constexpr_rshift_check<s16x16>(4.0f), "s16x16 >> must be constexpr");
}

FL_TEST_CASE("constexpr comparisons") {
    FL_STATIC_ASSERT(constexpr_lt_check<s16x16>(1.0f, 2.0f), "s16x16 < must be constexpr");
    FL_STATIC_ASSERT(constexpr_eq_check<s16x16>(1.0f), "s16x16 == must be constexpr");
    FL_STATIC_ASSERT(constexpr_lt_check<s8x8>(1.0f, 2.0f), "s8x8 < must be constexpr");
    FL_STATIC_ASSERT(constexpr_eq_check<u16x16>(1.0f), "u16x16 == must be constexpr");
}

FL_TEST_CASE("constexpr conversions") {
    FL_STATIC_ASSERT(constexpr_to_int_check<s16x16>(2.0f) == 2, "s16x16 to_int must be constexpr");
    FL_STATIC_ASSERT(constexpr_to_int_check<s8x8>(3.0f) == 3, "s8x8 to_int must be constexpr");
    // to_float constexpr check (just verify it compiles)
    FL_STATIC_ASSERT(constexpr_to_float_check<s16x16>(1.0f) > 0.0f, "s16x16 to_float must be constexpr");
}

FL_TEST_CASE("constexpr math functions") {
    // mod
    FL_STATIC_ASSERT(constexpr_mod_check<s16x16>(1.5f, 1.0f), "s16x16 mod must be constexpr");
    FL_STATIC_ASSERT(constexpr_mod_check<s8x8>(1.5f, 1.0f), "s8x8 mod must be constexpr");
    // floor
    FL_STATIC_ASSERT(constexpr_floor_check<s16x16>(1.7f), "s16x16 floor must be constexpr");
    FL_STATIC_ASSERT(constexpr_floor_check<s8x8>(1.7f), "s8x8 floor must be constexpr");
    // ceil
    FL_STATIC_ASSERT(constexpr_ceil_check<s16x16>(1.3f), "s16x16 ceil must be constexpr");
    FL_STATIC_ASSERT(constexpr_ceil_check<s8x8>(1.3f), "s8x8 ceil must be constexpr");
    // fract
    FL_STATIC_ASSERT(constexpr_fract_check<s16x16>(1.7f), "s16x16 fract must be constexpr");
    // abs
    FL_STATIC_ASSERT(constexpr_abs_check<s16x16>(-1.5f), "s16x16 abs must be constexpr");
    FL_STATIC_ASSERT(constexpr_abs_check<s8x8>(-1.5f), "s8x8 abs must be constexpr");
    // sign
    FL_STATIC_ASSERT(constexpr_sign_check<s16x16>(1.0f) == 1, "s16x16 sign(+) must be 1");
    FL_STATIC_ASSERT(constexpr_sign_check<s16x16>(-1.0f) == -1, "s16x16 sign(-) must be -1");
    // lerp
    FL_STATIC_ASSERT(constexpr_lerp_check<s16x16>(0.0f, 2.0f, 0.5f), "s16x16 lerp must be constexpr");
    // clamp
    FL_STATIC_ASSERT(constexpr_clamp_check<s16x16>(3.0f, 0.0f, 2.0f), "s16x16 clamp must be constexpr");
    // step
    FL_STATIC_ASSERT(constexpr_step_check<s16x16>(0.5f, 1.0f), "s16x16 step must be constexpr");
    // sqrt - all signed types
    FL_STATIC_ASSERT(constexpr_sqrt_check<s4x12>(1.0f), "s4x12 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<s8x8>(1.0f), "s8x8 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<s8x24>(1.0f), "s8x24 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<s12x4>(1.0f), "s12x4 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<s16x16>(1.0f), "s16x16 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<s24x8>(1.0f), "s24x8 sqrt must be constexpr");
    // sqrt - all unsigned types
    FL_STATIC_ASSERT(constexpr_sqrt_check<u4x12>(1.0f), "u4x12 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<u8x8>(1.0f), "u8x8 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<u8x24>(1.0f), "u8x24 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<u12x4>(1.0f), "u12x4 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<u16x16>(1.0f), "u16x16 sqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_sqrt_check<u24x8>(1.0f), "u24x8 sqrt must be constexpr");
    // rsqrt - all signed types
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s4x12>(1.0f), "s4x12 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s8x8>(1.0f), "s8x8 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s8x24>(1.0f), "s8x24 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s12x4>(1.0f), "s12x4 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s16x16>(1.0f), "s16x16 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<s24x8>(1.0f), "s24x8 rsqrt must be constexpr");
    // rsqrt - all unsigned types
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u4x12>(1.0f), "u4x12 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u8x8>(1.0f), "u8x8 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u8x24>(1.0f), "u8x24 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u12x4>(1.0f), "u12x4 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u16x16>(1.0f), "u16x16 rsqrt must be constexpr");
    FL_STATIC_ASSERT(constexpr_rsqrt_check<u24x8>(1.0f), "u24x8 rsqrt must be constexpr");
}

// Verify constexpr works for concrete value computation
FL_TEST_CASE("constexpr value verification") {
    // s16x16: 1.5 + 0.5 = 2.0
    FL_STATIC_ASSERT((s16x16(1.5f) + s16x16(0.5f)).raw() == s16x16(2.0f).raw(),
                  "1.5 + 0.5 must equal 2.0");

    // s16x16: 3.0 * 2.0 = 6.0
    FL_STATIC_ASSERT((s16x16(3.0f) * s16x16(2.0f)).raw() == s16x16(6.0f).raw(),
                  "3.0 * 2.0 must equal 6.0");

    // s16x16: floor(1.7) = 1.0
    FL_STATIC_ASSERT(s16x16::floor(s16x16(1.7f)).raw() == s16x16(1.0f).raw(),
                  "floor(1.7) must equal 1.0");

    // s8x8: -2.0 negation
    FL_STATIC_ASSERT((-s8x8(2.0f)).raw() == s8x8(-2.0f).raw(),
                  "-(2.0) must equal -2.0");

    // s16x16: clamp(3.0, 0.0, 2.0) = 2.0
    FL_STATIC_ASSERT(s16x16::clamp(s16x16(3.0f), s16x16(0.0f), s16x16(2.0f)).raw() == s16x16(2.0f).raw(),
                  "clamp(3.0, 0.0, 2.0) must equal 2.0");

    // s16x16: abs(-1.5) = 1.5
    FL_STATIC_ASSERT(s16x16::abs(s16x16(-1.5f)).raw() == s16x16(1.5f).raw(),
                  "abs(-1.5) must equal 1.5");

    // s16x16: sqrt(4.0) = 2.0
    FL_STATIC_ASSERT(s16x16::sqrt(s16x16(4.0f)).raw() == s16x16(2.0f).raw(),
                  "sqrt(4.0) must equal 2.0");

    // s16x16: sqrt(1.0) = 1.0
    FL_STATIC_ASSERT(s16x16::sqrt(s16x16(1.0f)).raw() == s16x16(1.0f).raw(),
                  "sqrt(1.0) must equal 1.0");

    // s16x16: sqrt(0.0) = 0.0
    FL_STATIC_ASSERT(s16x16::sqrt(s16x16(0.0f)).raw() == 0,
                  "sqrt(0.0) must equal 0.0");

    // u16x16: sqrt(4.0) = 2.0
    FL_STATIC_ASSERT(u16x16::sqrt(u16x16(4.0f)).raw() == u16x16(2.0f).raw(),
                  "u16x16 sqrt(4.0) must equal 2.0");

    // s16x16: rsqrt(1.0) = 1.0
    FL_STATIC_ASSERT(s16x16::rsqrt(s16x16(1.0f)).raw() == s16x16(1.0f).raw(),
                  "rsqrt(1.0) must equal 1.0");

    // s16x16: rsqrt(0.0) = 0.0 (zero guard)
    FL_STATIC_ASSERT(s16x16::rsqrt(s16x16(0.0f)).raw() == 0,
                  "rsqrt(0.0) must equal 0.0");

    // u16x16: rsqrt(1.0) = 1.0
    FL_STATIC_ASSERT(u16x16::rsqrt(u16x16(1.0f)).raw() == u16x16(1.0f).raw(),
                  "u16x16 rsqrt(1.0) must equal 1.0");
}

} // anonymous namespace

} // FL_TEST_FILE
