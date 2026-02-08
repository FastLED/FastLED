// Unified template tests for all signed fixed-point types.

#include "test.h"
#include "fl/fixed_point.h"
#include "fl/stl/math.h"

using namespace fl;

namespace { // Anonymous namespace for fixed_point tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// --- Tolerance functions computed from fractional precision ---

template <typename T> constexpr float tol() {
    return (T::FRAC_BITS >= 24) ? 0.0001f
         : (T::FRAC_BITS >= 16) ? 0.0005f
         : (T::FRAC_BITS >= 12) ? 0.001f
         : (T::FRAC_BITS >= 8)  ? 0.01f
         : 0.1f;
}
template <typename T> constexpr float trig_tol() {
    // Measured maxErr (sin/cos): FRAC24=0.000043, FRAC16=0.000059, FRAC12=0.000470,
    //                            FRAC8=0.007704, FRAC4=0.124
    // Measured maxErr (atan/asin/acos): FRAC16=0.000293, FRAC12=0.001075, FRAC8=0.014
    // Must cover both forward and inverse trig.
    return (T::FRAC_BITS >= 16) ? 0.0005f
         : (T::FRAC_BITS >= 12) ? 0.002f
         : (T::FRAC_BITS >= 8)  ? 0.02f
         : 0.15f;
}
template <typename T> constexpr float pyth_tol() {
    // Pythagorean identity: sin²+cos² ≈ 1. Compounds trig error.
    return (T::FRAC_BITS >= 16) ? 0.001f
         : (T::FRAC_BITS >= 12) ? 0.005f
         : (T::FRAC_BITS >= 8)  ? 0.05f
         : 0.15f;
}
template <typename T> constexpr float pow_tol() {
    // Measured maxErr (sweep): FRAC16=0.010, FRAC12=0.004, FRAC8=0.299, FRAC4=4.98
    // Template tests check specific points (pow(2,2)=4, pow(4,0.5)=2) which are easier.
    return (T::FRAC_BITS >= 16) ? 0.015f
         : (T::FRAC_BITS >= 12) ? 0.01f
         : (T::FRAC_BITS >= 8)  ? 0.1f
         : 0.5f;
}
template <typename T> constexpr float sqrt_tol() {
    // Measured maxErr: FRAC24=0.000001, FRAC16=0.000040, FRAC12=0.001367,
    //                  FRAC8=0.014063, FRAC4=0.224
    return (T::FRAC_BITS >= 16) ? 0.0002f
         : (T::FRAC_BITS >= 12) ? 0.005f
         : (T::FRAC_BITS >= 8)  ? 0.03f
         : 0.25f;
}
template <typename T> constexpr float smooth_tol() {
    // Measured maxErr: FRAC24≈0, FRAC16=0.000065, FRAC12=0.001047,
    //                  FRAC8=0.017023, FRAC4=0.191
    return (T::FRAC_BITS >= 16) ? 0.0005f
         : (T::FRAC_BITS >= 12) ? 0.003f
         : (T::FRAC_BITS >= 8)  ? 0.04f
         : 0.2f;
}
template <typename T> constexpr float rt_tol() {
    return (T::FRAC_BITS >= 16) ? 0.001f
         : (T::FRAC_BITS >= 12) ? 0.002f
         : (T::FRAC_BITS >= 8)  ? 0.02f
         : 0.15f;
}
template <typename T> float sqrt_id_tol(float v) {
    return (T::FRAC_BITS >= 24) ? v * 0.001f + 0.001f
         : (T::FRAC_BITS >= 16) ? v * 0.005f + 0.005f
         : (T::FRAC_BITS >= 12) ? v * 0.01f  + 0.01f
         : (T::FRAC_BITS >= 8)  ? v * 0.05f  + 0.1f
         : v * 0.15f + 0.5f;
}

template <typename T>
void check_near(T val, float expected, float tolerance) {
    FL_CHECK_CLOSE(val.to_float(), expected, tolerance);
}

template <typename T>
void check_near(T val, float expected) {
    check_near(val, expected, tol<T>());
}

#define FP_TYPES s4x12, s8x8, s8x24, s12x4, s16x16, s24x8

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - default construction", T, FP_TYPES) {
    T a;
    FL_CHECK_EQ(a.raw(), raw_t<T>(0));
    FL_CHECK_EQ(a.to_int(), 0);
}

FL_TEST_CASE_TEMPLATE("fixed_point - float construction", T, FP_TYPES) {
    using R = raw_t<T>;
    T one(1.0f);
    FL_CHECK_EQ(one.raw(), R(1) << T::FRAC_BITS);
    FL_CHECK_EQ(one.to_int(), 1);

    T half(0.5f);
    FL_CHECK_EQ(half.raw(), R(1) << (T::FRAC_BITS - 1));
    FL_CHECK_EQ(half.to_int(), 0);

    T neg(-1.0f);
    FL_CHECK_EQ(neg.raw(), -(R(1) << T::FRAC_BITS));
    FL_CHECK_EQ(neg.to_int(), -1);

    T neg_half(-0.5f);
    FL_CHECK_EQ(neg_half.to_int(), -1); // floor(-0.5) via arithmetic shift

    T val(3.0f);
    FL_CHECK_EQ(val.to_int(), 3);

    T neg_val(-3.0f);
    FL_CHECK_EQ(neg_val.to_int(), -3);
}

FL_TEST_CASE_TEMPLATE("fixed_point - from_raw", T, FP_TYPES) {
    using R = raw_t<T>;
    // 1.0
    T a = T::from_raw(R(1) << T::FRAC_BITS);
    FL_CHECK_EQ(a.to_int(), 1);

    // 2.5
    R raw_2_5 = (R(2) << T::FRAC_BITS) + (R(1) << (T::FRAC_BITS - 1));
    T b = T::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), 2);
    check_near(b, 2.5f);

    // smallest negative fraction
    T c = T::from_raw(R(-1));
    FL_CHECK_EQ(c.raw(), R(-1));
    FL_CHECK_EQ(c.to_int(), -1); // arithmetic shift
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - addition", T, FP_TYPES) {
    T a(1.0f), b(2.0f);
    T c = a + b;
    FL_CHECK_EQ(c.to_int(), 3);
    check_near(c, 3.0f);

    // Fractional
    T d(0.25f), e(0.75f);
    check_near(d + e, 1.0f);

    // Negative
    T f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    T zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());
}

FL_TEST_CASE_TEMPLATE("fixed_point - subtraction", T, FP_TYPES) {
    T a(3.0f), b(1.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    T zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());

    // Fractional
    T c(1.75f), d(0.25f);
    check_near(c - d, 1.5f);
}

FL_TEST_CASE_TEMPLATE("fixed_point - unary negation", T, FP_TYPES) {
    T a(3.5f);
    T neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    FL_CHECK_EQ((-neg_a).raw(), a.raw());

    // Negate zero
    T zero;
    FL_CHECK_EQ((-zero).raw(), raw_t<T>(0));
}

FL_TEST_CASE_TEMPLATE("fixed_point - fixed-point multiply", T, FP_TYPES) {
    T a(2.0f), b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    T half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 = identity
    T one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    T zero;
    FL_CHECK_EQ((a * zero).raw(), raw_t<T>(0));

    // Negative * positive
    T neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * T(-3.0f), 6.0f);

    // Fractional precision
    T c(1.5f), d(2.0f);
    check_near(c * d, 3.0f);

    // Higher fractional precision
    T e(1.5f), f(2.5f);
    check_near(e * f, 3.75f, tol<T>());
}

FL_TEST_CASE_TEMPLATE("fixed_point - fixed-point divide", T, FP_TYPES) {
    T a(6.0f), b(3.0f);
    check_near(a / b, 2.0f);

    T one(1.0f), two(2.0f);
    check_near(one / two, 0.5f);

    T four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 = identity
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // Negative dividend
    T neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / T(-3.0f), -2.0f);

    // Both negative
    check_near(neg / T(-3.0f), 2.0f);

    // Fractional result: 1/3
    T three(3.0f);
    check_near(one / three, 0.3333f, tol<T>() * 10.0f);
}

FL_TEST_CASE_TEMPLATE("fixed_point - scalar multiply", T, FP_TYPES) {
    using R = raw_t<T>;
    T a(1.5f);

    // fp * scalar
    check_near(a * R(2), 3.0f);
    check_near(a * R(0), 0.0f);
    check_near(a * R(-1), -1.5f);

    // scalar * fp (commutative friend)
    check_near(R(2) * a, 3.0f);
    check_near(R(-3) * a, -4.5f);

    // Large scalar multiply (only for types with enough INT_BITS)
    if (T::INT_BITS > 8) {
        check_near(a * R(100), 150.0f, tol<T>() * 10.0f);
    }
}

FL_TEST_CASE_TEMPLATE("fixed_point - right shift", T, FP_TYPES) {
    T a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    T b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative preserves sign (arithmetic shift)
    T neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

// ---------------------------------------------------------------------------
// Comparisons
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - comparisons", T, FP_TYPES) {
    T a(1.0f), b(2.0f), c(1.0f);
    T neg(-1.0f);
    T zero;

    FL_CHECK(a == c);
    FL_CHECK(a != b);
    FL_CHECK_FALSE(a == b);
    FL_CHECK_FALSE(a != c);

    FL_CHECK(a < b);
    FL_CHECK(b > a);
    FL_CHECK_FALSE(b < a);
    FL_CHECK_FALSE(a > b);

    FL_CHECK(a <= b);
    FL_CHECK(a <= c);
    FL_CHECK(b >= a);
    FL_CHECK(a >= c);

    FL_CHECK(neg < zero);
    FL_CHECK(neg < a);
    FL_CHECK(zero > neg);
    FL_CHECK(a > neg);
}

// ---------------------------------------------------------------------------
// Trigonometry
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - sin", T, FP_TYPES) {
    T zero;
    check_near(T::sin(zero), 0.0f, trig_tol<T>());

    T half_pi(1.5707963f);
    check_near(T::sin(half_pi), 1.0f, trig_tol<T>());

    T pi(3.1415926f);
    check_near(T::sin(pi), 0.0f, trig_tol<T>());

    T neg_half_pi(-1.5707963f);
    check_near(T::sin(neg_half_pi), -1.0f, trig_tol<T>());
}

FL_TEST_CASE_TEMPLATE("fixed_point - cos", T, FP_TYPES) {
    T zero;
    check_near(T::cos(zero), 1.0f, trig_tol<T>());

    T half_pi(1.5707963f);
    check_near(T::cos(half_pi), 0.0f, trig_tol<T>());

    T pi(3.1415926f);
    check_near(T::cos(pi), -1.0f, trig_tol<T>());
}

FL_TEST_CASE_TEMPLATE("fixed_point - sincos", T, FP_TYPES) {
    T angle(0.7854f); // ~pi/4
    T s, c;
    T::sincos(angle, s, c);

    check_near(s, 0.7071f, trig_tol<T>());
    check_near(c, 0.7071f, trig_tol<T>());

    // sincos must match individual sin/cos
    FL_CHECK_EQ(s.raw(), T::sin(angle).raw());
    FL_CHECK_EQ(c.raw(), T::cos(angle).raw());
}

FL_TEST_CASE_TEMPLATE("fixed_point - pythagorean identity", T, FP_TYPES) {
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 2.0f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        T a(ang);
        T s = T::sin(a);
        T c = T::cos(a);
        T sum = s * s + c * c;
        check_near(sum, 1.0f, pyth_tol<T>());
    }
}

// ---------------------------------------------------------------------------
// Mixed arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - mixed arithmetic expressions", T, FP_TYPES) {
    T a(2.0f), b(3.0f), c(0.5f);
    check_near((a + b) * c, 2.5f);

    T d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // Round-trip: (a / b) * b ~ a
    check_near((a / b) * b, 2.0f, rt_tol<T>());

    // Chained
    T one(1.0f);
    T result = (a + b) * c / one;
    check_near(result, 2.5f);
}

// ---------------------------------------------------------------------------
// to_float
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - to_float", T, FP_TYPES) {
    T one(1.0f);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, tol<T>());

    T half(0.5f);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, tol<T>());

    T neg(-2.5f);
    FL_CHECK_CLOSE(neg.to_float(), -2.5f, tol<T>());

    T zero;
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, tol<T>());
}

// ---------------------------------------------------------------------------
// Math functions
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - sqrt", T, FP_TYPES) {
    check_near(T::sqrt(T(4.0f)), 2.0f, sqrt_tol<T>());
    check_near(T::sqrt(T(1.0f)), 1.0f, sqrt_tol<T>());
    check_near(T::sqrt(T(2.0f)), 1.4142f, sqrt_tol<T>());

    FL_CHECK_EQ(T::sqrt(T()).raw(), raw_t<T>(0));        // sqrt(0) = 0
    FL_CHECK_EQ(T::sqrt(T(-1.0f)).raw(), raw_t<T>(0));   // sqrt(neg) = 0

    // sqrt(9) = 3  (only for types that can represent 9)
    if (T::INT_BITS > 4) {
        check_near(T::sqrt(T(9.0f)), 3.0f, sqrt_tol<T>());
    }
}

FL_TEST_CASE_TEMPLATE("fixed_point - rsqrt", T, FP_TYPES) {
    check_near(T::rsqrt(T(4.0f)), 0.5f, sqrt_tol<T>());
    check_near(T::rsqrt(T(1.0f)), 1.0f, sqrt_tol<T>());

    FL_CHECK_EQ(T::rsqrt(T()).raw(), raw_t<T>(0));       // rsqrt(0) = 0
    FL_CHECK_EQ(T::rsqrt(T(-1.0f)).raw(), raw_t<T>(0));  // rsqrt(neg) = 0
}

FL_TEST_CASE_TEMPLATE("fixed_point - pow", T, FP_TYPES) {
    // 2^2 = 4 (safe for all types)
    check_near(T::pow(T(2.0f), T(2.0f)), 4.0f, pow_tol<T>());

    // 4^0.5 = 2 (sqrt via pow)
    check_near(T::pow(T(4.0f), T(0.5f)), 2.0f, pow_tol<T>());

    // x^0 = 1
    check_near(T::pow(T(5.0f), T(0.0f)), 1.0f, pow_tol<T>());

    // 0^x = 0
    FL_CHECK_EQ(T::pow(T(), T(2.0f)).raw(), raw_t<T>(0));

    // negative base = 0
    FL_CHECK_EQ(T::pow(T(-1.0f), T(2.0f)).raw(), raw_t<T>(0));

    // 2^3 = 8 (only for types that can represent 8)
    if (T::INT_BITS > 4) {
        check_near(T::pow(T(2.0f), T(3.0f)), 8.0f, pow_tol<T>());
    }
}

FL_TEST_CASE_TEMPLATE("fixed_point - sqrt identity", T, FP_TYPES) {
    float values[] = {1.0f, 2.0f, 4.0f};
    for (float v : values) {
        T x(v);
        T s = T::sqrt(x);
        T sq = s * s;
        check_near(sq, v, sqrt_id_tol<T>(v));
    }

    // Extended values for types with enough range
    if (T::INT_BITS > 4) {
        float ext_values[] = {9.0f, 16.0f};
        for (float v : ext_values) {
            T x(v);
            T s = T::sqrt(x);
            T sq = s * s;
            check_near(sq, v, sqrt_id_tol<T>(v));
        }
    }
    if (T::INT_BITS > 8) {
        float big_values[] = {0.25f, 100.0f};
        for (float v : big_values) {
            T x(v);
            T s = T::sqrt(x);
            T sq = s * s;
            check_near(sq, v, sqrt_id_tol<T>(v));
        }
    }
}

FL_TEST_CASE_TEMPLATE("fixed_point - floor and ceil", T, FP_TYPES) {
    check_near(T::floor(T(2.75f)), 2.0f);
    check_near(T::ceil(T(2.75f)), 3.0f);
    check_near(T::floor(T(-1.25f)), -2.0f);
    check_near(T::ceil(T(-1.25f)), -1.0f);
    check_near(T::floor(T(3.0f)), 3.0f);
    check_near(T::ceil(T(3.0f)), 3.0f);
}

FL_TEST_CASE_TEMPLATE("fixed_point - fract", T, FP_TYPES) {
    check_near(T::fract(T(2.75f)), 0.75f);
    FL_CHECK_EQ(T::fract(T(1.0f)).raw(), raw_t<T>(0));
    check_near(T::fract(T(0.5f)), 0.5f);
}

FL_TEST_CASE_TEMPLATE("fixed_point - abs and sign", T, FP_TYPES) {
    check_near(T::abs(T(3.5f)), 3.5f);
    check_near(T::abs(T(-3.5f)), 3.5f);
    FL_CHECK_EQ(T::abs(T()).raw(), raw_t<T>(0));

    check_near(T::sign(T(5.0f)), 1.0f);
    check_near(T::sign(T(-5.0f)), -1.0f);
    FL_CHECK_EQ(T::sign(T()).raw(), raw_t<T>(0));
}

FL_TEST_CASE_TEMPLATE("fixed_point - mod", T, FP_TYPES) {
    check_near(T::mod(T(5.5f), T(2.0f)), 1.5f);
    check_near(T::mod(T(3.0f), T(1.5f)), 0.0f);
    check_near(T::mod(T(1.0f), T(3.0f)), 1.0f);
}

// ---------------------------------------------------------------------------
// Inverse trigonometry
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - inverse trig", T, FP_TYPES) {
    check_near(T::atan(T(1.0f)), 0.7854f, trig_tol<T>());
    check_near(T::atan(T(0.0f)), 0.0f, trig_tol<T>());
    check_near(T::atan2(T(1.0f), T(1.0f)), 0.7854f, trig_tol<T>());
    check_near(T::asin(T(0.0f)), 0.0f, trig_tol<T>());
    check_near(T::asin(T(1.0f)), 1.5708f, trig_tol<T>());
    check_near(T::acos(T(1.0f)), 0.0f, trig_tol<T>());
    check_near(T::acos(T(0.0f)), 1.5708f, trig_tol<T>());
}

// ---------------------------------------------------------------------------
// Interpolation and clamping
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - lerp, clamp, step, smoothstep", T, FP_TYPES) {
    // lerp
    check_near(T::lerp(T(0.0f), T(4.0f), T(0.5f)), 2.0f, tol<T>());
    check_near(T::lerp(T(1.0f), T(3.0f), T(0.0f)), 1.0f);
    check_near(T::lerp(T(1.0f), T(3.0f), T(1.0f)), 3.0f);

    // Wider lerp range (for types that can represent 10)
    if (T::INT_BITS > 4) {
        check_near(T::lerp(T(0.0f), T(10.0f), T(0.5f)), 5.0f, tol<T>() * 10.0f);
    }

    // clamp
    check_near(T::clamp(T(5.0f), T(0.0f), T(3.0f)), 3.0f);
    check_near(T::clamp(T(-1.0f), T(0.0f), T(3.0f)), 0.0f);
    check_near(T::clamp(T(1.5f), T(0.0f), T(3.0f)), 1.5f);

    // step
    check_near(T::step(T(1.0f), T(0.5f)), 0.0f);
    check_near(T::step(T(1.0f), T(1.5f)), 1.0f);
    check_near(T::step(T(1.0f), T(1.0f)), 1.0f);

    // smoothstep
    check_near(T::smoothstep(T(0.0f), T(1.0f), T(-0.5f)), 0.0f, smooth_tol<T>());
    check_near(T::smoothstep(T(0.0f), T(1.0f), T(1.5f)), 1.0f, smooth_tol<T>());
    check_near(T::smoothstep(T(0.0f), T(1.0f), T(0.5f)), 0.5f, smooth_tol<T>());
}

// ---------------------------------------------------------------------------
// Edge values
// ---------------------------------------------------------------------------

FL_TEST_CASE_TEMPLATE("fixed_point - edge values", T, FP_TYPES) {
    using R = raw_t<T>;
    constexpr int max_int = (1 << (T::INT_BITS - 1)) - 1;
    constexpr int min_int = -(1 << (T::INT_BITS - 1));

    T big(static_cast<float>(max_int));
    FL_CHECK_EQ(big.to_int(), max_int);

    T neg_big(static_cast<float>(min_int));
    FL_CHECK_EQ(neg_big.to_int(), min_int);

    // Smallest positive fraction
    T tiny = T::from_raw(R(1));
    FL_CHECK_EQ(tiny.raw(), R(1));
    FL_CHECK_EQ(tiny.to_int(), 0);
    FL_CHECK(tiny > T());

    // Smallest negative fraction
    T neg_tiny = T::from_raw(R(-1));
    FL_CHECK_EQ(neg_tiny.raw(), R(-1));
    FL_CHECK(neg_tiny < T());
}

// ---------------------------------------------------------------------------
// Accuracy measurement for s16x16 transcendental functions.
// Sweeps input ranges and measures max/avg absolute error vs float reference.
// ---------------------------------------------------------------------------

struct AccuracyResult {
    float maxErr;
    float avgErr;
    float worstInput;
    float worstInput2;
    int nSamples;
};

AccuracyResult measure_sin() {
    AccuracyResult r = {};
    const int N = 10000;
    const float lo = -6.2831853f;
    const float hi = 6.2831853f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::sinf(x);
        float got = s16x16::sin(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_cos() {
    AccuracyResult r = {};
    const int N = 10000;
    const float lo = -6.2831853f;
    const float hi = 6.2831853f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::cosf(x);
        float got = s16x16::cos(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_atan() {
    AccuracyResult r = {};
    const int N = 10000;
    const float lo = -8.0f;
    const float hi = 8.0f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::atanf(x);
        float got = s16x16::atan(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_atan2() {
    AccuracyResult r = {};
    const int N = 100;
    const float lo = -7.0f;
    const float hi = 7.0f;
    float sumErr = 0;
    int count = 0;
    for (int iy = 0; iy < N; ++iy) {
        float y = lo + (hi - lo) * iy / (N - 1);
        for (int ix = 0; ix < N; ++ix) {
            float x = lo + (hi - lo) * ix / (N - 1);
            if (fl::fabsf(x) < 0.1f && fl::fabsf(y) < 0.1f) continue;
            float ref = fl::atan2f(y, x);
            float got = s16x16::atan2(s16x16(y), s16x16(x)).to_float();
            float err = fl::fabsf(got - ref);
            sumErr += err;
            ++count;
            if (err > r.maxErr) {
                r.maxErr = err;
                r.worstInput = y;
                r.worstInput2 = x;
            }
        }
    }
    r.avgErr = count > 0 ? sumErr / count : 0;
    r.nSamples = count;
    return r;
}

AccuracyResult measure_asin() {
    AccuracyResult r = {};
    const int N = 2000;
    const float lo = -0.999f;
    const float hi = 0.999f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::asinf(x);
        float got = s16x16::asin(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_acos() {
    AccuracyResult r = {};
    const int N = 2000;
    const float lo = -0.999f;
    const float hi = 0.999f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::acosf(x);
        float got = s16x16::acos(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_sqrt_accuracy() {
    AccuracyResult r = {};
    const int N = 10000;
    const float lo = 0.001f;
    const float hi = 32000.0f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::sqrtf(x);
        float got = s16x16::sqrt(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_rsqrt() {
    AccuracyResult r = {};
    const int N = 5000;
    const float lo = 0.01f;
    const float hi = 1000.0f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = 1.0f / fl::sqrtf(x);
        float got = s16x16::rsqrt(s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

AccuracyResult measure_pow(float exponent, float baseLo, float baseHi) {
    AccuracyResult r = {};
    const int N = 5000;
    float sumErr = 0;
    int count = 0;
    for (int i = 0; i < N; ++i) {
        float base = baseLo + (baseHi - baseLo) * i / (N - 1);
        float ref = fl::powf(base, exponent);
        if (ref > 32000.0f || ref < 0.0f) continue;
        float got = s16x16::pow(s16x16(base), s16x16(exponent)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        ++count;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = base; }
    }
    r.avgErr = count > 0 ? sumErr / count : 0;
    r.nSamples = count;
    return r;
}

AccuracyResult measure_smoothstep() {
    AccuracyResult r = {};
    const int N = 5000;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = -0.5f + 2.0f * i / (N - 1);
        float t = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        float ref = t * t * (3.0f - 2.0f * t);
        float got = s16x16::smoothstep(s16x16(0.0f), s16x16(1.0f), s16x16(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

void print_row(const char *name, const AccuracyResult &r) {
    fl::printf("%s: maxErr=%.6f avgErr=%.6f worstInput=%.4f\n",
               name, r.maxErr, r.avgErr, r.worstInput);
}

void print_row2(const char *name, const AccuracyResult &r) {
    fl::printf("%s: maxErr=%.6f avgErr=%.6f worstInput=(%.4f, %.4f)\n",
               name, r.maxErr, r.avgErr, r.worstInput, r.worstInput2);
}

FL_TEST_CASE("s16x16 accuracy report") {
    fl::printf("\ns16x16 Accuracy Report (vs float reference)\n");

    AccuracyResult sin_r = measure_sin();
    print_row("sin", sin_r);

    AccuracyResult cos_r = measure_cos();
    print_row("cos", cos_r);

    AccuracyResult atan_r = measure_atan();
    print_row("atan", atan_r);

    AccuracyResult atan2_r = measure_atan2();
    print_row2("atan2", atan2_r);

    AccuracyResult asin_r = measure_asin();
    print_row("asin", asin_r);

    AccuracyResult acos_r = measure_acos();
    print_row("acos", acos_r);

    AccuracyResult sqrt_r = measure_sqrt_accuracy();
    print_row("sqrt", sqrt_r);

    AccuracyResult rsqrt_r = measure_rsqrt();
    print_row("rsqrt", rsqrt_r);

    AccuracyResult pow05_r = measure_pow(0.5f, 0.1f, 100.0f);
    print_row("pow(x,0.5)", pow05_r);

    AccuracyResult pow06_r = measure_pow(0.6f, 0.1f, 100.0f);
    print_row("pow(x,0.6)", pow06_r);

    AccuracyResult pow20_r = measure_pow(2.0f, 0.1f, 10.0f);
    print_row("pow(x,2.0)", pow20_r);

    AccuracyResult pow30_r = measure_pow(3.0f, 0.1f, 5.0f);
    print_row("pow(x,3.0)", pow30_r);

    AccuracyResult smooth_r = measure_smoothstep();
    print_row("smoothstep", smooth_r);

    fl::printf("\n");

    // Bounds set ~20% above measured values to catch regressions.
    FL_CHECK_LT(sin_r.maxErr, 0.00007f);   // measured: 0.000049
    FL_CHECK_LT(cos_r.maxErr, 0.00007f);   // measured: 0.000049
    FL_CHECK_LT(atan_r.maxErr, 0.0005f);   // measured: 0.000289
    FL_CHECK_LT(atan2_r.maxErr, 0.0005f);  // measured: 0.000288
    FL_CHECK_LT(asin_r.maxErr, 0.0005f);   // measured: 0.000293
    FL_CHECK_LT(acos_r.maxErr, 0.0005f);   // measured: 0.000289
    FL_CHECK_LT(sqrt_r.maxErr, 0.0003f);   // measured: 0.000144
    FL_CHECK_LT(rsqrt_r.maxErr, 0.005f);   // measured: 0.003967
    FL_CHECK_LT(pow05_r.maxErr, 0.002f);   // measured: 0.001374
    FL_CHECK_LT(pow06_r.maxErr, 0.004f);   // measured: 0.002517
    FL_CHECK_LT(pow20_r.maxErr, 0.05f);    // measured: 0.038174
    FL_CHECK_LT(pow30_r.maxErr, 0.07f);    // measured: 0.054359
    FL_CHECK_LT(smooth_r.maxErr, 0.0002f); // measured: 0.000069
}

// ---------------------------------------------------------------------------
// Multi-type accuracy measurement (template-based)
// ---------------------------------------------------------------------------

template <typename T>
AccuracyResult measure_sin_t() {
    AccuracyResult r = {};
    const int N = 5000;
    const float lo = -6.2831853f;
    const float hi = 6.2831853f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::sinf(x);
        float got = T::sin(T(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

template <typename T>
AccuracyResult measure_atan_t() {
    AccuracyResult r = {};
    const int N = 5000;
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    const float lo = -(maxRange < 7.0f ? maxRange : 7.0f);
    const float hi = (maxRange < 7.0f ? maxRange : 7.0f);
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::atanf(x);
        float got = T::atan(T(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

template <typename T>
AccuracyResult measure_sqrt_t() {
    AccuracyResult r = {};
    const int N = 5000;
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    const float lo = 0.01f;
    const float hi = (maxRange < 100.0f ? maxRange : 100.0f);
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::sqrtf(x);
        if (ref > maxRange) continue;
        float got = T::sqrt(T(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

template <typename T>
AccuracyResult measure_pow_t(float exponent, float baseLo, float baseHi) {
    AccuracyResult r = {};
    const int N = 3000;
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    float sumErr = 0;
    int count = 0;
    for (int i = 0; i < N; ++i) {
        float base = baseLo + (baseHi - baseLo) * i / (N - 1);
        float ref = fl::powf(base, exponent);
        if (ref > maxRange || ref < 0.0f) continue;
        float got = T::pow(T(base), T(exponent)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        ++count;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = base; }
    }
    r.avgErr = count > 0 ? sumErr / count : 0;
    r.nSamples = count;
    return r;
}

template <typename T>
AccuracyResult measure_smoothstep_t() {
    AccuracyResult r = {};
    const int N = 3000;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = -0.5f + 2.0f * i / (N - 1);
        float t = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        float ref = t * t * (3.0f - 2.0f * t);
        float got = T::smoothstep(T(0.0f), T(1.0f), T(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

template <typename T>
void run_type_accuracy(const char* name) {
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    AccuracyResult sin_r = measure_sin_t<T>();
    AccuracyResult atan_r = measure_atan_t<T>();
    AccuracyResult sqrt_r = measure_sqrt_t<T>();
    AccuracyResult pow05_r = measure_pow_t<T>(0.5f, 0.1f,
        (maxRange < 10.0f ? maxRange : 10.0f));
    AccuracyResult pow20_r = measure_pow_t<T>(2.0f, 0.1f,
        (maxRange < 5.0f ? maxRange : 5.0f));
    AccuracyResult smooth_r = measure_smoothstep_t<T>();

    const int frac = T::FRAC_BITS;
    fl::printf("%s (FRAC=%d): sin=%.6f atan=%.6f sqrt=%.6f pow05=%.6f pow20=%.6f smooth=%.6f\n",
               name, frac,
               sin_r.maxErr, atan_r.maxErr, sqrt_r.maxErr,
               pow05_r.maxErr, pow20_r.maxErr, smooth_r.maxErr);
}

FL_TEST_CASE("multi-type accuracy report") {
    fl::printf("\nMulti-type Accuracy Report (maxErr vs float)\n");
    run_type_accuracy<s4x12>("s4x12 ");
    run_type_accuracy<s8x8>("s8x8  ");
    run_type_accuracy<s8x24>("s8x24 ");
    run_type_accuracy<s12x4>("s12x4 ");
    run_type_accuracy<s16x16>("s16x16");
    run_type_accuracy<s24x8>("s24x8 ");
    fl::printf("\n");
}

} // anonymous namespace
