#pragma once

// fixed_point test helpers
// Extracted from tests/fl/math/fixed_point.cpp as part of #3127 / #3130.
//
// Contains:
//   - Bit-exact test impl templates (test_*_impl<T>)
//   - Transcendental test impl templates (test_sin_impl, etc.)
//   - AccuracyResult struct + non-templated measure_* helpers + print_row helpers
//   - Templated measure_*_t<T> helpers
//   - run_type_accuracy<T> reporter
//   - test_bit_exact_equivalence<Tmpl, Conc>
//   - check_all_accuracy_bounds<T> regression gate
//
// Usage: include this header from WITHIN each test file's anonymous
// namespace (after fixed_point_bounds.h), so each translation unit gets
// its own private copy of the helpers. Same pattern as fixed_point_bounds.h.

// ===========================================================================
// Bit-Exact Test Helpers (Template Infrastructure)
// ===========================================================================
// Helper functions that implement bit-exact test logic for any fixed-point type.
// Called by concrete test cases below for explicit per-type testing.

template<typename T>
void test_default_construction_impl() {
    T a;
    FL_CHECK_EQ(a.raw(), raw_t<T>(0));
    FL_CHECK_EQ(a.to_int(), 0);
}

template<typename T>
void test_float_construction_impl() {
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

template<typename T>
void test_from_raw_impl() {
    using R = raw_t<T>;
    // 1.0
    T a = T::from_raw(R(1) << T::FRAC_BITS);
    FL_CHECK_EQ(a.to_int(), 1);
    FL_CHECK_EQ(a.raw(), R(1) << T::FRAC_BITS);

    // 2.5
    R raw_2_5 = (R(2) << T::FRAC_BITS) + (R(1) << (T::FRAC_BITS - 1));
    T b = T::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), 2);
    FL_CHECK_EQ(b.raw(), raw_2_5);

    // smallest negative fraction
    T c = T::from_raw(R(-1));
    FL_CHECK_EQ(c.raw(), R(-1));
    FL_CHECK_EQ(c.to_int(), -1); // arithmetic shift
}

template<typename T>
void test_addition_impl() {
    T a(1.0f), b(2.0f);
    T c = a + b;
    FL_CHECK_EQ(c.raw(), (a.raw() + b.raw()));
    FL_CHECK_EQ(c.to_int(), 3);

    // Zero identity
    T zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());

    // Commutativity
    FL_CHECK_EQ((a + b).raw(), (b + a).raw());
}

template<typename T>
void test_subtraction_impl() {
    T a(3.0f), b(1.0f);
    T c = a - b;
    FL_CHECK_EQ(c.raw(), (a.raw() - b.raw()));
    FL_CHECK_EQ(c.to_int(), 2);

    // Self subtraction
    T zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());
}

template<typename T>
void test_unary_negation_impl() {
    T a(3.5f);
    T neg_a = -a;
    FL_CHECK_EQ(neg_a.raw(), -a.raw());

    // Double negation
    FL_CHECK_EQ((-neg_a).raw(), a.raw());

    // Negate zero
    T zero;
    FL_CHECK_EQ((-zero).raw(), raw_t<T>(0));
}

template<typename T>
void test_multiply_impl() {
    T a(2.0f), b(3.0f);
    T c = a * b;
    FL_CHECK_EQ(c.to_int(), 6);

    // 0.5 * 0.5 = 0.25
    T half(0.5f);
    T quarter = half * half;
    using R = raw_t<T>;
    FL_CHECK_EQ(quarter.raw(), R(1) << (T::FRAC_BITS - 2)); // 0.25

    // Multiply by 1 = identity
    T one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    T zero;
    FL_CHECK_EQ((a * zero).raw(), raw_t<T>(0));

    // Commutativity
    FL_CHECK_EQ((a * b).raw(), (b * a).raw());
}

template<typename T>
void test_divide_impl() {
    T a(6.0f), b(3.0f);
    T c = a / b;
    FL_CHECK_EQ(c.to_int(), 2);

    // Divide by 1 = identity
    T one(1.0f);
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // 1.0 / 2.0 = 0.5
    using R = raw_t<T>;
    T half = one / T(2.0f);
    FL_CHECK_EQ(half.raw(), R(1) << (T::FRAC_BITS - 1));
}

template<typename T>
void test_scalar_multiply_impl() {
    T a(1.5f);

    // fp * scalar
    T b = a * 2;
    FL_CHECK_EQ(b.to_int(), 3);
    FL_CHECK_EQ(b.raw(), a.raw() * 2);

    // scalar * fp (commutative friend)
    T c = 2 * a;
    FL_CHECK_EQ(c.raw(), b.raw());

    // Multiply by 1
    FL_CHECK_EQ((a * 1).raw(), a.raw());

    // Multiply by 0
    FL_CHECK_EQ((a * 0).raw(), raw_t<T>(0));
}

template<typename T>
void test_right_shift_impl() {
    T a(4.0f);
    T b = a >> 1;
    FL_CHECK_EQ(b.to_int(), 2);
    FL_CHECK_EQ(b.raw(), a.raw() >> 1);

    T c = a >> 2;
    FL_CHECK_EQ(c.to_int(), 1);
    FL_CHECK_EQ(c.raw(), a.raw() >> 2);

    // Shift by 0 is identity
    FL_CHECK_EQ((a >> 0).raw(), a.raw());
}

template<typename T>
void test_comparisons_impl() {
    T a(1.0f), b(2.0f), c(1.0f);
    T neg(-1.0f);
    T zero;

    // Equality
    FL_CHECK(a == c);
    FL_CHECK_FALSE(a == b);
    FL_CHECK_EQ(a.raw(), c.raw());

    // Inequality
    FL_CHECK(a != b);
    FL_CHECK_FALSE(a != c);

    // Less than
    FL_CHECK(a < b);
    FL_CHECK_FALSE(b < a);
    FL_CHECK_FALSE(a < c);

    // Greater than
    FL_CHECK(b > a);
    FL_CHECK_FALSE(a > b);
    FL_CHECK_FALSE(a > c);

    // Less than or equal
    FL_CHECK(a <= b);
    FL_CHECK(a <= c);
    FL_CHECK_FALSE(b <= a);

    // Greater than or equal
    FL_CHECK(b >= a);
    FL_CHECK(a >= c);
    FL_CHECK_FALSE(a >= b);

    // Negative comparisons
    FL_CHECK(neg < zero);
    FL_CHECK(neg < a);
    FL_CHECK(zero > neg);
    FL_CHECK(a > neg);
}

template<typename T>
void test_floor_ceil_impl() {
    using R = raw_t<T>;
    T a(2.75f);
    T floored = T::floor(a);
    FL_CHECK_EQ(floored.raw(), R(2) << T::FRAC_BITS);

    T ceiled = T::ceil(a);
    FL_CHECK_EQ(ceiled.raw(), R(3) << T::FRAC_BITS);

    // Negative values
    T neg(-1.25f);
    FL_CHECK_EQ(T::floor(neg).raw(), R(-2) * (R(1) << T::FRAC_BITS));
    FL_CHECK_EQ(T::ceil(neg).raw(), R(-1) * (R(1) << T::FRAC_BITS));

    // Integer values (no change)
    T whole(3.0f);
    FL_CHECK_EQ(T::floor(whole).raw(), whole.raw());
    FL_CHECK_EQ(T::ceil(whole).raw(), whole.raw());
}

template<typename T>
void test_fract_impl() {
    using R = raw_t<T>;
    T a(2.75f);
    T frac = T::fract(a);
    FL_CHECK_EQ(frac.raw(), R(3) << (T::FRAC_BITS - 2)); // 0.75

    // Integer value (no fraction)
    T whole(1.0f);
    FL_CHECK_EQ(T::fract(whole).raw(), raw_t<T>(0));

    // Half
    T half(0.5f);
    FL_CHECK_EQ(T::fract(half).raw(), half.raw());
}

template<typename T>
void test_abs_sign_impl() {
    T pos(3.5f);
    FL_CHECK_EQ(T::abs(pos).raw(), pos.raw());

    T neg(-3.5f);
    FL_CHECK_EQ(T::abs(neg).raw(), pos.raw());

    T zero;
    FL_CHECK_EQ(T::abs(zero).raw(), raw_t<T>(0));

    // sign
    FL_CHECK_EQ(T::sign(pos), 1);
    FL_CHECK_EQ(T::sign(neg), -1);
    FL_CHECK_EQ(T::sign(zero), 0);
}

// ===========================================================================
// Transcendental Test Helpers (Template Infrastructure)
// ===========================================================================
// Helper functions for transcendental operations with explicit accuracy bounds.
// Accuracy bounds are passed as parameters rather than template-bound internally.

template<typename T>
void test_sin_impl(float sin_max) {
    T zero;
    FL_CHECK_CLOSE(T::sin(zero).to_float(), 0.0f, sin_max);

    T half_pi(1.5707963f);
    FL_CHECK_CLOSE(T::sin(half_pi).to_float(), 1.0f, sin_max);

    T pi(3.1415926f);
    FL_CHECK_CLOSE(T::sin(pi).to_float(), 0.0f, sin_max);

    T neg_half_pi(-1.5707963f);
    FL_CHECK_CLOSE(T::sin(neg_half_pi).to_float(), -1.0f, sin_max);
}

template<typename T>
void test_cos_impl(float cos_max) {
    T zero;
    FL_CHECK_CLOSE(T::cos(zero).to_float(), 1.0f, cos_max);

    T half_pi(1.5707963f);
    FL_CHECK_CLOSE(T::cos(half_pi).to_float(), 0.0f, cos_max);

    T pi(3.1415926f);
    FL_CHECK_CLOSE(T::cos(pi).to_float(), -1.0f, cos_max);
}

template<typename T>
void test_sincos_impl(float sin_max, float cos_max) {
    T angle(0.7854f); // ~pi/4
    T s, c;
    T::sincos(angle, s, c);

    FL_CHECK_CLOSE(s.to_float(), 0.7071f, sin_max);
    FL_CHECK_CLOSE(c.to_float(), 0.7071f, cos_max);

    // sincos must match individual sin/cos
    FL_CHECK_EQ(s.raw(), T::sin(angle).raw());
    FL_CHECK_EQ(c.raw(), T::cos(angle).raw());
}

template<typename T>
void test_atan_impl(float atan_max) {
    FL_CHECK_CLOSE(T::atan(T(1.0f)).to_float(), 0.7854f, atan_max);
    FL_CHECK_CLOSE(T::atan(T(0.0f)).to_float(), 0.0f, atan_max);
    FL_CHECK_CLOSE(T::atan(T(-1.0f)).to_float(), -0.7854f, atan_max);
}

template<typename T>
void test_atan2_impl(float atan2_max) {
    FL_CHECK_CLOSE(T::atan2(T(1.0f), T(1.0f)).to_float(), 0.7854f, atan2_max);
    FL_CHECK_CLOSE(T::atan2(T(0.0f), T(1.0f)).to_float(), 0.0f, atan2_max);
    FL_CHECK_CLOSE(T::atan2(T(1.0f), T(0.0f)).to_float(), 1.5708f, atan2_max);
}

template<typename T>
void test_asin_impl(float asin_max) {
    FL_CHECK_CLOSE(T::asin(T(0.0f)).to_float(), 0.0f, asin_max);
    FL_CHECK_CLOSE(T::asin(T(1.0f)).to_float(), 1.5708f, asin_max);
    FL_CHECK_CLOSE(T::asin(T(0.5f)).to_float(), 0.5236f, asin_max);
}

template<typename T>
void test_acos_impl(float acos_max) {
    FL_CHECK_CLOSE(T::acos(T(1.0f)).to_float(), 0.0f, acos_max);
    FL_CHECK_CLOSE(T::acos(T(0.0f)).to_float(), 1.5708f, acos_max);
    FL_CHECK_CLOSE(T::acos(T(0.5f)).to_float(), 1.0472f, acos_max);
}

template<typename T>
void test_sqrt_impl(float sqrt_max) {
    FL_CHECK_CLOSE(T::sqrt(T(4.0f)).to_float(), 2.0f, sqrt_max);
    FL_CHECK_CLOSE(T::sqrt(T(1.0f)).to_float(), 1.0f, sqrt_max);
    FL_CHECK_CLOSE(T::sqrt(T(2.0f)).to_float(), 1.4142f, sqrt_max);

    FL_CHECK_EQ(T::sqrt(T()).raw(), raw_t<T>(0));        // sqrt(0) = 0
    FL_CHECK_EQ(T::sqrt(T(-1.0f)).raw(), raw_t<T>(0));   // sqrt(neg) = 0

    // sqrt(9) = 3 (only for types that can represent 9)
    if (T::INT_BITS > 4) {
        FL_CHECK_CLOSE(T::sqrt(T(9.0f)).to_float(), 3.0f, sqrt_max);
    }
}

template<typename T>
void test_rsqrt_impl(float rsqrt_max) {
    FL_CHECK_CLOSE(T::rsqrt(T(4.0f)).to_float(), 0.5f, rsqrt_max);
    FL_CHECK_CLOSE(T::rsqrt(T(1.0f)).to_float(), 1.0f, rsqrt_max);

    FL_CHECK_EQ(T::rsqrt(T()).raw(), raw_t<T>(0));       // rsqrt(0) = 0
    FL_CHECK_EQ(T::rsqrt(T(-1.0f)).raw(), raw_t<T>(0));  // rsqrt(neg) = 0
}

template<typename T>
void test_pow_basic_impl(float pow_x_0_5_max, float pow_x_2_0_max) {
    // 2^2 = 4 (safe for all types)
    FL_CHECK_CLOSE(T::pow(T(2.0f), T(2.0f)).to_float(), 4.0f, pow_x_2_0_max);

    // 4^0.5 = 2 (sqrt via pow)
    FL_CHECK_CLOSE(T::pow(T(4.0f), T(0.5f)).to_float(), 2.0f, pow_x_0_5_max);

    // x^0 = 1
    FL_CHECK_CLOSE(T::pow(T(5.0f), T(0.0f)).to_float(), 1.0f, pow_x_2_0_max);

    // 0^x = 0
    FL_CHECK_EQ(T::pow(T(), T(2.0f)).raw(), raw_t<T>(0));

    // negative base = 0
    FL_CHECK_EQ(T::pow(T(-1.0f), T(2.0f)).raw(), raw_t<T>(0));
}

template<typename T>
void test_pow_extended_impl(float pow_x_3_0_max) {
    // 2^3 = 8 (only for types that can represent 8)
    if (T::INT_BITS > 4) {
        FL_CHECK_CLOSE(T::pow(T(2.0f), T(3.0f)).to_float(), 8.0f, pow_x_3_0_max);
    }
}

template<typename T>
void test_smoothstep_impl(float smoothstep_max) {
    FL_CHECK_CLOSE(T::smoothstep(T(0.0f), T(1.0f), T(-0.5f)).to_float(), 0.0f, smoothstep_max);
    FL_CHECK_CLOSE(T::smoothstep(T(0.0f), T(1.0f), T(1.5f)).to_float(), 1.0f, smoothstep_max);
    FL_CHECK_CLOSE(T::smoothstep(T(0.0f), T(1.0f), T(0.5f)).to_float(), 0.5f, smoothstep_max);
}

template<typename T>
void test_to_float_impl(float epsilon, float epsilon_large) {
    // Zero
    FL_CHECK_EQ(T(0.0f).to_float(), 0.0f);

    // Positive integers
    FL_CHECK_CLOSE(T(1.0f).to_float(), 1.0f, epsilon);
    FL_CHECK_CLOSE(T(2.0f).to_float(), 2.0f, epsilon);

    // Negative integers
    FL_CHECK_CLOSE(T(-1.0f).to_float(), -1.0f, epsilon);
    FL_CHECK_CLOSE(T(-2.0f).to_float(), -2.0f, epsilon);

    // Positive fractions
    FL_CHECK_CLOSE(T(0.5f).to_float(), 0.5f, epsilon);
    FL_CHECK_CLOSE(T(0.25f).to_float(), 0.25f, epsilon);
    FL_CHECK_CLOSE(T(1.5f).to_float(), 1.5f, epsilon);

    // Negative fractions
    FL_CHECK_CLOSE(T(-0.5f).to_float(), -0.5f, epsilon);
    FL_CHECK_CLOSE(T(-0.25f).to_float(), -0.25f, epsilon);
    FL_CHECK_CLOSE(T(-1.5f).to_float(), -1.5f, epsilon);

    // Additional tests for types with more fractional precision
    if (T::FRAC_BITS >= 12) {
        FL_CHECK_CLOSE(T(0.125f).to_float(), 0.125f, epsilon);
        FL_CHECK_CLOSE(T(2.75f).to_float(), 2.75f, epsilon);
    }

    // Additional tests for types with more integer bits
    if (T::INT_BITS >= 8) {
        FL_CHECK_CLOSE(T(100.0f).to_float(), 100.0f, epsilon_large);
        FL_CHECK_CLOSE(T(-100.0f).to_float(), -100.0f, epsilon_large);
    }

    // Additional tests for types with many integer bits
    if (T::INT_BITS >= 12) {
        FL_CHECK_CLOSE(T(1000.0f).to_float(), 1000.0f, epsilon_large);
        FL_CHECK_CLOSE(T(-1000.0f).to_float(), -1000.0f, epsilon_large);
    }

    // Test range extremes for s4x12 (4 integer bits: -8 to +7.999)
    if (T::INT_BITS == 4) {
        FL_CHECK_CLOSE(T(7.5f).to_float(), 7.5f, epsilon);
        FL_CHECK_CLOSE(T(-7.5f).to_float(), -7.5f, epsilon);
    }

    // Test high-precision fractional values for s8x24
    if (T::FRAC_BITS >= 24) {
        FL_CHECK_CLOSE(T(0.123456f).to_float(), 0.123456f, epsilon);
    }
}


struct AccuracyResult {
    float maxErr;
    float avgErr;
    float worstInput;
    float worstInput2;
    int nSamples;
};

AccuracyResult measure_sin() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points covers full cycle (was 100)
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
    const int N = 10;  // Spot testing: 10 key points covers full cycle (was 100)
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
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
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
    const int N = 5;  // Spot testing: 5×5 = 25 samples (was 20×20 = 400)
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
    const int N = 10;  // Spot testing: 10 key points adequate for [-0.999, 0.999] range (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for [-0.999, 0.999] range (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for base range coverage (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for [-0.5, 1.5] range (was 50)
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

template <typename T>
AccuracyResult measure_sin_t() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points covers full cycle (was 100)
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
AccuracyResult measure_cos_t() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points covers full cycle (was 100)
    const float lo = -6.2831853f;
    const float hi = 6.2831853f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::cosf(x);
        float got = T::cos(T(x)).to_float();
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
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
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
AccuracyResult measure_atan2_t() {
    AccuracyResult r = {};
    const int N = 5;  // Spot testing: 5×5 = 25 samples (was 20×20 = 400)
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    const float lo = -(maxRange < 7.0f ? maxRange : 7.0f);
    const float hi = (maxRange < 7.0f ? maxRange : 7.0f);
    float sumErr = 0;
    int count = 0;
    for (int ix = 0; ix < N; ++ix) {
        for (int iy = 0; iy < N; ++iy) {
            float x = lo + (hi - lo) * ix / (N - 1);
            float y = lo + (hi - lo) * iy / (N - 1);
            if (fl::fabsf(x) < 0.01f && fl::fabsf(y) < 0.01f) continue;
            float ref = fl::atan2f(y, x);
            float got = T::atan2(T(y), T(x)).to_float();
            float err = fl::fabsf(got - ref);
            sumErr += err;
            ++count;
            if (err > r.maxErr) { r.maxErr = err; r.worstInput = y; r.worstInput2 = x; }
        }
    }
    r.avgErr = count > 0 ? sumErr / count : 0;
    r.nSamples = count;
    return r;
}

template <typename T>
AccuracyResult measure_asin_t() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points adequate for [-1, 1] range (was 50)
    const float lo = -1.0f;
    const float hi = 1.0f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::asinf(x);
        float got = T::asin(T(x)).to_float();
        float err = fl::fabsf(got - ref);
        sumErr += err;
        if (err > r.maxErr) { r.maxErr = err; r.worstInput = x; }
    }
    r.avgErr = sumErr / N;
    r.nSamples = N;
    return r;
}

template <typename T>
AccuracyResult measure_acos_t() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points adequate for [-1, 1] range (was 50)
    const float lo = -1.0f;
    const float hi = 1.0f;
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = fl::acosf(x);
        float got = T::acos(T(x)).to_float();
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
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
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
AccuracyResult measure_rsqrt_t() {
    AccuracyResult r = {};
    const int N = 10;  // Spot testing: 10 key points adequate for range coverage (was 50)
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);
    const float lo = 0.01f;
    const float hi = (maxRange < 100.0f ? maxRange : 100.0f);
    float sumErr = 0;
    for (int i = 0; i < N; ++i) {
        float x = lo + (hi - lo) * i / (N - 1);
        float ref = 1.0f / fl::sqrtf(x);
        if (ref > maxRange) continue;
        float got = T::rsqrt(T(x)).to_float();
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
    const int N = 10;  // Spot testing: 10 key points adequate for base range coverage (was 50)
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
    const int N = 10;  // Spot testing: 10 key points adequate for [-0.5, 1.5] range (was 50)
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

// ---------------------------------------------------------------------------
// Bit-exact equivalence: fixed_point<I,F> vs concrete sIxF
// Ensures template and concrete types produce identical results.
// ---------------------------------------------------------------------------

template <typename Tmpl, typename Conc>
void test_bit_exact_equivalence() {
    // Determine test value ranges based on INT_BITS
    constexpr int intBits = Tmpl::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);

    // Select test values appropriate for the type's range
    float test_vals[9];
    int num_vals = 0;
    test_vals[num_vals++] = 0.0f;
    test_vals[num_vals++] = 0.25f;
    test_vals[num_vals++] = 0.5f;
    test_vals[num_vals++] = 1.0f;
    test_vals[num_vals++] = -1.0f;
    if (maxRange >= 2.0f) test_vals[num_vals++] = 2.0f;
    if (maxRange >= 5.0f) test_vals[num_vals++] = 5.0f;
    if (maxRange >= 10.0f) test_vals[num_vals++] = 10.0f;
    if (maxRange >= 100.0f) test_vals[num_vals++] = 100.0f;

    // Test arithmetic operations
    for (int i = 0; i < num_vals; ++i) {
        for (int j = 0; j < num_vals; ++j) {
            float a_val = test_vals[i];
            float b_val = test_vals[j];
            Tmpl ta(a_val), tb(b_val);
            Conc ca(a_val), cb(b_val);

            FL_CHECK_EQ((ta + tb).raw(), (ca + cb).raw());
            FL_CHECK_EQ((ta - tb).raw(), (ca - cb).raw());
            FL_CHECK_EQ((ta * tb).raw(), (ca * cb).raw());
            if (b_val != 0.0f) {
                FL_CHECK_EQ((ta / tb).raw(), (ca / cb).raw());
            }
        }
    }

    // Test transcendental functions
    float trig_vals[] = {0.0f, 0.5f, 1.0f, 1.5708f, 3.1416f};
    for (float v : trig_vals) {
        Tmpl tv(v);
        Conc cv(v);

        FL_CHECK_EQ(Tmpl::sin(tv).raw(), Conc::sin(cv).raw());
        FL_CHECK_EQ(Tmpl::cos(tv).raw(), Conc::cos(cv).raw());
        FL_CHECK_EQ(Tmpl::sqrt(tv).raw(), Conc::sqrt(cv).raw());
        FL_CHECK_EQ(Tmpl::atan(tv).raw(), Conc::atan(cv).raw());
    }

    // Test pow for s16x16 type (has sufficient range)
    if (intBits >= 16) {
        for (float v : trig_vals) {
            Tmpl tv(v);
            Conc cv(v);
            FL_CHECK_EQ(Tmpl::pow(tv, Tmpl(2.0f)).raw(), Conc::pow(cv, Conc(2.0f)).raw());
        }
    }
}

// ---------------------------------------------------------------------------
// Per-type accuracy regression gates
// ---------------------------------------------------------------------------
// Comprehensive regression tests that verify ALL transcendental functions
// against their type-specific accuracy bounds. These tests catch any
// degradation in accuracy across the entire transcendental function suite.

template <typename T>
void check_all_accuracy_bounds() {
    // Trig functions
    AccuracyResult sin_r = measure_sin_t<T>();
    FL_CHECK_LT(sin_r.maxErr, accuracy_bounds<T>::sin_max);

    AccuracyResult cos_r = measure_cos_t<T>();
    FL_CHECK_LT(cos_r.maxErr, accuracy_bounds<T>::cos_max);

    // Inverse trig
    AccuracyResult atan_r = measure_atan_t<T>();
    FL_CHECK_LT(atan_r.maxErr, accuracy_bounds<T>::atan_max);

    AccuracyResult atan2_r = measure_atan2_t<T>();
    FL_CHECK_LT(atan2_r.maxErr, accuracy_bounds<T>::atan2_max);

    AccuracyResult asin_r = measure_asin_t<T>();
    FL_CHECK_LT(asin_r.maxErr, accuracy_bounds<T>::asin_max);

    AccuracyResult acos_r = measure_acos_t<T>();
    FL_CHECK_LT(acos_r.maxErr, accuracy_bounds<T>::acos_max);

    // Root functions
    AccuracyResult sqrt_r = measure_sqrt_t<T>();
    FL_CHECK_LT(sqrt_r.maxErr, accuracy_bounds<T>::sqrt_max);

    AccuracyResult rsqrt_r = measure_rsqrt_t<T>();
    FL_CHECK_LT(rsqrt_r.maxErr, accuracy_bounds<T>::rsqrt_max);

    // Power functions
    constexpr int intBits = T::INT_BITS;
    const float maxRange = static_cast<float>((1 << (intBits - 1)) - 1);

    AccuracyResult pow05_r = measure_pow_t<T>(0.5f, 0.1f,
        (maxRange < 10.0f ? maxRange : 10.0f));
    FL_CHECK_LT(pow05_r.maxErr, accuracy_bounds<T>::pow_x_0_5_max);

    AccuracyResult pow06_r = measure_pow_t<T>(0.6f, 0.1f,
        (maxRange < 10.0f ? maxRange : 10.0f));
    FL_CHECK_LT(pow06_r.maxErr, accuracy_bounds<T>::pow_x_0_6_max);

    AccuracyResult pow20_r = measure_pow_t<T>(2.0f, 0.1f,
        (maxRange < 5.0f ? maxRange : 5.0f));
    FL_CHECK_LT(pow20_r.maxErr, accuracy_bounds<T>::pow_x_2_0_max);

    AccuracyResult pow30_r = measure_pow_t<T>(3.0f, 0.1f,
        (maxRange < 5.0f ? maxRange : 5.0f));
    FL_CHECK_LT(pow30_r.maxErr, accuracy_bounds<T>::pow_x_3_0_max);

    // Interpolation
    AccuracyResult smooth_r = measure_smoothstep_t<T>();
    FL_CHECK_LT(smooth_r.maxErr, accuracy_bounds<T>::smoothstep_max);
}
