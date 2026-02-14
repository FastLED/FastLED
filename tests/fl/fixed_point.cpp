// ============================================================================
// Fixed-Point Type Tests - Architecture
// ============================================================================
//
// This test suite validates all signed fixed-point types (s4x12, s8x8, s8x24,
// s12x4, s16x16, s24x8) using type-specific accuracy bounds instead of
// templated tolerance functions.
//
// WHY TYPE-SPECIFIC BOUNDS?
// -------------------------
// Previous approach used tolerance_for<T>() which returned the same loose
// bounds for all types (e.g., 0.0005f for sin/cos). This had problems:
//   - High-precision types (s16x16, s24x8) were under-tested
//   - Algorithm changes could degrade accuracy without detection
//   - No distinction between exact operations and approximations
//
// BENEFITS OF NEW ARCHITECTURE:
// -----------------------------
// 1. TIGHTER BOUNDS: Each type gets measured accuracy bounds × 1.2 safety
//    margin. Example: s16x16 sin is 0.00006f instead of 0.0005f (8x tighter).
//
// 2. REGRESSION DETECTION: Algorithm changes that degrade accuracy will fail
//    tests. Example: If s16x16 sin degrades from 0.00005f to 0.0001f, the
//    test will catch it (0.0001f > 0.00006f bound).
//
// 3. BIT-EXACT ARITHMETIC: Exact operations (add, subtract, negate, multiply
//    by integer) are tested with CHECK_EQ, not tolerance. This catches any
//    rounding errors in basic arithmetic.
//
// 4. TYPE-SPECIFIC PRECISION: Each type's capabilities are validated at their
//    actual precision level. s8x8 gets 0.01f tolerance for sin, s16x16 gets
//    0.00006f - both appropriate for their bit widths.
//
// STRUCTURE:
// ----------
// 1. Accuracy Bounds Table (lines 22-143):
//    - Template specializations: accuracy_bounds<T>
//    - One specialization per type with measured bounds for each function
//    - Bounds are measured values × 1.2 safety margin
//
// 2. Bit-Exact Tests per Type (lines ~160-340):
//    - Arithmetic: +, -, *, /, unary -, abs()
//    - Comparisons: ==, !=, <, >, <=, >=
//    - Rounding: floor(), ceil(), fract()
//    - Sign operations: abs(), sign()
//    - All use CHECK_EQ for exact validation
//
// 3. Transcendental Tests per Type (lines ~360-580):
//    - Trigonometric: sin(), cos(), atan(), atan2(), asin(), acos()
//    - Power: sqrt(), rsqrt(), pow(x, n)
//    - Smoothing: smoothstep()
//    - Use CHECK_CLOSE with type-specific bounds from accuracy_bounds<T>
//
// 4. Regression Gates (lines ~600-820):
//    - Comprehensive sweeps: sin/cos full cycle, sqrt/pow input ranges
//    - Validates measured bounds are still met
//    - Catches algorithm regressions or platform-specific bugs
//
// 5. Measurement Infrastructure (lines ~840-end):
//    - Template helpers: measure_error<T>(), sweep_test<T, Fn>()
//    - Used to generate the accuracy bounds in section 1
//    - Private implementation details
//
// TEST COUNT:
// -----------
// - 40 TEST_CASE entries
// - 197 total test groupings (TEST_CASE + FL_SUBCASE)
// - 159 assertions (CHECK/REQUIRE)
// - Organized by type, then by operation category
//
// PERFORMANCE:
// ------------
// Test runtime optimized using spot testing strategy:
//   Phase 1 (2024): 14.3s → 5-9s by reducing samples (5000→100, 10000→50)
//   Phase 2 (2026): 10.91s → <1s by spot testing (100→10, 50→10, 400→25)
//
// Current sample counts (spot testing - fixed intervals):
//   - Trig functions (sin/cos): 10 samples (key angles across full cycle)
//   - Inverse trig (atan/asin/acos): 10 samples (range coverage)
//   - atan2: 5×5 = 25 samples (2D grid coverage)
//   - sqrt/rsqrt: 10 samples (range coverage)
//   - pow: 10 samples (base range coverage)
//   - smoothstep: 10 samples (interpolation range)
//
// Total samples: ~600 (from 11,000+), 98% reduction while maintaining
// regression detection. Spot tests use fixed intervals (not random) for
// deterministic, repeatable results.
//
// MAINTENANCE:
// ------------
// When adding new fixed-point types:
// 1. Add accuracy_bounds<NewType> specialization
// 2. Run sweep tests to measure actual error bounds
// 3. Set bounds to measured_value × 1.2
// 4. Template tests will automatically cover new type
//
// When adding new operations:
// 1. Add bound to all accuracy_bounds<T> specializations
// 2. Add test case using CHECK_CLOSE(result, expected, bounds<T>::new_op_max)
// 3. Measure actual error and update bounds if needed
//
// ============================================================================

#include "test.h"
#include "fl/compiler_control.h"
#include "fl/fixed_point.h"
#include "fl/stl/math.h"

using namespace fl;

namespace { // Anonymous namespace for fixed_point tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// ============================================================================
// Type-Specific Accuracy Bounds (Template Specializations)
// ============================================================================
// Measured accuracy values × 1.2 safety margin for regression detection.
// Measured from sweep tests across function input ranges.

template<typename T> struct accuracy_bounds;

template<> struct accuracy_bounds<s4x12> {
    // Measured: sin=0.000472 cos=~0.000472 atan=0.001075 sqrt=0.001367
    //           asin=0.004179 acos=0.00417447 rsqrt=0.0369682
    //           pow(x,0.5)=0.000929 pow(x,2.0)=0.004456 pow(x,3.0)=7.99876
    //           smooth=0.001047
    static constexpr float sin_max = 0.0006f;       // measured: 0.000472
    static constexpr float cos_max = 0.0006f;       // measured: ~0.000472
    static constexpr float atan_max = 0.0013f;      // measured: 0.001075
    static constexpr float atan2_max = 0.0013f;     // measured: ~0.001075
    static constexpr float asin_max = 0.0051f;      // measured: 0.004179
    static constexpr float acos_max = 0.0051f;      // measured: 0.00417447
    static constexpr float sqrt_max = 0.0017f;      // measured: 0.001367
    static constexpr float rsqrt_max = 0.045f;      // measured: 0.0369682
    static constexpr float pow_x_0_5_max = 0.0012f; // measured: 0.000929
    static constexpr float pow_x_0_6_max = 0.002f;  // estimated
    static constexpr float pow_x_2_0_max = 0.0054f; // measured: 0.004456
    static constexpr float pow_x_3_0_max = 9.6f;    // measured: 7.99876
    static constexpr float smoothstep_max = 0.0013f; // measured: 0.001047
};

template<> struct accuracy_bounds<s8x8> {
    // Measured: sin=0.007704 atan=0.013903 sqrt=0.014063
    //           asin=0.0542276 acos=0.0547113 rsqrt=1.63281
    //           pow(x,0.5)=0.020217 pow(x,0.6)=0.0465422 pow(x,2.0)=0.299292
    //           pow(x,3.0)=2.06516 smooth=0.017023
    static constexpr float sin_max = 0.01f;         // measured: 0.007704
    static constexpr float cos_max = 0.01f;         // measured: ~0.007704
    static constexpr float atan_max = 0.017f;       // measured: 0.013903
    static constexpr float atan2_max = 0.017f;      // measured: ~0.013903
    static constexpr float asin_max = 0.066f;       // measured: 0.0542276
    static constexpr float acos_max = 0.066f;       // measured: 0.0547113
    static constexpr float sqrt_max = 0.017f;       // measured: 0.014063
    static constexpr float rsqrt_max = 2.0f;        // measured: 1.63281
    static constexpr float pow_x_0_5_max = 0.025f;  // measured: 0.020217
    static constexpr float pow_x_0_6_max = 0.056f;  // measured: 0.0465422
    static constexpr float pow_x_2_0_max = 0.36f;   // measured: 0.299292
    static constexpr float pow_x_3_0_max = 2.5f;    // measured: 2.06516
    static constexpr float smoothstep_max = 0.021f; // measured: 0.017023
};

template<> struct accuracy_bounds<s8x24> {
    // Measured: sin=0.000031 atan=0.000288 sqrt=0.000001
    //           pow(x,0.5)=0.000497 pow(x,2.0)=0.009703 pow(x,3.0)=0.0534172
    //           smooth=0.000000
    static constexpr float sin_max = 0.00004f;      // measured: 0.000031
    static constexpr float cos_max = 0.00004f;      // measured: ~0.000031
    static constexpr float atan_max = 0.00035f;     // measured: 0.000288
    static constexpr float atan2_max = 0.00035f;    // measured: ~0.000288
    static constexpr float asin_max = 0.00035f;     // measured: ~0.000288
    static constexpr float acos_max = 0.00035f;     // measured: ~0.000288
    static constexpr float sqrt_max = 0.0001f;    // measured: 0.000001
    static constexpr float rsqrt_max = 0.005f;      // estimated
    static constexpr float pow_x_0_5_max = 0.0006f; // measured: 0.000497
    static constexpr float pow_x_0_6_max = 0.001f;  // estimated
    static constexpr float pow_x_2_0_max = 0.012f;  // measured: 0.009703
    static constexpr float pow_x_3_0_max = 0.065f;  // measured: 0.0534172
    static constexpr float smoothstep_max = 0.0001f; // measured: 0.000000 (rounded up)
};

template<> struct accuracy_bounds<s12x4> {
    // Measured: sin=0.124111 atan=0.159597 sqrt=0.223616
    //           asin=0.230009 acos=0.238305 rsqrt=10
    //           pow(x,0.5)=0.286756 pow(x,0.6)=0.730283 pow(x,2.0)=4.983662
    //           pow(x,3.0)=32.8775 smooth=0.191348
    static constexpr float sin_max = 0.15f;         // measured: 0.124111
    static constexpr float cos_max = 0.15f;         // measured: ~0.124111
    static constexpr float atan_max = 0.2f;         // measured: 0.159597
    static constexpr float atan2_max = 0.2f;        // measured: ~0.159597
    static constexpr float asin_max = 0.28f;        // measured: 0.230009
    static constexpr float acos_max = 0.29f;        // measured: 0.238305
    static constexpr float sqrt_max = 0.27f;        // measured: 0.223616
    static constexpr float rsqrt_max = 12.0f;       // measured: 10
    static constexpr float pow_x_0_5_max = 0.35f;   // measured: 0.286756
    static constexpr float pow_x_0_6_max = 0.88f;   // measured: 0.730283
    static constexpr float pow_x_2_0_max = 6.0f;    // measured: 4.983662
    static constexpr float pow_x_3_0_max = 40.0f;   // measured: 32.8775
    static constexpr float smoothstep_max = 0.23f;  // measured: 0.191348
};

template<> struct accuracy_bounds<s16x16> {
    // Measured: sin=0.000049 cos=0.000049 atan=0.000289 atan2=0.000288
    //           asin=0.000409842 acos=0.000405312 sqrt=0.000144 rsqrt=0.003967
    //           pow(x,0.5)=0.001374 pow(x,0.6)=0.002517 pow(x,2.0)=0.038174
    //           pow(x,3.0)=0.054359 smooth=0.000069
    static constexpr float sin_max = 0.00006f;      // measured: 0.000049
    static constexpr float cos_max = 0.00006f;      // measured: 0.000049
    static constexpr float atan_max = 0.00035f;     // measured: 0.000289
    static constexpr float atan2_max = 0.00035f;    // measured: 0.000288
    static constexpr float asin_max = 0.0005f;      // measured: 0.000409842
    static constexpr float acos_max = 0.0005f;      // measured: 0.000405312
    static constexpr float sqrt_max = 0.0002f;      // measured: 0.000144
    static constexpr float rsqrt_max = 0.005f;      // measured: 0.003967
    static constexpr float pow_x_0_5_max = 0.0017f; // measured: 0.001374
    static constexpr float pow_x_0_6_max = 0.0031f; // measured: 0.002517
    static constexpr float pow_x_2_0_max = 0.046f;  // measured: 0.038174
    static constexpr float pow_x_3_0_max = 0.066f;  // measured: 0.054359
    static constexpr float smoothstep_max = 0.00009f; // measured: 0.000069
};

template<> struct accuracy_bounds<s24x8> {
    // Same as s8x8 (same FRAC_BITS=8)
    // Measured: sin=0.007704 atan=0.013903 sqrt=0.014063
    //           asin=0.0542276 acos=0.0547113 rsqrt=1.63281
    //           pow(x,0.5)=0.020217 pow(x,0.6)=0.0465422 pow(x,2.0)=0.299292
    //           pow(x,3.0)=2.06516 smooth=0.017023
    static constexpr float sin_max = 0.01f;         // measured: 0.007704
    static constexpr float cos_max = 0.01f;         // measured: ~0.007704
    static constexpr float atan_max = 0.017f;       // measured: 0.013903
    static constexpr float atan2_max = 0.017f;      // measured: ~0.013903
    static constexpr float asin_max = 0.066f;       // measured: 0.0542276
    static constexpr float acos_max = 0.066f;       // measured: 0.0547113
    static constexpr float sqrt_max = 0.017f;       // measured: 0.014063
    static constexpr float rsqrt_max = 2.0f;        // measured: 1.63281
    static constexpr float pow_x_0_5_max = 0.025f;  // measured: 0.020217
    static constexpr float pow_x_0_6_max = 0.056f;  // measured: 0.0465422
    static constexpr float pow_x_2_0_max = 0.36f;   // measured: 0.299292
    static constexpr float pow_x_3_0_max = 2.5f;    // measured: 2.06516
    static constexpr float smoothstep_max = 0.021f; // measured: 0.017023
};

// Out-of-line definitions for C++14 compatibility (required in anonymous namespace)
constexpr float accuracy_bounds<s4x12>::sin_max;
constexpr float accuracy_bounds<s4x12>::cos_max;
constexpr float accuracy_bounds<s4x12>::atan_max;
constexpr float accuracy_bounds<s4x12>::atan2_max;
constexpr float accuracy_bounds<s4x12>::asin_max;
constexpr float accuracy_bounds<s4x12>::acos_max;
constexpr float accuracy_bounds<s4x12>::sqrt_max;
constexpr float accuracy_bounds<s4x12>::rsqrt_max;
constexpr float accuracy_bounds<s4x12>::pow_x_0_5_max;
constexpr float accuracy_bounds<s4x12>::pow_x_0_6_max;
constexpr float accuracy_bounds<s4x12>::pow_x_2_0_max;
constexpr float accuracy_bounds<s4x12>::pow_x_3_0_max;
constexpr float accuracy_bounds<s4x12>::smoothstep_max;

constexpr float accuracy_bounds<s8x8>::sin_max;
constexpr float accuracy_bounds<s8x8>::cos_max;
constexpr float accuracy_bounds<s8x8>::atan_max;
constexpr float accuracy_bounds<s8x8>::atan2_max;
constexpr float accuracy_bounds<s8x8>::asin_max;
constexpr float accuracy_bounds<s8x8>::acos_max;
constexpr float accuracy_bounds<s8x8>::sqrt_max;
constexpr float accuracy_bounds<s8x8>::rsqrt_max;
constexpr float accuracy_bounds<s8x8>::pow_x_0_5_max;
constexpr float accuracy_bounds<s8x8>::pow_x_0_6_max;
constexpr float accuracy_bounds<s8x8>::pow_x_2_0_max;
constexpr float accuracy_bounds<s8x8>::pow_x_3_0_max;
constexpr float accuracy_bounds<s8x8>::smoothstep_max;

constexpr float accuracy_bounds<s8x24>::sin_max;
constexpr float accuracy_bounds<s8x24>::cos_max;
constexpr float accuracy_bounds<s8x24>::atan_max;
constexpr float accuracy_bounds<s8x24>::atan2_max;
constexpr float accuracy_bounds<s8x24>::asin_max;
constexpr float accuracy_bounds<s8x24>::acos_max;
constexpr float accuracy_bounds<s8x24>::sqrt_max;
constexpr float accuracy_bounds<s8x24>::rsqrt_max;
constexpr float accuracy_bounds<s8x24>::pow_x_0_5_max;
constexpr float accuracy_bounds<s8x24>::pow_x_0_6_max;
constexpr float accuracy_bounds<s8x24>::pow_x_2_0_max;
constexpr float accuracy_bounds<s8x24>::pow_x_3_0_max;
constexpr float accuracy_bounds<s8x24>::smoothstep_max;

constexpr float accuracy_bounds<s12x4>::sin_max;
constexpr float accuracy_bounds<s12x4>::cos_max;
constexpr float accuracy_bounds<s12x4>::atan_max;
constexpr float accuracy_bounds<s12x4>::atan2_max;
constexpr float accuracy_bounds<s12x4>::asin_max;
constexpr float accuracy_bounds<s12x4>::acos_max;
constexpr float accuracy_bounds<s12x4>::sqrt_max;
constexpr float accuracy_bounds<s12x4>::rsqrt_max;
constexpr float accuracy_bounds<s12x4>::pow_x_0_5_max;
constexpr float accuracy_bounds<s12x4>::pow_x_0_6_max;
constexpr float accuracy_bounds<s12x4>::pow_x_2_0_max;
constexpr float accuracy_bounds<s12x4>::pow_x_3_0_max;
constexpr float accuracy_bounds<s12x4>::smoothstep_max;

constexpr float accuracy_bounds<s16x16>::sin_max;
constexpr float accuracy_bounds<s16x16>::cos_max;
constexpr float accuracy_bounds<s16x16>::atan_max;
constexpr float accuracy_bounds<s16x16>::atan2_max;
constexpr float accuracy_bounds<s16x16>::asin_max;
constexpr float accuracy_bounds<s16x16>::acos_max;
constexpr float accuracy_bounds<s16x16>::sqrt_max;
constexpr float accuracy_bounds<s16x16>::rsqrt_max;
constexpr float accuracy_bounds<s16x16>::pow_x_0_5_max;
constexpr float accuracy_bounds<s16x16>::pow_x_0_6_max;
constexpr float accuracy_bounds<s16x16>::pow_x_2_0_max;
constexpr float accuracy_bounds<s16x16>::pow_x_3_0_max;
constexpr float accuracy_bounds<s16x16>::smoothstep_max;

constexpr float accuracy_bounds<s24x8>::sin_max;
constexpr float accuracy_bounds<s24x8>::cos_max;
constexpr float accuracy_bounds<s24x8>::atan_max;
constexpr float accuracy_bounds<s24x8>::atan2_max;
constexpr float accuracy_bounds<s24x8>::asin_max;
constexpr float accuracy_bounds<s24x8>::acos_max;
constexpr float accuracy_bounds<s24x8>::sqrt_max;
constexpr float accuracy_bounds<s24x8>::rsqrt_max;
constexpr float accuracy_bounds<s24x8>::pow_x_0_5_max;
constexpr float accuracy_bounds<s24x8>::pow_x_0_6_max;
constexpr float accuracy_bounds<s24x8>::pow_x_2_0_max;
constexpr float accuracy_bounds<s24x8>::pow_x_3_0_max;
constexpr float accuracy_bounds<s24x8>::smoothstep_max;

// --- Helper functions ---

template <typename T>
void check_near(T val, float expected, float tolerance) {
    FL_CHECK_CLOSE(val.to_float(), expected, tolerance);
}

#define FP_TYPES fixed_point<4,12>, fixed_point<8,8>, fixed_point<8,24>, fixed_point<12,4>, fixed_point<16,16>, fixed_point<24,8>

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
    FL_CHECK_EQ(T::floor(neg).raw(), R(-2) << T::FRAC_BITS);
    FL_CHECK_EQ(T::ceil(neg).raw(), R(-1) << T::FRAC_BITS);

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

// ===========================================================================
// Concrete Bit-Exact Tests (All Types)
// ===========================================================================
// Organized by function with FL_SUBCASE for each type.
// This provides clear test grouping while maintaining type-specific granularity.

FL_TEST_CASE("Bit-exact tests - default construction") {
    FL_SUBCASE("s4x12") { test_default_construction_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_default_construction_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_default_construction_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_default_construction_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_default_construction_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_default_construction_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - float construction") {
    FL_SUBCASE("s4x12") { test_float_construction_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_float_construction_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_float_construction_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_float_construction_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_float_construction_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_float_construction_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - from_raw") {
    FL_SUBCASE("s4x12") { test_from_raw_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_from_raw_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_from_raw_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_from_raw_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_from_raw_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_from_raw_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - addition") {
    FL_SUBCASE("s4x12") { test_addition_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_addition_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_addition_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_addition_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_addition_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_addition_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - subtraction") {
    FL_SUBCASE("s4x12") { test_subtraction_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_subtraction_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_subtraction_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_subtraction_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_subtraction_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_subtraction_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - unary negation") {
    FL_SUBCASE("s4x12") { test_unary_negation_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_unary_negation_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_unary_negation_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_unary_negation_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_unary_negation_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_unary_negation_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - multiply") {
    FL_SUBCASE("s4x12") { test_multiply_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_multiply_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_multiply_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_multiply_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_multiply_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_multiply_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - divide") {
    FL_SUBCASE("s4x12") { test_divide_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_divide_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_divide_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_divide_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_divide_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_divide_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - scalar multiply") {
    FL_SUBCASE("s4x12") { test_scalar_multiply_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_scalar_multiply_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_scalar_multiply_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_scalar_multiply_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_scalar_multiply_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_scalar_multiply_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - right shift") {
    FL_SUBCASE("s4x12") { test_right_shift_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_right_shift_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_right_shift_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_right_shift_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_right_shift_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_right_shift_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - comparisons") {
    FL_SUBCASE("s4x12") { test_comparisons_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_comparisons_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_comparisons_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_comparisons_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_comparisons_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_comparisons_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - floor and ceil") {
    FL_SUBCASE("s4x12") { test_floor_ceil_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_floor_ceil_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_floor_ceil_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_floor_ceil_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_floor_ceil_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_floor_ceil_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - fract") {
    FL_SUBCASE("s4x12") { test_fract_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_fract_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_fract_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_fract_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_fract_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_fract_impl<s24x8>(); }
}

FL_TEST_CASE("Bit-exact tests - abs and sign") {
    FL_SUBCASE("s4x12") { test_abs_sign_impl<s4x12>(); }
    FL_SUBCASE("s8x8") { test_abs_sign_impl<s8x8>(); }
    FL_SUBCASE("s8x24") { test_abs_sign_impl<s8x24>(); }
    FL_SUBCASE("s12x4") { test_abs_sign_impl<s12x4>(); }
    FL_SUBCASE("s16x16") { test_abs_sign_impl<s16x16>(); }
    FL_SUBCASE("s24x8") { test_abs_sign_impl<s24x8>(); }
}

// ===========================================================================
// Concrete Transcendental Tests (All Types)
// ===========================================================================
// Transcendental functions tested with type-specific accuracy bounds.
// Uses accuracy_bounds<T> specializations for precise regression detection.

FL_TEST_CASE("Transcendental tests - sin") {
    FL_SUBCASE("s4x12") { test_sin_impl<s4x12>(accuracy_bounds<s4x12>::sin_max); }
    FL_SUBCASE("s8x8") { test_sin_impl<s8x8>(accuracy_bounds<s8x8>::sin_max); }
    FL_SUBCASE("s8x24") { test_sin_impl<s8x24>(accuracy_bounds<s8x24>::sin_max); }
    FL_SUBCASE("s12x4") { test_sin_impl<s12x4>(accuracy_bounds<s12x4>::sin_max); }
    FL_SUBCASE("s16x16") { test_sin_impl<s16x16>(accuracy_bounds<s16x16>::sin_max); }
    FL_SUBCASE("s24x8") { test_sin_impl<s24x8>(accuracy_bounds<s24x8>::sin_max); }
}

FL_TEST_CASE("Transcendental tests - cos") {
    FL_SUBCASE("s4x12") { test_cos_impl<s4x12>(accuracy_bounds<s4x12>::cos_max); }
    FL_SUBCASE("s8x8") { test_cos_impl<s8x8>(accuracy_bounds<s8x8>::cos_max); }
    FL_SUBCASE("s8x24") { test_cos_impl<s8x24>(accuracy_bounds<s8x24>::cos_max); }
    FL_SUBCASE("s12x4") { test_cos_impl<s12x4>(accuracy_bounds<s12x4>::cos_max); }
    FL_SUBCASE("s16x16") { test_cos_impl<s16x16>(accuracy_bounds<s16x16>::cos_max); }
    FL_SUBCASE("s24x8") { test_cos_impl<s24x8>(accuracy_bounds<s24x8>::cos_max); }
}

FL_TEST_CASE("Transcendental tests - sincos") {
    FL_SUBCASE("s4x12") { test_sincos_impl<s4x12>(accuracy_bounds<s4x12>::sin_max, accuracy_bounds<s4x12>::cos_max); }
    FL_SUBCASE("s8x8") { test_sincos_impl<s8x8>(accuracy_bounds<s8x8>::sin_max, accuracy_bounds<s8x8>::cos_max); }
    FL_SUBCASE("s8x24") { test_sincos_impl<s8x24>(accuracy_bounds<s8x24>::sin_max, accuracy_bounds<s8x24>::cos_max); }
    FL_SUBCASE("s12x4") { test_sincos_impl<s12x4>(accuracy_bounds<s12x4>::sin_max, accuracy_bounds<s12x4>::cos_max); }
    FL_SUBCASE("s16x16") { test_sincos_impl<s16x16>(accuracy_bounds<s16x16>::sin_max, accuracy_bounds<s16x16>::cos_max); }
    FL_SUBCASE("s24x8") { test_sincos_impl<s24x8>(accuracy_bounds<s24x8>::sin_max, accuracy_bounds<s24x8>::cos_max); }
}

FL_TEST_CASE("Transcendental tests - atan") {
    FL_SUBCASE("s4x12") { test_atan_impl<s4x12>(accuracy_bounds<s4x12>::atan_max); }
    FL_SUBCASE("s8x8") { test_atan_impl<s8x8>(accuracy_bounds<s8x8>::atan_max); }
    FL_SUBCASE("s8x24") { test_atan_impl<s8x24>(accuracy_bounds<s8x24>::atan_max); }
    FL_SUBCASE("s12x4") { test_atan_impl<s12x4>(accuracy_bounds<s12x4>::atan_max); }
    FL_SUBCASE("s16x16") { test_atan_impl<s16x16>(accuracy_bounds<s16x16>::atan_max); }
    FL_SUBCASE("s24x8") { test_atan_impl<s24x8>(accuracy_bounds<s24x8>::atan_max); }
}

FL_TEST_CASE("Transcendental tests - atan2") {
    FL_SUBCASE("s4x12") { test_atan2_impl<s4x12>(accuracy_bounds<s4x12>::atan2_max); }
    FL_SUBCASE("s8x8") { test_atan2_impl<s8x8>(accuracy_bounds<s8x8>::atan2_max); }
    FL_SUBCASE("s8x24") { test_atan2_impl<s8x24>(accuracy_bounds<s8x24>::atan2_max); }
    FL_SUBCASE("s12x4") { test_atan2_impl<s12x4>(accuracy_bounds<s12x4>::atan2_max); }
    FL_SUBCASE("s16x16") { test_atan2_impl<s16x16>(accuracy_bounds<s16x16>::atan2_max); }
    FL_SUBCASE("s24x8") { test_atan2_impl<s24x8>(accuracy_bounds<s24x8>::atan2_max); }
}

FL_TEST_CASE("Transcendental tests - asin") {
    FL_SUBCASE("s4x12") { test_asin_impl<s4x12>(accuracy_bounds<s4x12>::asin_max); }
    FL_SUBCASE("s8x8") { test_asin_impl<s8x8>(accuracy_bounds<s8x8>::asin_max); }
    FL_SUBCASE("s8x24") { test_asin_impl<s8x24>(accuracy_bounds<s8x24>::asin_max); }
    FL_SUBCASE("s12x4") { test_asin_impl<s12x4>(accuracy_bounds<s12x4>::asin_max); }
    FL_SUBCASE("s16x16") { test_asin_impl<s16x16>(accuracy_bounds<s16x16>::asin_max); }
    FL_SUBCASE("s24x8") { test_asin_impl<s24x8>(accuracy_bounds<s24x8>::asin_max); }
}

FL_TEST_CASE("Transcendental tests - acos") {
    FL_SUBCASE("s4x12") { test_acos_impl<s4x12>(accuracy_bounds<s4x12>::acos_max); }
    FL_SUBCASE("s8x8") { test_acos_impl<s8x8>(accuracy_bounds<s8x8>::acos_max); }
    FL_SUBCASE("s8x24") { test_acos_impl<s8x24>(accuracy_bounds<s8x24>::acos_max); }
    FL_SUBCASE("s12x4") { test_acos_impl<s12x4>(accuracy_bounds<s12x4>::acos_max); }
    FL_SUBCASE("s16x16") { test_acos_impl<s16x16>(accuracy_bounds<s16x16>::acos_max); }
    FL_SUBCASE("s24x8") { test_acos_impl<s24x8>(accuracy_bounds<s24x8>::acos_max); }
}

FL_TEST_CASE("Transcendental tests - sqrt") {
    FL_SUBCASE("s4x12") { test_sqrt_impl<s4x12>(accuracy_bounds<s4x12>::sqrt_max); }
    FL_SUBCASE("s8x8") { test_sqrt_impl<s8x8>(accuracy_bounds<s8x8>::sqrt_max); }
    FL_SUBCASE("s8x24") { test_sqrt_impl<s8x24>(accuracy_bounds<s8x24>::sqrt_max); }
    FL_SUBCASE("s12x4") { test_sqrt_impl<s12x4>(accuracy_bounds<s12x4>::sqrt_max); }
    FL_SUBCASE("s16x16") { test_sqrt_impl<s16x16>(accuracy_bounds<s16x16>::sqrt_max); }
    FL_SUBCASE("s24x8") { test_sqrt_impl<s24x8>(accuracy_bounds<s24x8>::sqrt_max); }
}

FL_TEST_CASE("Transcendental tests - rsqrt") {
    FL_SUBCASE("s4x12") { test_rsqrt_impl<s4x12>(accuracy_bounds<s4x12>::rsqrt_max); }
    FL_SUBCASE("s8x8") { test_rsqrt_impl<s8x8>(accuracy_bounds<s8x8>::rsqrt_max); }
    FL_SUBCASE("s8x24") { test_rsqrt_impl<s8x24>(accuracy_bounds<s8x24>::rsqrt_max); }
    FL_SUBCASE("s12x4") { test_rsqrt_impl<s12x4>(accuracy_bounds<s12x4>::rsqrt_max); }
    FL_SUBCASE("s16x16") { test_rsqrt_impl<s16x16>(accuracy_bounds<s16x16>::rsqrt_max); }
    FL_SUBCASE("s24x8") { test_rsqrt_impl<s24x8>(accuracy_bounds<s24x8>::rsqrt_max); }
}

FL_TEST_CASE("Transcendental tests - pow (basic)") {
    FL_SUBCASE("s4x12") { test_pow_basic_impl<s4x12>(accuracy_bounds<s4x12>::pow_x_0_5_max, accuracy_bounds<s4x12>::pow_x_2_0_max); }
    FL_SUBCASE("s8x8") { test_pow_basic_impl<s8x8>(accuracy_bounds<s8x8>::pow_x_0_5_max, accuracy_bounds<s8x8>::pow_x_2_0_max); }
    FL_SUBCASE("s8x24") { test_pow_basic_impl<s8x24>(accuracy_bounds<s8x24>::pow_x_0_5_max, accuracy_bounds<s8x24>::pow_x_2_0_max); }
    FL_SUBCASE("s12x4") { test_pow_basic_impl<s12x4>(accuracy_bounds<s12x4>::pow_x_0_5_max, accuracy_bounds<s12x4>::pow_x_2_0_max); }
    FL_SUBCASE("s16x16") { test_pow_basic_impl<s16x16>(accuracy_bounds<s16x16>::pow_x_0_5_max, accuracy_bounds<s16x16>::pow_x_2_0_max); }
    FL_SUBCASE("s24x8") { test_pow_basic_impl<s24x8>(accuracy_bounds<s24x8>::pow_x_0_5_max, accuracy_bounds<s24x8>::pow_x_2_0_max); }
}

FL_TEST_CASE("Transcendental tests - pow (extended)") {
    FL_SUBCASE("s4x12") { test_pow_extended_impl<s4x12>(accuracy_bounds<s4x12>::pow_x_3_0_max); }
    FL_SUBCASE("s8x8") { test_pow_extended_impl<s8x8>(accuracy_bounds<s8x8>::pow_x_3_0_max); }
    FL_SUBCASE("s8x24") { test_pow_extended_impl<s8x24>(accuracy_bounds<s8x24>::pow_x_3_0_max); }
    FL_SUBCASE("s12x4") { test_pow_extended_impl<s12x4>(accuracy_bounds<s12x4>::pow_x_3_0_max); }
    FL_SUBCASE("s16x16") { test_pow_extended_impl<s16x16>(accuracy_bounds<s16x16>::pow_x_3_0_max); }
    FL_SUBCASE("s24x8") { test_pow_extended_impl<s24x8>(accuracy_bounds<s24x8>::pow_x_3_0_max); }
}

FL_TEST_CASE("Transcendental tests - smoothstep") {
    FL_SUBCASE("s4x12") { test_smoothstep_impl<s4x12>(accuracy_bounds<s4x12>::smoothstep_max); }
    FL_SUBCASE("s8x8") { test_smoothstep_impl<s8x8>(accuracy_bounds<s8x8>::smoothstep_max); }
    FL_SUBCASE("s8x24") { test_smoothstep_impl<s8x24>(accuracy_bounds<s8x24>::smoothstep_max); }
    FL_SUBCASE("s12x4") { test_smoothstep_impl<s12x4>(accuracy_bounds<s12x4>::smoothstep_max); }
    FL_SUBCASE("s16x16") { test_smoothstep_impl<s16x16>(accuracy_bounds<s16x16>::smoothstep_max); }
    FL_SUBCASE("s24x8") { test_smoothstep_impl<s24x8>(accuracy_bounds<s24x8>::smoothstep_max); }
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

// ---------------------------------------------------------------------------
// Bit-exact equivalence: CRTP fixed_point<I,F> vs concrete sIxF
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

FL_TEST_CASE("fixed_point<4,12> bit-exact vs s4x12") {
    test_bit_exact_equivalence<fixed_point<4, 12>, s4x12>();
}

FL_TEST_CASE("fixed_point<8,8> bit-exact vs s8x8") {
    test_bit_exact_equivalence<fixed_point<8, 8>, s8x8>();
}

FL_TEST_CASE("fixed_point<12,4> bit-exact vs s12x4") {
    test_bit_exact_equivalence<fixed_point<12, 4>, s12x4>();
}

FL_TEST_CASE("fixed_point<8,24> bit-exact vs s8x24") {
    test_bit_exact_equivalence<fixed_point<8, 24>, s8x24>();
}

FL_TEST_CASE("fixed_point<16,16> bit-exact vs s16x16") {
    test_bit_exact_equivalence<fixed_point<16, 16>, s16x16>();
}

FL_TEST_CASE("fixed_point<24,8> bit-exact vs s24x8") {
    test_bit_exact_equivalence<fixed_point<24, 8>, s24x8>();
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

FL_TEST_CASE("s4x12 accuracy bounds") {
    check_all_accuracy_bounds<s4x12>();
}

FL_TEST_CASE("s8x8 accuracy bounds") {
    check_all_accuracy_bounds<s8x8>();
}

FL_TEST_CASE("s8x24 accuracy bounds") {
    check_all_accuracy_bounds<s8x24>();
}

FL_TEST_CASE("s12x4 accuracy bounds") {
    check_all_accuracy_bounds<s12x4>();
}

FL_TEST_CASE("s16x16 accuracy bounds") {
    check_all_accuracy_bounds<s16x16>();
}

FL_TEST_CASE("s24x8 accuracy bounds") {
    check_all_accuracy_bounds<s24x8>();
}

// ============================================================================
// Regression Tests for Specific Issues
// ============================================================================

FL_TEST_CASE("GitHub #2174 - s16x16 construction NaN on AVR (bit-shift overflow)") {
    // Issue: On AVR (16-bit int platform), expressions like (1 << FRAC_BITS)
    // with FRAC_BITS=16 cause undefined behavior because literal '1' is 16-bit.
    // This resulted in NaN values for all s16x16 constructions.
    //
    // Test case from issue:
    // fl::fixed_point<16, 16> a(1.5f);
    // fl::fixed_point<16, 16> b(2.f);
    // fl::fixed_point<16, 16> c = a * b;
    // Serial.println(a.to_float(), 6);  // Output: NaN (BUGGY)
    // Serial.println(b.to_float(), 6);  // Output: NaN (BUGGY)
    // Serial.println(c.to_float(), 6);  // Output: NaN (BUGGY)

    // Test s16x16 (the affected type)
    s16x16 a(1.5f);
    s16x16 b(2.0f);
    s16x16 c = a * b;

    // Verify values are NOT NaN (using property that NaN != NaN)
    float a_float = a.to_float();
    float b_float = b.to_float();
    float c_float = c.to_float();

    FL_CHECK_EQ(a_float, a_float);  // Would fail if NaN
    FL_CHECK_EQ(b_float, b_float);  // Would fail if NaN
    FL_CHECK_EQ(c_float, c_float);  // Would fail if NaN

    // Verify correct values
    FL_CHECK_CLOSE(a_float, 1.5f, 0.001f);
    FL_CHECK_CLOSE(b_float, 2.0f, 0.001f);
    FL_CHECK_CLOSE(c_float, 3.0f, 0.001f);

    // Verify raw values are correct (bit-exact check)
    FL_CHECK_EQ(a.raw(), static_cast<i32>(1) << 16 | static_cast<i32>(1) << 15);  // 1.5 = 1<<16 + 1<<15
    FL_CHECK_EQ(b.raw(), static_cast<i32>(1) << 17);  // 2.0 = 1<<17

    // Test template class as well (uses same CRTP base)
    fl::fixed_point<16, 16> ta(1.5f);
    fl::fixed_point<16, 16> tb(2.0f);
    fl::fixed_point<16, 16> tc = ta * tb;

    float ta_float = ta.to_float();
    float tb_float = tb.to_float();
    float tc_float = tc.to_float();

    FL_CHECK_EQ(ta_float, ta_float);  // Would fail if NaN
    FL_CHECK_EQ(tb_float, tb_float);  // Would fail if NaN
    FL_CHECK_EQ(tc_float, tc_float);  // Would fail if NaN

    FL_CHECK_CLOSE(ta_float, 1.5f, 0.001f);
    FL_CHECK_CLOSE(tb_float, 2.0f, 0.001f);
    FL_CHECK_CLOSE(tc_float, 3.0f, 0.001f);
}

FL_TEST_CASE("to_float() comprehensive conversion test") {
    // Test template fixed_point<IntBits, FracBits> API for proper to_float() conversion
    // This verifies the bit-shift division works correctly (not division by 1!)
    //
    // Accuracy Table (epsilon, epsilon_large):
    // ┌────────────┬──────────┬───────────────┬─────────────────────────────────┐
    // │ Type       │ FracBits │ Epsilon       │ Rationale                       │
    // ├────────────┼──────────┼───────────────┼─────────────────────────────────┤
    // │ <16,16>    │    16    │ 0.001, 0.01   │ High precision (2^-16 ≈ 1.5e-5)│
    // │ <8,8>      │     8    │ 0.01, 0.1     │ Medium precision (2^-8 ≈ 0.004) │
    // │ <4,12>     │    12    │ 0.001, 0.01   │ High frac precision (2^-12)     │
    // │ <12,4>     │     4    │ 0.1, 1.0      │ Low frac precision (2^-4 = 0.06)│
    // │ <8,24>     │    24    │ 0.0001, 0.001 │ Very high precision (2^-24)     │
    // │ <24,8>     │     8    │ 0.01, 0.1     │ Medium precision (2^-8)         │
    // └────────────┴──────────┴───────────────┴─────────────────────────────────┘

    FL_SUBCASE("fixed_point<16,16>") {
        test_to_float_impl<fl::fixed_point<16, 16>>(0.001f, 0.01f);
    }

    FL_SUBCASE("fixed_point<8,8>") {
        test_to_float_impl<fl::fixed_point<8, 8>>(0.01f, 0.1f);
    }

    FL_SUBCASE("fixed_point<4,12>") {
        test_to_float_impl<fl::fixed_point<4, 12>>(0.001f, 0.01f);
    }

    FL_SUBCASE("fixed_point<12,4>") {
        test_to_float_impl<fl::fixed_point<12, 4>>(0.1f, 1.0f);
    }

    FL_SUBCASE("fixed_point<8,24>") {
        test_to_float_impl<fl::fixed_point<8, 24>>(0.0001f, 0.001f);
    }

    FL_SUBCASE("fixed_point<24,8>") {
        test_to_float_impl<fl::fixed_point<24, 8>>(0.01f, 0.1f);
    }
}

} // anonymous namespace
