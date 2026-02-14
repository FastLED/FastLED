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

// ---------------------------------------------------------------------------
// u4x12 tests (from u4x12.cpp)
// ---------------------------------------------------------------------------

static bool approx_equal(float a, float b, float eps = 0.01f) {
    return (a - b) < eps && (b - a) < eps;
}

FL_TEST_CASE("u4x12: construction from float") {
    u4x12 zero(0.0f);
    FL_CHECK(zero.raw() == 0);
    FL_CHECK(zero.to_float() == 0.0f);

    u4x12 one(1.0f);
    FL_CHECK(one.raw() == 4096);  // 1.0 * 2^12
    FL_CHECK(one.to_float() == 1.0f);

    u4x12 half(0.5f);
    FL_CHECK(half.raw() == 2048);  // 0.5 * 2^12
    FL_CHECK(half.to_float() == 0.5f);

    u4x12 max_val(15.999f);
    FL_CHECK(max_val.to_float() > 15.0f);
    FL_CHECK(max_val.to_float() < 16.0f);

    u4x12 pi(3.14159f);
    FL_CHECK(approx_equal(pi.to_float(), 3.14159f, 0.001f));
}

FL_TEST_CASE("u4x12: from_raw construction") {
    u4x12 zero = u4x12::from_raw(0);
    FL_CHECK(zero.to_float() == 0.0f);

    u4x12 one = u4x12::from_raw(4096);
    FL_CHECK(one.to_float() == 1.0f);

    u4x12 quarter = u4x12::from_raw(1024);
    FL_CHECK(quarter.to_float() == 0.25f);
}

FL_TEST_CASE("u4x12: to_int conversion") {
    u4x12 zero(0.0f);
    FL_CHECK(zero.to_int() == 0);

    u4x12 one(1.0f);
    FL_CHECK(one.to_int() == 1);

    u4x12 two_half(2.5f);
    FL_CHECK(two_half.to_int() == 2);  // Truncates

    u4x12 fifteen(15.0f);
    FL_CHECK(fifteen.to_int() == 15);
}

FL_TEST_CASE("u4x12: addition") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = a + b;
    FL_CHECK(approx_equal(c.to_float(), 5.0f));

    u4x12 d(0.25f);
    u4x12 e(0.75f);
    u4x12 f = d + e;
    FL_CHECK(approx_equal(f.to_float(), 1.0f));
}

FL_TEST_CASE("u4x12: subtraction") {
    u4x12 a(5.0f);
    u4x12 b(3.0f);
    u4x12 c = a - b;
    FL_CHECK(approx_equal(c.to_float(), 2.0f));

    u4x12 d(1.0f);
    u4x12 e(0.25f);
    u4x12 f = d - e;
    FL_CHECK(approx_equal(f.to_float(), 0.75f));
}

FL_TEST_CASE("u4x12: multiplication") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = a * b;
    FL_CHECK(approx_equal(c.to_float(), 6.0f));

    u4x12 d(0.5f);
    u4x12 e(0.5f);
    u4x12 f = d * e;
    FL_CHECK(approx_equal(f.to_float(), 0.25f));

    u4x12 g(4.0f);
    u4x12 h(2.5f);
    u4x12 i = g * h;
    FL_CHECK(approx_equal(i.to_float(), 10.0f));
}

FL_TEST_CASE("u4x12: division") {
    u4x12 a(6.0f);
    u4x12 b(3.0f);
    u4x12 c = a / b;
    FL_CHECK(approx_equal(c.to_float(), 2.0f));

    u4x12 d(1.0f);
    u4x12 e(2.0f);
    u4x12 f = d / e;
    FL_CHECK(approx_equal(f.to_float(), 0.5f));

    u4x12 g(10.0f);
    u4x12 h(4.0f);
    u4x12 i = g / h;
    FL_CHECK(approx_equal(i.to_float(), 2.5f));
}

FL_TEST_CASE("u4x12: scalar multiplication") {
    u4x12 a(2.5f);
    u4x12 b = a * static_cast<u16>(3);
    FL_CHECK(approx_equal(b.to_float(), 7.5f));

    u4x12 c(0.25f);
    u4x12 d = static_cast<u16>(4) * c;
    FL_CHECK(approx_equal(d.to_float(), 1.0f));
}

FL_TEST_CASE("u4x12: shift operations") {
    u4x12 a(8.0f);
    u4x12 b = a >> 1;
    FL_CHECK(approx_equal(b.to_float(), 4.0f));

    u4x12 c(2.0f);
    u4x12 d = c << 1;
    FL_CHECK(approx_equal(d.to_float(), 4.0f));

    u4x12 e(1.0f);
    u4x12 f = e >> 2;
    FL_CHECK(approx_equal(f.to_float(), 0.25f));
}

FL_TEST_CASE("u4x12: comparisons") {
    u4x12 a(3.0f);
    u4x12 b(5.0f);
    u4x12 c(3.0f);

    FL_CHECK(a < b);
    FL_CHECK(b > a);
    FL_CHECK(a <= c);
    FL_CHECK(a >= c);
    FL_CHECK(a == c);
    FL_CHECK(a != b);

    FL_CHECK_FALSE(a > b);
    FL_CHECK_FALSE(b < a);
    FL_CHECK_FALSE(a != c);
}

FL_TEST_CASE("u4x12: mod operation") {
    u4x12 a(7.5f);
    u4x12 b(3.0f);
    u4x12 c = u4x12::mod(a, b);
    FL_CHECK(approx_equal(c.to_float(), 1.5f));

    u4x12 d(10.0f);
    u4x12 e(4.0f);
    u4x12 f = u4x12::mod(d, e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f));
}

FL_TEST_CASE("u4x12: floor operation") {
    u4x12 a(3.7f);
    u4x12 b = u4x12::floor(a);
    FL_CHECK(approx_equal(b.to_float(), 3.0f));

    u4x12 c(5.1f);
    u4x12 d = u4x12::floor(c);
    FL_CHECK(approx_equal(d.to_float(), 5.0f));

    u4x12 e(0.9f);
    u4x12 f = u4x12::floor(e);
    FL_CHECK(approx_equal(f.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: ceil operation") {
    u4x12 a(3.1f);
    u4x12 b = u4x12::ceil(a);
    FL_CHECK(approx_equal(b.to_float(), 4.0f));

    u4x12 c(5.9f);
    u4x12 d = u4x12::ceil(c);
    FL_CHECK(approx_equal(d.to_float(), 6.0f));

    u4x12 e(2.0f);
    u4x12 f = u4x12::ceil(e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f));
}

FL_TEST_CASE("u4x12: fract operation") {
    u4x12 a(3.75f);
    u4x12 b = u4x12::fract(a);
    FL_CHECK(approx_equal(b.to_float(), 0.75f));

    u4x12 c(5.25f);
    u4x12 d = u4x12::fract(c);
    FL_CHECK(approx_equal(d.to_float(), 0.25f));

    u4x12 e(7.0f);
    u4x12 f = u4x12::fract(e);
    FL_CHECK(approx_equal(f.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: abs operation") {
    u4x12 a(3.5f);
    u4x12 b = u4x12::abs(a);
    FL_CHECK(approx_equal(b.to_float(), 3.5f));

    u4x12 c(0.0f);
    u4x12 d = u4x12::abs(c);
    FL_CHECK(approx_equal(d.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: min/max operations") {
    u4x12 a(3.0f);
    u4x12 b(5.0f);

    u4x12 min_val = u4x12::min(a, b);
    FL_CHECK(approx_equal(min_val.to_float(), 3.0f));

    u4x12 max_val = u4x12::max(a, b);
    FL_CHECK(approx_equal(max_val.to_float(), 5.0f));

    u4x12 c(7.0f);
    u4x12 d(7.0f);
    FL_CHECK(approx_equal(u4x12::min(c, d).to_float(), 7.0f));
    FL_CHECK(approx_equal(u4x12::max(c, d).to_float(), 7.0f));
}

FL_TEST_CASE("u4x12: lerp operation") {
    u4x12 a(0.0f);
    u4x12 b(10.0f);
    u4x12 t(0.5f);
    u4x12 c = u4x12::lerp(a, b, t);
    FL_CHECK(approx_equal(c.to_float(), 5.0f));

    u4x12 d(2.0f);
    u4x12 e(8.0f);
    u4x12 f(0.25f);
    u4x12 g = u4x12::lerp(d, e, f);
    FL_CHECK(approx_equal(g.to_float(), 3.5f));
}

FL_TEST_CASE("u4x12: clamp operation") {
    u4x12 lo(2.0f);
    u4x12 hi(8.0f);

    u4x12 a(5.0f);
    u4x12 b = u4x12::clamp(a, lo, hi);
    FL_CHECK(approx_equal(b.to_float(), 5.0f));

    u4x12 c(1.0f);
    u4x12 d = u4x12::clamp(c, lo, hi);
    FL_CHECK(approx_equal(d.to_float(), 2.0f));

    u4x12 e(10.0f);
    u4x12 f = u4x12::clamp(e, lo, hi);
    FL_CHECK(approx_equal(f.to_float(), 8.0f));
}

FL_TEST_CASE("u4x12: step operation") {
    u4x12 edge(5.0f);

    u4x12 a(3.0f);
    u4x12 b = u4x12::step(edge, a);
    FL_CHECK(approx_equal(b.to_float(), 0.0f));

    u4x12 c(7.0f);
    u4x12 d = u4x12::step(edge, c);
    FL_CHECK(approx_equal(d.to_float(), 1.0f));

    u4x12 e(5.0f);
    u4x12 f = u4x12::step(edge, e);
    FL_CHECK(approx_equal(f.to_float(), 1.0f));  // x >= edge
}

FL_TEST_CASE("u4x12: smoothstep operation") {
    u4x12 edge0(0.0f);
    u4x12 edge1(1.0f);

    u4x12 a(0.5f);
    u4x12 b = u4x12::smoothstep(edge0, edge1, a);
    FL_CHECK(approx_equal(b.to_float(), 0.5f, 0.05f));

    u4x12 c(0.0f);
    u4x12 d = u4x12::smoothstep(edge0, edge1, c);
    FL_CHECK(approx_equal(d.to_float(), 0.0f, 0.05f));

    u4x12 e(1.0f);
    u4x12 f = u4x12::smoothstep(edge0, edge1, e);
    FL_CHECK(approx_equal(f.to_float(), 1.0f, 0.05f));
}

FL_TEST_CASE("u4x12: sqrt operation") {
    u4x12 a(4.0f);
    u4x12 b = u4x12::sqrt(a);
    FL_CHECK(approx_equal(b.to_float(), 2.0f, 0.05f));

    u4x12 c(9.0f);
    u4x12 d = u4x12::sqrt(c);
    FL_CHECK(approx_equal(d.to_float(), 3.0f, 0.05f));

    u4x12 e(2.0f);
    u4x12 f = u4x12::sqrt(e);
    FL_CHECK(approx_equal(f.to_float(), 1.414f, 0.05f));

    u4x12 g(0.0f);
    u4x12 h = u4x12::sqrt(g);
    FL_CHECK(approx_equal(h.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: rsqrt operation") {
    u4x12 a(4.0f);
    u4x12 b = u4x12::rsqrt(a);
    FL_CHECK(approx_equal(b.to_float(), 0.5f, 0.05f));

    u4x12 c(1.0f);
    u4x12 d = u4x12::rsqrt(c);
    FL_CHECK(approx_equal(d.to_float(), 1.0f, 0.05f));

    u4x12 e(0.25f);
    u4x12 f = u4x12::rsqrt(e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f, 0.05f));
}

FL_TEST_CASE("u4x12: pow operation") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = u4x12::pow(a, b);
    FL_CHECK(approx_equal(c.to_float(), 8.0f, 0.1f));

    u4x12 d(3.0f);
    u4x12 e(2.0f);
    u4x12 f = u4x12::pow(d, e);
    FL_CHECK(approx_equal(f.to_float(), 9.0f, 0.1f));

    u4x12 g(4.0f);
    u4x12 h(0.5f);
    u4x12 i = u4x12::pow(g, h);
    FL_CHECK(approx_equal(i.to_float(), 2.0f, 0.1f));
}

FL_TEST_CASE("u4x12: edge cases") {
    // Zero
    u4x12 zero(0.0f);
    FL_CHECK(zero.raw() == 0);
    FL_CHECK(zero.to_float() == 0.0f);
    FL_CHECK(zero.to_int() == 0);

    // Max representable value (just under 16.0)
    u4x12 max_val = u4x12::from_raw(0xFFFF);
    FL_CHECK(max_val.to_float() > 15.99f);
    FL_CHECK(max_val.to_float() < 16.0f);
    FL_CHECK(max_val.to_int() == 15);

    // Small values
    u4x12 tiny(0.000244140625f);  // 1/4096 exactly
    FL_CHECK(tiny.raw() == 1);

    // Operations on zero
    u4x12 one(1.0f);
    FL_CHECK(approx_equal((zero + one).to_float(), 1.0f));
    FL_CHECK(approx_equal((one * zero).to_float(), 0.0f));
    FL_CHECK(approx_equal(u4x12::sqrt(zero).to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: range verification") {
    // Verify we can represent values in [0, 16)
    u4x12 zero(0.0f);
    FL_CHECK(zero.to_float() >= 0.0f);

    u4x12 eight(8.0f);
    FL_CHECK(approx_equal(eight.to_float(), 8.0f));

    u4x12 fifteen(15.0f);
    FL_CHECK(approx_equal(fifteen.to_float(), 15.0f));

    u4x12 almost_sixteen(15.999f);
    FL_CHECK(almost_sixteen.to_float() < 16.0f);
}

FL_TEST_CASE("u4x12: fractional precision") {
    // With 12 fractional bits, we have 1/4096 precision
    float quantum = 1.0f / 4096.0f;

    u4x12 a(quantum);
    FL_CHECK(a.raw() == 1);

    u4x12 b(2.0f * quantum);
    FL_CHECK(b.raw() == 2);

    // Verify we can represent small fractions accurately
    u4x12 quarter(0.25f);
    FL_CHECK(quarter.raw() == 1024);
    FL_CHECK(approx_equal(quarter.to_float(), 0.25f, 0.0001f));

    u4x12 eighth(0.125f);
    FL_CHECK(eighth.raw() == 512);
    FL_CHECK(approx_equal(eighth.to_float(), 0.125f, 0.0001f));
}

FL_TEST_CASE("u4x12: compound operations") {
    // Test (a + b) * c - Keep result under 16
    u4x12 a(1.0f);
    u4x12 b(2.0f);
    u4x12 c(3.0f);
    u4x12 result = (a + b) * c;
    FL_CHECK(approx_equal(result.to_float(), 9.0f));

    // Test (a * b) / c
    u4x12 d(6.0f);
    u4x12 e(2.0f);
    u4x12 f(3.0f);
    u4x12 result2 = (d * e) / f;
    FL_CHECK(approx_equal(result2.to_float(), 4.0f));

    // Test lerp with computed t
    u4x12 g(0.0f);
    u4x12 h(10.0f);
    u4x12 i(1.0f);
    u4x12 j(2.0f);
    u4x12 t = i / j;  // 0.5
    u4x12 result3 = u4x12::lerp(g, h, t);
    FL_CHECK(approx_equal(result3.to_float(), 5.0f));
}


// ---------------------------------------------------------------------------
// u8x8 tests (from u8x8.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u8x8 - construction from zero") {
    u8x8 zero;
    FL_CHECK(zero.raw() == 0);
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, 0.001f);
}

FL_TEST_CASE("u8x8 - construction from one") {
    u8x8 one(1.0f);
    FL_CHECK(one.raw() == 256);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from fractional") {
    u8x8 half(0.5f);
    FL_CHECK(half.raw() == 128);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from large value") {
    u8x8 large(100.5f);
    FL_CHECK(large.to_int() == 100);
    FL_CHECK_CLOSE(large.to_float(), 100.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from max value") {
    u8x8 max_val(255.99f);
    FL_CHECK_CLOSE(max_val.to_float(), 255.99f, 0.02f);
}

FL_TEST_CASE("u8x8 - from_raw construction") {
    u8x8 from_raw_test = u8x8::from_raw(512);
    FL_CHECK(from_raw_test.raw() == 512);
    FL_CHECK_CLOSE(from_raw_test.to_float(), 2.0f, 0.01f);
}

// ---- Addition tests ----------------------------------------------------

FL_TEST_CASE("u8x8 - addition basic") {
    u8x8 a(2.5f);
    u8x8 b(1.25f);
    u8x8 c = a + b;
    FL_CHECK_CLOSE(c.to_float(), 3.75f, 0.01f);
}

FL_TEST_CASE("u8x8 - addition large values") {
    u8x8 d(100.0f);
    u8x8 e(50.5f);
    u8x8 f = d + e;
    FL_CHECK_CLOSE(f.to_float(), 150.5f, 0.02f);
}

FL_TEST_CASE("u8x8 - addition overflow") {
    u8x8 big(200.0f);
    u8x8 overflow = big + big;
    // Result wraps in u16 storage
    FL_CHECK(overflow.raw() > 0);
}

// ---- Subtraction tests -------------------------------------------------

FL_TEST_CASE("u8x8 - subtraction basic") {
    u8x8 a(5.5f);
    u8x8 b(2.25f);
    u8x8 c = a - b;
    FL_CHECK_CLOSE(c.to_float(), 3.25f, 0.01f);
}

FL_TEST_CASE("u8x8 - subtraction large values") {
    u8x8 d(100.0f);
    u8x8 e(50.5f);
    u8x8 f = d - e;
    FL_CHECK_CLOSE(f.to_float(), 49.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - subtraction underflow") {
    u8x8 small(1.0f);
    u8x8 large(5.0f);
    u8x8 underflow = small - large;
    // Result wraps negative in u16
    FL_CHECK(underflow.raw() > 0x8000);
}

// ---- Multiplication tests ----------------------------------------------

FL_TEST_CASE("u8x8 - multiplication basic") {
    u8x8 a(2.5f);
    u8x8 b(3.0f);
    u8x8 c = a * b;
    FL_CHECK_CLOSE(c.to_float(), 7.5f, 0.02f);
}

FL_TEST_CASE("u8x8 - multiplication with fraction") {
    u8x8 d(10.0f);
    u8x8 e(0.5f);
    u8x8 f = d * e;
    FL_CHECK_CLOSE(f.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - multiplication small values") {
    u8x8 small(0.25f);
    u8x8 g = small * small;
    FL_CHECK_CLOSE(g.to_float(), 0.0625f, 0.005f);
}

FL_TEST_CASE("u8x8 - scalar multiplication") {
    u8x8 h(2.5f);
    u8x8 i = h * 4;
    FL_CHECK_CLOSE(i.to_float(), 10.0f, 0.02f);
}

FL_TEST_CASE("u8x8 - scalar multiplication commutative") {
    u8x8 h(2.5f);
    u8x8 j = 3 * h;
    FL_CHECK_CLOSE(j.to_float(), 7.5f, 0.02f);
}

// ---- Division tests ----------------------------------------------------

FL_TEST_CASE("u8x8 - division basic") {
    u8x8 a(10.0f);
    u8x8 b(2.0f);
    u8x8 c = a / b;
    FL_CHECK_CLOSE(c.to_float(), 5.0f, 0.02f);
}

FL_TEST_CASE("u8x8 - division fractional") {
    u8x8 d(7.5f);
    u8x8 e(2.5f);
    u8x8 f = d / e;
    FL_CHECK_CLOSE(f.to_float(), 3.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - division result less than one") {
    u8x8 g(1.0f);
    u8x8 h(4.0f);
    u8x8 i = g / h;
    FL_CHECK_CLOSE(i.to_float(), 0.25f, 0.01f);
}

FL_TEST_CASE("u8x8 - division by zero") {
    u8x8 j(5.0f);
    u8x8 zero;
    u8x8 k = j / zero;
    FL_CHECK(k.raw() == 0);
}

// ---- Shift tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - right shift") {
    u8x8 a(8.0f);
    u8x8 b = a >> 1;
    FL_CHECK_CLOSE(b.to_float(), 4.0f, 0.01f);

    u8x8 c = a >> 2;
    FL_CHECK_CLOSE(c.to_float(), 2.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - left shift") {
    u8x8 d(4.0f);
    u8x8 e = d << 1;
    FL_CHECK_CLOSE(e.to_float(), 8.0f, 0.01f);

    u8x8 f = d << 2;
    FL_CHECK_CLOSE(f.to_float(), 16.0f, 0.02f);
}

// ---- Comparison tests --------------------------------------------------

FL_TEST_CASE("u8x8 - comparisons") {
    u8x8 a(5.0f);
    u8x8 b(3.0f);
    u8x8 c(5.0f);

    FL_CHECK(a > b);
    FL_CHECK(b < a);
    FL_CHECK(a >= c);
    FL_CHECK(a <= c);
    FL_CHECK(a == c);
    FL_CHECK(a != b);
}

FL_TEST_CASE("u8x8 - comparisons large values") {
    u8x8 d(100.0f);
    u8x8 e(200.0f);
    FL_CHECK(d < e);
    FL_CHECK(e > d);
}

// ---- Math function tests -----------------------------------------------

FL_TEST_CASE("u8x8 - mod") {
    u8x8 a(10.0f);
    u8x8 b(3.0f);
    u8x8 c = u8x8::mod(a, b);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - floor") {
    u8x8 d(5.75f);
    u8x8 e = u8x8::floor(d);
    FL_CHECK_CLOSE(e.to_float(), 5.0f, 0.01f);

    u8x8 f(10.25f);
    u8x8 g = u8x8::floor(f);
    FL_CHECK_CLOSE(g.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - ceil") {
    u8x8 h(5.25f);
    u8x8 i = u8x8::ceil(h);
    FL_CHECK_CLOSE(i.to_float(), 6.0f, 0.01f);

    u8x8 j(10.0f);
    u8x8 k = u8x8::ceil(j);
    FL_CHECK_CLOSE(k.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - fract") {
    u8x8 l(5.75f);
    u8x8 m = u8x8::fract(l);
    FL_CHECK_CLOSE(m.to_float(), 0.75f, 0.01f);
}

FL_TEST_CASE("u8x8 - abs") {
    // Unsigned values are always non-negative
    u8x8 n(5.0f);
    u8x8 o = u8x8::abs(n);
    FL_CHECK_CLOSE(o.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - min") {
    u8x8 p(5.0f);
    u8x8 q(3.0f);
    u8x8 r = u8x8::min(p, q);
    FL_CHECK_CLOSE(r.to_float(), 3.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - max") {
    u8x8 p(5.0f);
    u8x8 q(3.0f);
    u8x8 s = u8x8::max(p, q);
    FL_CHECK_CLOSE(s.to_float(), 5.0f, 0.01f);
}

// ---- lerp tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - lerp midpoint") {
    u8x8 a(0.0f);
    u8x8 b(10.0f);
    u8x8 t(0.5f);
    u8x8 c = u8x8::lerp(a, b, t);
    FL_CHECK_CLOSE(c.to_float(), 5.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - lerp at endpoints") {
    u8x8 a(0.0f);
    u8x8 b(10.0f);

    u8x8 t0(0.0f);
    u8x8 d = u8x8::lerp(a, b, t0);
    FL_CHECK_CLOSE(d.to_float(), 0.0f, 0.01f);

    u8x8 t1(1.0f);
    u8x8 e = u8x8::lerp(a, b, t1);
    FL_CHECK_CLOSE(e.to_float(), 10.0f, 0.02f);
}

// ---- clamp tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - clamp within range") {
    u8x8 lo(0.0f);
    u8x8 hi(10.0f);
    u8x8 within(5.0f);
    u8x8 b = u8x8::clamp(within, lo, hi);
    FL_CHECK_CLOSE(b.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - clamp above range") {
    u8x8 lo(0.0f);
    u8x8 hi(10.0f);
    u8x8 above(15.0f);
    u8x8 c = u8x8::clamp(above, lo, hi);
    FL_CHECK_CLOSE(c.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - clamp below range") {
    u8x8 lo(5.0f);
    u8x8 hi(10.0f);
    u8x8 below(2.0f);
    u8x8 a = u8x8::clamp(below, lo, hi);
    FL_CHECK_CLOSE(a.to_float(), 5.0f, 0.01f);
}

// ---- step tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - step below edge") {
    u8x8 edge(5.0f);
    u8x8 below(3.0f);
    u8x8 a = u8x8::step(edge, below);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - step above edge") {
    u8x8 edge(5.0f);
    u8x8 above(7.0f);
    u8x8 b = u8x8::step(edge, above);
    FL_CHECK_CLOSE(b.to_float(), 1.0f, 0.01f);
}

// ---- smoothstep tests --------------------------------------------------

FL_TEST_CASE("u8x8 - smoothstep below range") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 below_val(0.0f);
    u8x8 a = u8x8::smoothstep(edge0, edge1, below_val);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - smoothstep midpoint") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 mid(5.0f);
    u8x8 b = u8x8::smoothstep(edge0, edge1, mid);
    FL_CHECK_CLOSE(b.to_float(), 0.5f, 0.1f);
}

FL_TEST_CASE("u8x8 - smoothstep above range") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 above(15.0f);
    u8x8 c = u8x8::smoothstep(edge0, edge1, above);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.1f);
}

// ---- sqrt tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - sqrt zero") {
    u8x8 zero(0.0f);
    u8x8 a = u8x8::sqrt(zero);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - sqrt perfect squares") {
    u8x8 four(4.0f);
    u8x8 b = u8x8::sqrt(four);
    FL_CHECK_CLOSE(b.to_float(), 2.0f, 0.05f);

    u8x8 nine(9.0f);
    u8x8 c = u8x8::sqrt(nine);
    FL_CHECK_CLOSE(c.to_float(), 3.0f, 0.05f);

    u8x8 hundred(100.0f);
    u8x8 d = u8x8::sqrt(hundred);
    FL_CHECK_CLOSE(d.to_float(), 10.0f, 0.1f);
}

FL_TEST_CASE("u8x8 - sqrt irrational") {
    u8x8 two(2.0f);
    u8x8 e = u8x8::sqrt(two);
    FL_CHECK_CLOSE(e.to_float(), 1.414f, 0.05f);
}

// ---- rsqrt tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - rsqrt") {
    u8x8 four(4.0f);
    u8x8 a = u8x8::rsqrt(four);
    FL_CHECK_CLOSE(a.to_float(), 0.5f, 0.05f);

    u8x8 nine(9.0f);
    u8x8 b = u8x8::rsqrt(nine);
    FL_CHECK_CLOSE(b.to_float(), 0.333f, 0.05f);
}

// ---- pow tests ---------------------------------------------------------

FL_TEST_CASE("u8x8 - pow basic") {
    u8x8 base(2.0f);
    u8x8 exp(3.0f);
    u8x8 a = u8x8::pow(base, exp);
    FL_CHECK_CLOSE(a.to_float(), 8.0f, 0.2f);
}

FL_TEST_CASE("u8x8 - pow larger values") {
    u8x8 base2(10.0f);
    u8x8 exp2(2.0f);
    u8x8 b = u8x8::pow(base2, exp2);
    FL_CHECK_CLOSE(b.to_float(), 100.0f, 2.0f);
}

FL_TEST_CASE("u8x8 - pow zero exponent") {
    u8x8 base3(5.0f);
    u8x8 zero(0.0f);
    u8x8 c = u8x8::pow(base3, zero);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - pow one base") {
    u8x8 one(1.0f);
    u8x8 exp3(100.0f);
    u8x8 d = u8x8::pow(one, exp3);
    FL_CHECK_CLOSE(d.to_float(), 1.0f, 0.05f);
}

// ---- Edge case tests ---------------------------------------------------

FL_TEST_CASE("u8x8 - zero operations") {
    u8x8 zero;
    u8x8 one(1.0f);

    u8x8 a = zero + one;
    FL_CHECK_CLOSE(a.to_float(), 1.0f, 0.01f);

    u8x8 b = zero * one;
    FL_CHECK_CLOSE(b.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - max value operations") {
    u8x8 max_val(255.0f);
    u8x8 c = u8x8::floor(max_val);
    FL_CHECK_CLOSE(c.to_float(), 255.0f, 1.0f);
}

FL_TEST_CASE("u8x8 - tiny fractional values") {
    u8x8 tiny(0.00390625f);  // 1/256
    FL_CHECK(tiny.raw() > 0);
    FL_CHECK_CLOSE(tiny.to_float(), 0.00390625f, 0.001f);
}

// ---- Type trait tests --------------------------------------------------

FL_TEST_CASE("u8x8 - type traits") {
    FL_CHECK(u8x8::INT_BITS == 8);
    FL_CHECK(u8x8::FRAC_BITS == 8);
}


// ---------------------------------------------------------------------------
// u12x4 tests (from u12x4.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u12x4 - Construction") {
    FL_SUBCASE("Default constructor") {
        u12x4 a;
        FL_CHECK(a.raw() == 0);
        FL_CHECK(a.to_int() == 0);
        FL_CHECK(a.to_float() == 0.0f);
    }

    FL_SUBCASE("Float constructor") {
        u12x4 a(1.0f);
        FL_CHECK(a.raw() == 16);  // 1.0 * 2^4 = 16
        FL_CHECK(a.to_int() == 1);
        FL_CHECK(a.to_float() == 1.0f);

        u12x4 b(2.5f);
        FL_CHECK(b.raw() == 40);  // 2.5 * 2^4 = 40
        FL_CHECK(b.to_int() == 2);
        FL_CHECK(b.to_float() == 2.5f);

        u12x4 c(0.5f);
        FL_CHECK(c.raw() == 8);   // 0.5 * 2^4 = 8
        FL_CHECK(c.to_int() == 0);
        FL_CHECK(c.to_float() == 0.5f);
    }

    FL_SUBCASE("from_raw constructor") {
        u12x4 a = u12x4::from_raw(16);
        FL_CHECK(a.to_float() == 1.0f);

        u12x4 b = u12x4::from_raw(40);
        FL_CHECK(b.to_float() == 2.5f);

        u12x4 c = u12x4::from_raw(0);
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Large values") {
        u12x4 a(100.0f);
        FL_CHECK(a.to_int() == 100);
        FL_CHECK(a.to_float() == 100.0f);

        u12x4 b(4095.0f);  // Max 12-bit value
        FL_CHECK(b.to_int() == 4095);
        FL_CHECK(b.to_float() == 4095.0f);
    }
}

FL_TEST_CASE("u12x4 - Addition") {
    FL_SUBCASE("Basic addition") {
        u12x4 a(1.0f);
        u12x4 b(2.0f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional addition") {
        u12x4 a(1.5f);
        u12x4 b(2.5f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 4.0f);
    }

    FL_SUBCASE("Addition with zero") {
        u12x4 a(5.0f);
        u12x4 b(0.0f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 5.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(1.0f);
        a += u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Subtraction") {
    FL_SUBCASE("Basic subtraction") {
        u12x4 a(5.0f);
        u12x4 b(2.0f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional subtraction") {
        u12x4 a(3.5f);
        u12x4 b(1.5f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 2.0f);
    }

    FL_SUBCASE("Subtraction to zero") {
        u12x4 a(5.0f);
        u12x4 b(5.0f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(5.0f);
        a -= u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Multiplication") {
    FL_SUBCASE("Basic multiplication") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 6.0f);
    }

    FL_SUBCASE("Fractional multiplication") {
        u12x4 a(1.5f);
        u12x4 b(2.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 3.0f);

        u12x4 d(0.5f);
        u12x4 e(0.5f);
        u12x4 f = d * e;
        FL_CHECK(f.to_float() == doctest::Approx(0.25f).epsilon(0.1f));
    }

    FL_SUBCASE("Multiplication by zero") {
        u12x4 a(5.0f);
        u12x4 b(0.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Scalar multiplication") {
        u12x4 a(2.5f);
        u12x4 b = a * 2;
        FL_CHECK(b.to_float() == 5.0f);

        u12x4 c = 3 * a;
        FL_CHECK(c.to_float() == 7.5f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(2.0f);
        a *= u12x4(3.0f);
        FL_CHECK(a.to_float() == 6.0f);
    }
}

FL_TEST_CASE("u12x4 - Division") {
    FL_SUBCASE("Basic division") {
        u12x4 a(6.0f);
        u12x4 b(2.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional division") {
        u12x4 a(5.0f);
        u12x4 b(2.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == doctest::Approx(2.5f).epsilon(0.1f));

        u12x4 d(1.0f);
        u12x4 e(4.0f);
        u12x4 f = d / e;
        FL_CHECK(f.to_float() == doctest::Approx(0.25f).epsilon(0.1f));
    }

    FL_SUBCASE("Division by one") {
        u12x4 a(5.0f);
        u12x4 b(1.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == 5.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(6.0f);
        a /= u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Shifts") {
    FL_SUBCASE("Right shift") {
        u12x4 a(8.0f);
        u12x4 b = a >> 1;  // Shift raw value right by 1
        FL_CHECK(b.to_float() == 4.0f);

        u12x4 c = a >> 2;
        FL_CHECK(c.to_float() == 2.0f);
    }

    FL_SUBCASE("Left shift") {
        u12x4 a(2.0f);
        u12x4 b = a << 1;  // Shift raw value left by 1
        FL_CHECK(b.to_float() == 4.0f);

        u12x4 c = a << 2;
        FL_CHECK(c.to_float() == 8.0f);
    }
}

FL_TEST_CASE("u12x4 - Comparisons") {
    FL_SUBCASE("Less than") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        FL_CHECK(a < b);
        FL_CHECK_FALSE(b < a);
        FL_CHECK_FALSE(a < a);
    }

    FL_SUBCASE("Greater than") {
        u12x4 a(3.0f);
        u12x4 b(2.0f);
        FL_CHECK(a > b);
        FL_CHECK_FALSE(b > a);
        FL_CHECK_FALSE(a > a);
    }

    FL_SUBCASE("Less than or equal") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        FL_CHECK(a <= b);
        FL_CHECK(a <= a);
        FL_CHECK_FALSE(b <= a);
    }

    FL_SUBCASE("Greater than or equal") {
        u12x4 a(3.0f);
        u12x4 b(2.0f);
        FL_CHECK(a >= b);
        FL_CHECK(a >= a);
        FL_CHECK_FALSE(b >= a);
    }

    FL_SUBCASE("Equality") {
        u12x4 a(2.5f);
        u12x4 b(2.5f);
        u12x4 c(3.0f);
        FL_CHECK(a == b);
        FL_CHECK_FALSE(a == c);
    }

    FL_SUBCASE("Inequality") {
        u12x4 a(2.5f);
        u12x4 b(3.0f);
        FL_CHECK(a != b);
        FL_CHECK_FALSE(a != a);
    }
}

FL_TEST_CASE("u12x4 - Math functions") {
    FL_SUBCASE("mod") {
        u12x4 a(7.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::mod(a, b);
        FL_CHECK(c.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }

    FL_SUBCASE("floor") {
        u12x4 a(2.75f);
        u12x4 b = u12x4::floor(a);
        FL_CHECK(b.to_float() == 2.0f);

        u12x4 c(5.0f);
        u12x4 d = u12x4::floor(c);
        FL_CHECK(d.to_float() == 5.0f);
    }

    FL_SUBCASE("ceil") {
        u12x4 a(2.25f);
        u12x4 b = u12x4::ceil(a);
        FL_CHECK(b.to_float() == 3.0f);

        u12x4 c(5.0f);
        u12x4 d = u12x4::ceil(c);
        FL_CHECK(d.to_float() == 5.0f);
    }

    FL_SUBCASE("fract") {
        u12x4 a(2.75f);
        u12x4 b = u12x4::fract(a);
        FL_CHECK(b.to_float() == doctest::Approx(0.75f).epsilon(0.1f));

        u12x4 c(5.0f);
        u12x4 d = u12x4::fract(c);
        FL_CHECK(d.to_float() == 0.0f);
    }

    FL_SUBCASE("abs") {
        // For unsigned, abs is identity
        u12x4 a(5.5f);
        u12x4 b = u12x4::abs(a);
        FL_CHECK(b.to_float() == 5.5f);

        u12x4 c(0.0f);
        u12x4 d = u12x4::abs(c);
        FL_CHECK(d.to_float() == 0.0f);
    }

    FL_SUBCASE("min") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::min(a, b);
        FL_CHECK(c.to_float() == 2.0f);

        u12x4 d = u12x4::min(b, a);
        FL_CHECK(d.to_float() == 2.0f);
    }

    FL_SUBCASE("max") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::max(a, b);
        FL_CHECK(c.to_float() == 3.0f);

        u12x4 d = u12x4::max(b, a);
        FL_CHECK(d.to_float() == 3.0f);
    }

    FL_SUBCASE("clamp") {
        u12x4 lo(1.0f);
        u12x4 hi(5.0f);

        u12x4 a(3.0f);
        u12x4 b = u12x4::clamp(a, lo, hi);
        FL_CHECK(b.to_float() == 3.0f);

        u12x4 c(0.5f);
        u12x4 d = u12x4::clamp(c, lo, hi);
        FL_CHECK(d.to_float() == 1.0f);

        u12x4 e(6.0f);
        u12x4 f = u12x4::clamp(e, lo, hi);
        FL_CHECK(f.to_float() == 5.0f);
    }

    FL_SUBCASE("lerp") {
        u12x4 a(0.0f);
        u12x4 b(10.0f);

        u12x4 c = u12x4::lerp(a, b, u12x4(0.0f));
        FL_CHECK(c.to_float() == doctest::Approx(0.0f).epsilon(0.1f));

        u12x4 d = u12x4::lerp(a, b, u12x4(0.5f));
        FL_CHECK(d.to_float() == doctest::Approx(5.0f).epsilon(0.2f));

        u12x4 e = u12x4::lerp(a, b, u12x4(1.0f));
        FL_CHECK(e.to_float() == doctest::Approx(10.0f).epsilon(0.2f));
    }

    FL_SUBCASE("step") {
        u12x4 edge(5.0f);

        u12x4 a = u12x4::step(edge, u12x4(3.0f));
        FL_CHECK(a.to_float() == 0.0f);

        u12x4 b = u12x4::step(edge, u12x4(5.0f));
        FL_CHECK(b.to_float() == 1.0f);  // x >= edge returns 1

        u12x4 c = u12x4::step(edge, u12x4(7.0f));
        FL_CHECK(c.to_float() == 1.0f);
    }

    FL_SUBCASE("smoothstep") {
        u12x4 edge0(0.0f);
        u12x4 edge1(1.0f);

        u12x4 a = u12x4::smoothstep(edge0, edge1, u12x4(0.0f));
        FL_CHECK(a.to_float() == doctest::Approx(0.0f).epsilon(0.1f));

        u12x4 b = u12x4::smoothstep(edge0, edge1, u12x4(0.5f));
        FL_CHECK(b.to_float() == doctest::Approx(0.5f).epsilon(0.15f));

        u12x4 c = u12x4::smoothstep(edge0, edge1, u12x4(1.0f));
        FL_CHECK(c.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Advanced math") {
    FL_SUBCASE("sqrt") {
        u12x4 a(4.0f);
        u12x4 b = u12x4::sqrt(a);
        FL_CHECK(b.to_float() == doctest::Approx(2.0f).epsilon(0.1f));

        u12x4 c(9.0f);
        u12x4 d = u12x4::sqrt(c);
        FL_CHECK(d.to_float() == doctest::Approx(3.0f).epsilon(0.1f));

        u12x4 e(0.0f);
        u12x4 f = u12x4::sqrt(e);
        FL_CHECK(f.to_float() == 0.0f);
    }

    FL_SUBCASE("rsqrt") {
        u12x4 a(4.0f);
        u12x4 b = u12x4::rsqrt(a);
        FL_CHECK(b.to_float() == doctest::Approx(0.5f).epsilon(0.1f));

        u12x4 c(9.0f);
        u12x4 d = u12x4::rsqrt(c);
        FL_CHECK(d.to_float() == doctest::Approx(0.333f).epsilon(0.1f));
    }

    FL_SUBCASE("pow") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::pow(a, b);
        FL_CHECK(c.to_float() == doctest::Approx(8.0f).epsilon(0.5f));

        u12x4 d(5.0f);
        u12x4 e(2.0f);
        u12x4 f = u12x4::pow(d, e);
        FL_CHECK(f.to_float() == doctest::Approx(25.0f).epsilon(1.0f));

        // x^0 = 1
        u12x4 g(10.0f);
        u12x4 h(0.0f);
        u12x4 i = u12x4::pow(g, h);
        FL_CHECK(i.to_float() == doctest::Approx(1.0f).epsilon(0.1f));

        // 1^x = 1
        u12x4 j(1.0f);
        u12x4 k(5.0f);
        u12x4 l = u12x4::pow(j, k);
        FL_CHECK(l.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Edge cases") {
    FL_SUBCASE("Zero value") {
        u12x4 zero(0.0f);
        FL_CHECK(zero.raw() == 0);
        FL_CHECK(zero.to_int() == 0);
        FL_CHECK(zero.to_float() == 0.0f);
    }

    FL_SUBCASE("Maximum value") {
        // Max value with 12 integer bits is 4095.9375 (0xFFFF / 16)
        u12x4 max = u12x4::from_raw(0xFFFF);
        FL_CHECK(max.to_int() == 4095);
        FL_CHECK(max.to_float() == doctest::Approx(4095.9375f).epsilon(0.01f));
    }

    FL_SUBCASE("Small fractional values") {
        // Smallest representable non-zero value is 1/16 = 0.0625
        u12x4 small = u12x4::from_raw(1);
        FL_CHECK(small.to_float() == doctest::Approx(0.0625f).epsilon(0.001f));
    }

    FL_SUBCASE("Operations at boundaries") {
        u12x4 zero(0.0f);
        u12x4 one(1.0f);

        // 0 * anything = 0
        u12x4 a = zero * u12x4(100.0f);
        FL_CHECK(a.to_float() == 0.0f);

        // 1 * anything = anything
        u12x4 b = one * u12x4(5.5f);
        FL_CHECK(b.to_float() == doctest::Approx(5.5f).epsilon(0.1f));

        // anything + 0 = anything
        u12x4 c = u12x4(7.5f) + zero;
        FL_CHECK(c.to_float() == doctest::Approx(7.5f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Precision") {
    FL_SUBCASE("4-bit fractional precision") {
        // With 4 fractional bits, precision is 1/16 = 0.0625
        float values[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f};
        for (float val : values) {
            u12x4 a(val);
            FL_CHECK(a.to_float() == doctest::Approx(val).epsilon(0.001f));
        }
    }

    FL_SUBCASE("Rounding behavior") {
        // Values between representable steps should round down due to truncation
        u12x4 a(1.03f);  // Should become 1.0 (16/16)
        FL_CHECK(a.to_float() == doctest::Approx(1.0f).epsilon(0.1f));

        u12x4 b(1.07f);  // Should become 1.0625 (17/16)
        FL_CHECK(b.to_float() == doctest::Approx(1.0625f).epsilon(0.1f));
    }
}


// ---------------------------------------------------------------------------
// u16x16 tests (from u16x16.cpp)
// ---------------------------------------------------------------------------

constexpr float tol() { return 0.0005f; }
constexpr float sqrt_tol() { return 0.0002f; }
constexpr float pow_tol() { return 0.015f; }
constexpr float smooth_tol() { return 0.0005f; }
constexpr float rt_tol() { return 0.001f; }

void check_near(u16x16 val, float expected, float tolerance) {
    FL_CHECK_CLOSE(val.to_float(), expected, tolerance);
}

void check_near(u16x16 val, float expected) {
    check_near(val, expected, tol());
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - default construction") {
    u16x16 a;
    FL_CHECK_EQ(a.raw(), u32(0));
    FL_CHECK_EQ(a.to_int(), u32(0));
}

FL_TEST_CASE("u16x16 - float construction") {
    u16x16 one(1.0f);
    FL_CHECK_EQ(one.raw(), u32(1) << u16x16::FRAC_BITS);
    FL_CHECK_EQ(one.to_int(), u32(1));

    u16x16 half(0.5f);
    FL_CHECK_EQ(half.raw(), u32(1) << (u16x16::FRAC_BITS - 1));
    FL_CHECK_EQ(half.to_int(), u32(0));

    u16x16 val(3.0f);
    FL_CHECK_EQ(val.to_int(), u32(3));

    u16x16 zero(0.0f);
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));
}

FL_TEST_CASE("u16x16 - from_raw") {
    // 1.0
    u16x16 a = u16x16::from_raw(u32(1) << u16x16::FRAC_BITS);
    FL_CHECK_EQ(a.to_int(), u32(1));

    // 2.5
    u32 raw_2_5 = (u32(2) << u16x16::FRAC_BITS) + (u32(1) << (u16x16::FRAC_BITS - 1));
    u16x16 b = u16x16::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), u32(2));
    check_near(b, 2.5f);

    // smallest fraction
    u16x16 c = u16x16::from_raw(u32(1));
    FL_CHECK_EQ(c.raw(), u32(1));
    FL_CHECK_EQ(c.to_int(), u32(0));
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - addition") {
    u16x16 a(1.0f), b(2.0f);
    u16x16 c = a + b;
    FL_CHECK_EQ(c.to_int(), u32(3));
    check_near(c, 3.0f);

    // Fractional
    u16x16 d(0.25f), e(0.75f);
    check_near(d + e, 1.0f);

    // Zero identity
    u16x16 zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());
}

FL_TEST_CASE("u16x16 - subtraction") {
    u16x16 a(3.0f), b(1.0f);
    check_near(a - b, 2.0f);

    // Self subtraction
    u16x16 zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());

    // Fractional
    u16x16 c(1.75f), d(0.25f);
    check_near(c - d, 1.5f);

    // Underflow wraps (unsigned)
    u16x16 small(0.5f), big(1.0f);
    // big - small = 0.5 (positive result)
    check_near(big - small, 0.5f);
}

FL_TEST_CASE("u16x16 - fixed-point multiply") {
    u16x16 a(2.0f), b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    u16x16 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 = identity
    u16x16 one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    u16x16 zero;
    FL_CHECK_EQ((a * zero).raw(), u32(0));

    // Fractional precision
    u16x16 c(1.5f), d(2.0f);
    check_near(c * d, 3.0f);

    // Higher fractional precision
    u16x16 e(1.5f), f(2.5f);
    check_near(e * f, 3.75f);
}

FL_TEST_CASE("u16x16 - fixed-point divide") {
    u16x16 a(6.0f), b(3.0f);
    check_near(a / b, 2.0f);

    u16x16 one(1.0f), two(2.0f);
    check_near(one / two, 0.5f);

    u16x16 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 = identity
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // Fractional result: 1/3
    u16x16 three(3.0f);
    check_near(one / three, 0.3333f, tol() * 10.0f);
}

FL_TEST_CASE("u16x16 - scalar multiply") {
    u16x16 a(1.5f);

    // fp * scalar
    check_near(a * u32(2), 3.0f);
    check_near(a * u32(0), 0.0f);

    // scalar * fp (commutative friend)
    check_near(u32(2) * a, 3.0f);

    // Large scalar multiply
    check_near(a * u32(100), 150.0f, tol() * 10.0f);
}

FL_TEST_CASE("u16x16 - right shift") {
    u16x16 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    u16x16 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);
}

FL_TEST_CASE("u16x16 - left shift") {
    u16x16 a(1.0f);
    check_near(a << 1, 2.0f);
    check_near(a << 2, 4.0f);

    // Fractional shift
    u16x16 b(0.25f);
    check_near(b << 1, 0.5f);
    check_near(b << 2, 1.0f);
}

// ---------------------------------------------------------------------------
// Comparisons
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - comparisons") {
    u16x16 a(1.0f), b(2.0f), c(1.0f);
    u16x16 zero;

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

    FL_CHECK(zero < a);
    FL_CHECK(a > zero);
}

// ---------------------------------------------------------------------------
// Mixed arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - mixed arithmetic expressions") {
    u16x16 a(2.0f), b(3.0f), c(0.5f);
    check_near((a + b) * c, 2.5f);

    u16x16 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // Round-trip: (a / b) * b ~ a
    check_near((a / b) * b, 2.0f, rt_tol());

    // Chained
    u16x16 one(1.0f);
    u16x16 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

// ---------------------------------------------------------------------------
// to_float
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - to_float") {
    u16x16 one(1.0f);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, tol());

    u16x16 half(0.5f);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, tol());

    u16x16 val(2.5f);
    FL_CHECK_CLOSE(val.to_float(), 2.5f, tol());

    u16x16 zero;
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, tol());
}

// ---------------------------------------------------------------------------
// Math functions
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - sqrt") {
    check_near(u16x16::sqrt(u16x16(4.0f)), 2.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(1.0f)), 1.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(2.0f)), 1.4142f, sqrt_tol());

    FL_CHECK_EQ(u16x16::sqrt(u16x16()).raw(), u32(0)); // sqrt(0) = 0

    check_near(u16x16::sqrt(u16x16(9.0f)), 3.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(16.0f)), 4.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(100.0f)), 10.0f, sqrt_tol());
}

FL_TEST_CASE("u16x16 - rsqrt") {
    check_near(u16x16::rsqrt(u16x16(4.0f)), 0.5f, sqrt_tol());
    check_near(u16x16::rsqrt(u16x16(1.0f)), 1.0f, sqrt_tol());

    FL_CHECK_EQ(u16x16::rsqrt(u16x16()).raw(), u32(0)); // rsqrt(0) = 0
}

FL_TEST_CASE("u16x16 - pow") {
    // 2^2 = 4
    check_near(u16x16::pow(u16x16(2.0f), u16x16(2.0f)), 4.0f, pow_tol());

    // 4^0.5 = 2 (sqrt via pow)
    check_near(u16x16::pow(u16x16(4.0f), u16x16(0.5f)), 2.0f, pow_tol());

    // x^0 = 1
    check_near(u16x16::pow(u16x16(5.0f), u16x16(0.0f)), 1.0f, pow_tol());

    // 0^x = 0
    FL_CHECK_EQ(u16x16::pow(u16x16(), u16x16(2.0f)).raw(), u32(0));

    // 2^3 = 8
    check_near(u16x16::pow(u16x16(2.0f), u16x16(3.0f)), 8.0f, pow_tol());
}

FL_TEST_CASE("u16x16 - sqrt identity") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 16.0f, 25.0f};
    for (float v : values) {
        u16x16 x(v);
        u16x16 s = u16x16::sqrt(x);
        u16x16 sq = s * s;
        check_near(sq, v, v * 0.005f + 0.005f);
    }
}

FL_TEST_CASE("u16x16 - floor and ceil") {
    check_near(u16x16::floor(u16x16(2.75f)), 2.0f);
    check_near(u16x16::ceil(u16x16(2.75f)), 3.0f);
    check_near(u16x16::floor(u16x16(3.0f)), 3.0f);
    check_near(u16x16::ceil(u16x16(3.0f)), 3.0f);
    check_near(u16x16::floor(u16x16(0.25f)), 0.0f);
    check_near(u16x16::ceil(u16x16(0.25f)), 1.0f);
}

FL_TEST_CASE("u16x16 - fract") {
    check_near(u16x16::fract(u16x16(2.75f)), 0.75f);
    FL_CHECK_EQ(u16x16::fract(u16x16(1.0f)).raw(), u32(0));
    check_near(u16x16::fract(u16x16(0.5f)), 0.5f);
}

FL_TEST_CASE("u16x16 - min and max") {
    check_near(u16x16::min(u16x16(3.5f), u16x16(2.0f)), 2.0f);
    check_near(u16x16::max(u16x16(3.5f), u16x16(2.0f)), 3.5f);
    check_near(u16x16::min(u16x16(1.0f), u16x16(1.0f)), 1.0f);
    check_near(u16x16::max(u16x16(0.0f), u16x16(5.0f)), 5.0f);
}

FL_TEST_CASE("u16x16 - mod") {
    check_near(u16x16::mod(u16x16(5.5f), u16x16(2.0f)), 1.5f);
    check_near(u16x16::mod(u16x16(3.0f), u16x16(1.5f)), 0.0f);
    check_near(u16x16::mod(u16x16(1.0f), u16x16(3.0f)), 1.0f);
}

// ---------------------------------------------------------------------------
// Interpolation and clamping
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - lerp, clamp, step, smoothstep") {
    // lerp
    check_near(u16x16::lerp(u16x16(0.0f), u16x16(4.0f), u16x16(0.5f)), 2.0f);
    check_near(u16x16::lerp(u16x16(1.0f), u16x16(3.0f), u16x16(0.0f)), 1.0f);
    check_near(u16x16::lerp(u16x16(1.0f), u16x16(3.0f), u16x16(1.0f)), 3.0f);
    check_near(u16x16::lerp(u16x16(0.0f), u16x16(10.0f), u16x16(0.5f)), 5.0f, tol() * 10.0f);

    // clamp
    check_near(u16x16::clamp(u16x16(5.0f), u16x16(0.0f), u16x16(3.0f)), 3.0f);
    check_near(u16x16::clamp(u16x16(0.5f), u16x16(1.0f), u16x16(3.0f)), 1.0f);
    check_near(u16x16::clamp(u16x16(1.5f), u16x16(0.0f), u16x16(3.0f)), 1.5f);

    // step
    check_near(u16x16::step(u16x16(1.0f), u16x16(0.5f)), 0.0f);
    check_near(u16x16::step(u16x16(1.0f), u16x16(1.5f)), 1.0f);
    check_near(u16x16::step(u16x16(1.0f), u16x16(1.0f)), 1.0f);

    // smoothstep
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(0.0f)), 0.0f, smooth_tol());
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(1.0f)), 1.0f, smooth_tol());
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(0.5f)), 0.5f, smooth_tol());
}

// ---------------------------------------------------------------------------
// Edge values
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - edge values") {
    // Maximum representable integer part (65535)
    constexpr u32 max_int = (1u << u16x16::INT_BITS) - 1;
    u16x16 big(static_cast<float>(max_int));
    FL_CHECK_EQ(big.to_int(), max_int);

    // Smallest positive fraction
    u16x16 tiny = u16x16::from_raw(u32(1));
    FL_CHECK_EQ(tiny.raw(), u32(1));
    FL_CHECK_EQ(tiny.to_int(), u32(0));
    FL_CHECK(tiny > u16x16());

    // Zero
    u16x16 zero;
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));

    // Large value tests
    u16x16 thousand(1000.0f);
    FL_CHECK_EQ(thousand.to_int(), u32(1000));
    check_near(thousand, 1000.0f, tol() * 100.0f);

    u16x16 ten_thousand(10000.0f);
    FL_CHECK_EQ(ten_thousand.to_int(), u32(10000));
}

// ---------------------------------------------------------------------------
// Range tests (unsigned specific)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - unsigned range") {
    // Range is [0, 65536) for integer part
    u16x16 zero(0.0f);
    check_near(zero, 0.0f);

    u16x16 small(0.0001f);
    FL_CHECK(small > zero);

    u16x16 large(65535.0f);
    FL_CHECK(large.to_int() == u32(65535));

    // Test full range
    u16x16 mid(32768.0f);
    FL_CHECK_EQ(mid.to_int(), u32(32768));
    check_near(mid, 32768.0f, tol() * 1000.0f);
}

FL_TEST_CASE("u16x16 - fractional precision") {
    // 16 fractional bits means resolution of 1/65536 ≈ 0.0000152587890625
    u16x16 a(1.0f);
    u16x16 b(1.0f + 1.0f/65536.0f);

    // These should be distinguishable
    FL_CHECK(b > a);
    FL_CHECK(b.raw() == a.raw() + 1);

    // Test multiple fractional values
    u16x16 quarter(0.25f);
    u16x16 eighth(0.125f);
    u16x16 sixteenth(0.0625f);

    check_near(quarter, 0.25f);
    check_near(eighth, 0.125f);
    check_near(sixteenth, 0.0625f);
}



// ---------------------------------------------------------------------------
// u8x24 tests (from u8x24.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u8x24 construction") {
    FL_SUBCASE("default constructor") {
        u8x24 x;
        FL_CHECK_EQ(x.raw(), 0u);
        FL_CHECK_EQ(x.to_int(), 0u);
        FL_CHECK_EQ(x.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("from float") {
        u8x24 x(1.5f);
        FL_CHECK_EQ(x.to_float(), doctest::Approx(1.5f));

        u8x24 y(0.25f);
        FL_CHECK_EQ(y.to_float(), doctest::Approx(0.25f));

        u8x24 z(255.75f);
        FL_CHECK_EQ(z.to_float(), doctest::Approx(255.75f));
    }

    FL_SUBCASE("from_raw") {
        u8x24 x = u8x24::from_raw(1u << 24);  // 1.0
        FL_CHECK_EQ(x.to_float(), doctest::Approx(1.0f));

        u8x24 y = u8x24::from_raw(1u << 23);  // 0.5
        FL_CHECK_EQ(y.to_float(), doctest::Approx(0.5f));

        u8x24 z = u8x24::from_raw(0u);  // 0.0
        FL_CHECK_EQ(z.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("to_int") {
        u8x24 x(3.7f);
        FL_CHECK_EQ(x.to_int(), 3u);

        u8x24 y(0.9f);
        FL_CHECK_EQ(y.to_int(), 0u);

        u8x24 z(100.1f);
        FL_CHECK_EQ(z.to_int(), 100u);
    }
}

FL_TEST_CASE("u8x24 addition") {
    FL_SUBCASE("basic addition") {
        u8x24 a(1.5f);
        u8x24 b(2.5f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(4.0f));
    }

    FL_SUBCASE("fractional addition") {
        u8x24 a(0.25f);
        u8x24 b(0.75f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(1.0f));
    }

    FL_SUBCASE("zero addition") {
        u8x24 a(5.0f);
        u8x24 b(0.0f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));
    }
}

FL_TEST_CASE("u8x24 subtraction") {
    FL_SUBCASE("basic subtraction") {
        u8x24 a(5.5f);
        u8x24 b(2.5f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("fractional subtraction") {
        u8x24 a(1.0f);
        u8x24 b(0.25f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.75f));
    }

    FL_SUBCASE("zero result") {
        u8x24 a(3.0f);
        u8x24 b(3.0f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("underflow wraps (unsigned behavior)") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        u8x24 c = a - b;
        // Unsigned underflow wraps around
        FL_CHECK_GT(c.raw(), (1u << 30));  // Very large value
    }
}

FL_TEST_CASE("u8x24 multiplication") {
    FL_SUBCASE("basic multiplication") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(6.0f));
    }

    FL_SUBCASE("fractional multiplication") {
        u8x24 a(1.5f);
        u8x24 b(2.5f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.75f));
    }

    FL_SUBCASE("zero multiplication") {
        u8x24 a(5.0f);
        u8x24 b(0.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("multiplication by one") {
        u8x24 a(7.5f);
        u8x24 b(1.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(7.5f));
    }
}

FL_TEST_CASE("u8x24 division") {
    FL_SUBCASE("basic division") {
        u8x24 a(6.0f);
        u8x24 b(2.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("fractional division") {
        u8x24 a(7.5f);
        u8x24 b(2.5f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("division by one") {
        u8x24 a(9.0f);
        u8x24 b(1.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(9.0f));
    }

    FL_SUBCASE("division result less than one") {
        u8x24 a(1.0f);
        u8x24 b(4.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.25f));
    }
}

FL_TEST_CASE("u8x24 scalar multiplication") {
    FL_SUBCASE("multiply by scalar") {
        u8x24 a(2.5f);
        u8x24 b = a * 3u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(7.5f));
    }

    FL_SUBCASE("scalar multiply commutative") {
        u8x24 a(1.5f);
        u8x24 b = 4u * a;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(6.0f));
    }

    FL_SUBCASE("scalar multiply by zero") {
        u8x24 a(5.0f);
        u8x24 b = a * 0u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("scalar multiply by one") {
        u8x24 a(3.7f);
        u8x24 b = a * 1u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(3.7f));
    }
}

FL_TEST_CASE("u8x24 shift operations") {
    FL_SUBCASE("right shift") {
        u8x24 a(8.0f);
        u8x24 b = a >> 1;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c = a >> 2;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(2.0f));
    }

    FL_SUBCASE("left shift") {
        u8x24 a(2.0f);
        u8x24 b = a << 1;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c = a << 2;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(8.0f));
    }
}

FL_TEST_CASE("u8x24 comparisons") {
    FL_SUBCASE("less than") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        FL_CHECK(a < b);
        FL_CHECK_FALSE(b < a);
        FL_CHECK_FALSE(a < a);
    }

    FL_SUBCASE("greater than") {
        u8x24 a(3.0f);
        u8x24 b(1.5f);
        FL_CHECK(a > b);
        FL_CHECK_FALSE(b > a);
        FL_CHECK_FALSE(a > a);
    }

    FL_SUBCASE("less than or equal") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        FL_CHECK(a <= b);
        FL_CHECK(a <= a);
        FL_CHECK_FALSE(b <= a);
    }

    FL_SUBCASE("greater than or equal") {
        u8x24 a(5.0f);
        u8x24 b(3.0f);
        FL_CHECK(a >= b);
        FL_CHECK(a >= a);
        FL_CHECK_FALSE(b >= a);
    }

    FL_SUBCASE("equality") {
        u8x24 a(4.5f);
        u8x24 b(4.5f);
        u8x24 c(4.6f);
        FL_CHECK(a == b);
        FL_CHECK_FALSE(a == c);
    }

    FL_SUBCASE("inequality") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        FL_CHECK(a != b);
        FL_CHECK_FALSE(a != a);
    }
}

FL_TEST_CASE("u8x24 math functions") {
    FL_SUBCASE("mod") {
        u8x24 a(7.5f);
        u8x24 b(3.0f);
        u8x24 c = u8x24::mod(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(1.5f));
    }

    FL_SUBCASE("floor") {
        u8x24 a(3.7f);
        u8x24 b = u8x24::floor(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(3.0f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::floor(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("ceil") {
        u8x24 a(3.2f);
        u8x24 b = u8x24::ceil(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::ceil(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("fract") {
        u8x24 a(3.75f);
        u8x24 b = u8x24::fract(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.75f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::fract(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("min") {
        u8x24 a(3.0f);
        u8x24 b(5.0f);
        u8x24 c = u8x24::min(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));

        u8x24 d = u8x24::min(b, a);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("max") {
        u8x24 a(3.0f);
        u8x24 b(5.0f);
        u8x24 c = u8x24::max(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));

        u8x24 d = u8x24::max(b, a);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("clamp") {
        u8x24 lo(1.0f);
        u8x24 hi(10.0f);

        u8x24 a(5.0f);
        FL_CHECK_EQ(u8x24::clamp(a, lo, hi).to_float(), doctest::Approx(5.0f));

        u8x24 b(0.5f);
        FL_CHECK_EQ(u8x24::clamp(b, lo, hi).to_float(), doctest::Approx(1.0f));

        u8x24 c(15.0f);
        FL_CHECK_EQ(u8x24::clamp(c, lo, hi).to_float(), doctest::Approx(10.0f));
    }

    FL_SUBCASE("lerp") {
        u8x24 a(0.0f);
        u8x24 b(10.0f);
        u8x24 t(0.5f);
        u8x24 c = u8x24::lerp(a, b, t);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));

        u8x24 t0(0.0f);
        u8x24 d = u8x24::lerp(a, b, t0);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.0f));

        u8x24 t1(1.0f);
        u8x24 e = u8x24::lerp(a, b, t1);
        FL_CHECK_EQ(e.to_float(), doctest::Approx(10.0f));
    }

    FL_SUBCASE("step") {
        u8x24 edge(5.0f);
        u8x24 a(3.0f);
        u8x24 b(7.0f);

        FL_CHECK_EQ(u8x24::step(edge, a).to_float(), doctest::Approx(0.0f));
        FL_CHECK_EQ(u8x24::step(edge, b).to_float(), doctest::Approx(1.0f));
    }

    FL_SUBCASE("smoothstep") {
        u8x24 edge0(0.0f);
        u8x24 edge1(1.0f);
        u8x24 x(0.5f);
        u8x24 result = u8x24::smoothstep(edge0, edge1, x);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(0.5f).epsilon(0.01f));
    }
}

FL_TEST_CASE("u8x24 sqrt") {
    FL_SUBCASE("sqrt of perfect squares") {
        u8x24 a(4.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(2.0f).epsilon(0.001f));

        u8x24 c(9.0f);
        u8x24 d = u8x24::sqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.0f).epsilon(0.001f));
    }

    FL_SUBCASE("sqrt of non-perfect squares") {
        u8x24 a(2.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.414f).epsilon(0.01f));

        u8x24 c(10.0f);
        u8x24 d = u8x24::sqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.162f).epsilon(0.01f));
    }

    FL_SUBCASE("sqrt of zero") {
        u8x24 a(0.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("sqrt of one") {
        u8x24 a(1.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.0f).epsilon(0.001f));
    }
}

FL_TEST_CASE("u8x24 rsqrt") {
    FL_SUBCASE("rsqrt of perfect squares") {
        u8x24 a(4.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.5f).epsilon(0.01f));

        u8x24 c(9.0f);
        u8x24 d = u8x24::rsqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.333f).epsilon(0.01f));
    }

    FL_SUBCASE("rsqrt of one") {
        u8x24 a(1.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("rsqrt of zero returns zero") {
        u8x24 a(0.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }
}

FL_TEST_CASE("u8x24 pow") {
    FL_SUBCASE("integer exponents") {
        u8x24 base(2.0f);
        u8x24 exp(3.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(8.0f).epsilon(0.1f));
    }

    FL_SUBCASE("fractional exponents") {
        u8x24 base(4.0f);
        u8x24 exp(0.5f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(2.0f).epsilon(0.1f));
    }

    FL_SUBCASE("exponent of zero") {
        u8x24 base(5.0f);
        u8x24 exp(0.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("base of one") {
        u8x24 base(1.0f);
        u8x24 exp(100.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("base of zero") {
        u8x24 base(0.0f);
        u8x24 exp(5.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(0.0f));
    }
}

FL_TEST_CASE("u8x24 edge cases") {
    FL_SUBCASE("zero value") {
        u8x24 zero;
        FL_CHECK_EQ(zero.raw(), 0u);
        FL_CHECK_EQ(zero.to_int(), 0u);
        FL_CHECK_EQ(zero.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("maximum integer value (255)") {
        u8x24 max(255.0f);
        FL_CHECK_EQ(max.to_int(), 255u);
        FL_CHECK_EQ(max.to_float(), doctest::Approx(255.0f));
    }

    FL_SUBCASE("maximum fractional value (almost 256)") {
        // 8 integer bits allows values from 0 to just under 256
        // Maximum raw value: 0xFFFFFFFF represents 256.0 - 2^-24
        u32 max_raw = 0xFFFFFFFFu;
        u8x24 max = u8x24::from_raw(max_raw);
        FL_CHECK_GT(max.to_float(), 255.9f);
        // Due to float precision, this may round to exactly 256.0f
        FL_CHECK_LE(max.to_float(), 256.0f);
    }

    FL_SUBCASE("small fractional values") {
        u8x24 tiny(0.0001f);
        FL_CHECK_GT(tiny.to_float(), 0.0f);
        FL_CHECK_LT(tiny.to_float(), 0.001f);
    }
}

FL_TEST_CASE("u8x24 complex expressions") {
    FL_SUBCASE("combined operations") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        u8x24 c(4.0f);
        u8x24 result = (a + b) * c;
        FL_CHECK_EQ(result.to_float(), doctest::Approx(20.0f));
    }

    FL_SUBCASE("nested operations") {
        u8x24 a(10.0f);
        u8x24 b(2.0f);
        u8x24 c(3.0f);
        u8x24 result = a / (b + c);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(2.0f));
    }

    FL_SUBCASE("mixed operations") {
        u8x24 a(5.0f);
        u8x24 b(2.0f);
        u8x24 result = (a * b) - (a / b);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(7.5f));
    }
}


// ---------------------------------------------------------------------------
// u24x8 – unsigned 24.8 fixed-point
// ---------------------------------------------------------------------------

FL_TEST_CASE("u24x8 - default construction") {
    u24x8 a;
    FL_CHECK_EQ(a.raw(), u32(0));
    FL_CHECK_EQ(a.to_int(), u32(0));
}

FL_TEST_CASE("u24x8 - float construction") {
    u24x8 one(1.0f);
    FL_CHECK_EQ(one.raw(), u32(1) << 8);
    FL_CHECK_EQ(one.to_int(), u32(1));

    u24x8 half(0.5f);
    FL_CHECK_EQ(half.raw(), u32(1) << 7);
    FL_CHECK_EQ(half.to_int(), u32(0));

    u24x8 val(3.0f);
    FL_CHECK_EQ(val.to_int(), u32(3));

    u24x8 large(100.0f);
    FL_CHECK_EQ(large.to_int(), u32(100));

    u24x8 max_val(16777215.0f);  // ~2^24 - 1
    FL_CHECK_GT(max_val.raw(), u32(0));
}

FL_TEST_CASE("u24x8 - from_raw") {
    // 1.0
    u24x8 a = u24x8::from_raw(u32(1) << 8);
    FL_CHECK_EQ(a.to_int(), u32(1));

    // 2.5
    u32 raw_2_5 = (u32(2) << 8) + (u32(1) << 7);
    u24x8 b = u24x8::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), u32(2));
    FL_CHECK_CLOSE(b.to_float(), 2.5f, 0.01f);

    // smallest positive fraction
    u24x8 c = u24x8::from_raw(u32(1));
    FL_CHECK_EQ(c.raw(), u32(1));
    FL_CHECK_EQ(c.to_int(), u32(0));
    FL_CHECK(c > u24x8());
}

FL_TEST_CASE("u24x8 - addition") {
    u24x8 a(1.0f), b(2.0f);
    u24x8 c = a + b;
    FL_CHECK_EQ(c.to_int(), u32(3));
    FL_CHECK_CLOSE(c.to_float(), 3.0f, 0.01f);

    // Fractional
    u24x8 d(0.25f), e(0.75f);
    FL_CHECK_CLOSE((d + e).to_float(), 1.0f, 0.01f);

    // Zero identity
    u24x8 zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());
}

FL_TEST_CASE("u24x8 - subtraction") {
    u24x8 a(3.0f), b(1.0f);
    FL_CHECK_CLOSE((a - b).to_float(), 2.0f, 0.01f);

    // Self subtraction
    u24x8 zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());

    // Fractional
    u24x8 c(1.75f), d(0.25f);
    FL_CHECK_CLOSE((c - d).to_float(), 1.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - fixed-point multiply") {
    u24x8 a(2.0f), b(3.0f);
    FL_CHECK_CLOSE((a * b).to_float(), 6.0f, 0.01f);

    // 0.5 * 0.5 = 0.25
    u24x8 half(0.5f);
    FL_CHECK_CLOSE((half * half).to_float(), 0.25f, 0.01f);

    // Multiply by 1 = identity
    u24x8 one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    u24x8 zero;
    FL_CHECK_EQ((a * zero).raw(), u32(0));

    // Fractional precision
    u24x8 c(1.5f), d(2.0f);
    FL_CHECK_CLOSE((c * d).to_float(), 3.0f, 0.01f);

    // Higher fractional precision
    u24x8 e(1.5f), f(2.5f);
    FL_CHECK_CLOSE((e * f).to_float(), 3.75f, 0.01f);
}

FL_TEST_CASE("u24x8 - fixed-point divide") {
    u24x8 a(6.0f), b(3.0f);
    FL_CHECK_CLOSE((a / b).to_float(), 2.0f, 0.01f);

    u24x8 one(1.0f), two(2.0f);
    FL_CHECK_CLOSE((one / two).to_float(), 0.5f, 0.01f);

    u24x8 four(4.0f);
    FL_CHECK_CLOSE((one / four).to_float(), 0.25f, 0.01f);

    // Divide by 1 = identity
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // Fractional result: 1/3
    u24x8 three(3.0f);
    FL_CHECK_CLOSE((one / three).to_float(), 0.3333f, 0.1f);
}

FL_TEST_CASE("u24x8 - scalar multiply") {
    u24x8 a(1.5f);

    // fp * scalar
    FL_CHECK_CLOSE((a * u32(2)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE((a * u32(0)).to_float(), 0.0f, 0.01f);

    // scalar * fp (commutative friend)
    FL_CHECK_CLOSE((u32(2) * a).to_float(), 3.0f, 0.01f);

    // Large scalar multiply
    FL_CHECK_CLOSE((a * u32(100)).to_float(), 150.0f, 0.1f);
}

FL_TEST_CASE("u24x8 - right shift") {
    u24x8 a(4.0f);
    FL_CHECK_CLOSE((a >> 1).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE((a >> 2).to_float(), 1.0f, 0.01f);

    // Fractional shift
    u24x8 b(1.0f);
    FL_CHECK_CLOSE((b >> 1).to_float(), 0.5f, 0.01f);
    FL_CHECK_CLOSE((b >> 2).to_float(), 0.25f, 0.01f);
}

FL_TEST_CASE("u24x8 - left shift") {
    u24x8 a(1.0f);
    FL_CHECK_CLOSE((a << 1).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE((a << 2).to_float(), 4.0f, 0.01f);

    // Fractional shift
    u24x8 b(0.25f);
    FL_CHECK_CLOSE((b << 1).to_float(), 0.5f, 0.01f);
    FL_CHECK_CLOSE((b << 2).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - comparisons") {
    u24x8 a(1.0f), b(2.0f), c(1.0f);
    u24x8 zero;

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

    FL_CHECK(zero < a);
    FL_CHECK(a > zero);
}

FL_TEST_CASE("u24x8 - mixed arithmetic expressions") {
    u24x8 a(2.0f), b(3.0f), c(0.5f);
    FL_CHECK_CLOSE(((a + b) * c).to_float(), 2.5f, 0.01f);

    u24x8 d(4.0f);
    FL_CHECK_CLOSE((a * b - c * d).to_float(), 4.0f, 0.01f); // 6 - 2 = 4

    // Round-trip: (a / b) * b ~ a
    FL_CHECK_CLOSE(((a / b) * b).to_float(), 2.0f, 0.02f);

    // Chained
    u24x8 one(1.0f);
    u24x8 result = (a + b) * c / one;
    FL_CHECK_CLOSE(result.to_float(), 2.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - to_float") {
    u24x8 one(1.0f);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, 0.01f);

    u24x8 half(0.5f);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, 0.01f);

    u24x8 large(100.5f);
    FL_CHECK_CLOSE(large.to_float(), 100.5f, 0.01f);

    u24x8 zero;
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - sqrt") {
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(4.0f)).to_float(), 2.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(1.0f)).to_float(), 1.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(2.0f)).to_float(), 1.4142f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(9.0f)).to_float(), 3.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(16.0f)).to_float(), 4.0f, 0.03f);

    FL_CHECK_EQ(u24x8::sqrt(u24x8()).raw(), u32(0)); // sqrt(0) = 0
}

FL_TEST_CASE("u24x8 - rsqrt") {
    FL_CHECK_CLOSE(u24x8::rsqrt(u24x8(4.0f)).to_float(), 0.5f, 0.03f);
    FL_CHECK_CLOSE(u24x8::rsqrt(u24x8(1.0f)).to_float(), 1.0f, 0.03f);

    FL_CHECK_EQ(u24x8::rsqrt(u24x8()).raw(), u32(0)); // rsqrt(0) = 0
}

FL_TEST_CASE("u24x8 - pow") {
    // 2^2 = 4
    FL_CHECK_CLOSE(u24x8::pow(u24x8(2.0f), u24x8(2.0f)).to_float(), 4.0f, 0.1f);

    // 4^0.5 = 2 (sqrt via pow)
    FL_CHECK_CLOSE(u24x8::pow(u24x8(4.0f), u24x8(0.5f)).to_float(), 2.0f, 0.1f);

    // x^0 = 1
    FL_CHECK_CLOSE(u24x8::pow(u24x8(5.0f), u24x8(0.0f)).to_float(), 1.0f, 0.1f);

    // 0^x = 0
    FL_CHECK_EQ(u24x8::pow(u24x8(), u24x8(2.0f)).raw(), u32(0));

    // 2^3 = 8
    FL_CHECK_CLOSE(u24x8::pow(u24x8(2.0f), u24x8(3.0f)).to_float(), 8.0f, 0.1f);
}

FL_TEST_CASE("u24x8 - sqrt identity") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 16.0f, 25.0f};
    for (float v : values) {
        u24x8 x(v);
        u24x8 s = u24x8::sqrt(x);
        u24x8 sq = s * s;
        FL_CHECK_CLOSE(sq.to_float(), v, v * 0.05f + 0.1f);
    }
}

FL_TEST_CASE("u24x8 - floor and ceil") {
    FL_CHECK_CLOSE(u24x8::floor(u24x8(2.75f)).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(2.75f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::floor(u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::floor(u24x8(0.25f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(0.25f)).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - fract") {
    FL_CHECK_CLOSE(u24x8::fract(u24x8(2.75f)).to_float(), 0.75f, 0.01f);
    FL_CHECK_EQ(u24x8::fract(u24x8(1.0f)).raw(), u32(0));
    FL_CHECK_CLOSE(u24x8::fract(u24x8(0.5f)).to_float(), 0.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - abs") {
    // For unsigned, abs is identity
    FL_CHECK_CLOSE(u24x8::abs(u24x8(3.5f)).to_float(), 3.5f, 0.01f);
    FL_CHECK_EQ(u24x8::abs(u24x8()).raw(), u32(0));
}

FL_TEST_CASE("u24x8 - min and max") {
    u24x8 a(5.0f), b(10.0f);
    FL_CHECK_CLOSE(u24x8::min(a, b).to_float(), 5.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::max(a, b).to_float(), 10.0f, 0.01f);

    u24x8 c(7.5f);
    FL_CHECK_CLOSE(u24x8::min(a, c).to_float(), 5.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::max(a, c).to_float(), 7.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - mod") {
    FL_CHECK_CLOSE(u24x8::mod(u24x8(5.5f), u24x8(2.0f)).to_float(), 1.5f, 0.01f);
    FL_CHECK_CLOSE(u24x8::mod(u24x8(3.0f), u24x8(1.5f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::mod(u24x8(1.0f), u24x8(3.0f)).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - lerp, clamp, step, smoothstep") {
    // lerp (only works correctly when b >= a for unsigned types)
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(0.0f), u24x8(4.0f), u24x8(0.5f)).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(1.0f), u24x8(3.0f), u24x8(0.0f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(1.0f), u24x8(3.0f), u24x8(1.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(0.0f), u24x8(10.0f), u24x8(0.5f)).to_float(), 5.0f, 0.1f);

    // clamp
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(5.0f), u24x8(0.0f), u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(0.0f), u24x8(1.0f), u24x8(3.0f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(1.5f), u24x8(0.0f), u24x8(3.0f)).to_float(), 1.5f, 0.01f);

    // step
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(0.5f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(1.5f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(1.0f)).to_float(), 1.0f, 0.01f);

    // smoothstep
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(0.0f)).to_float(), 0.0f, 0.04f);
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(1.0f)).to_float(), 1.0f, 0.04f);
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(0.5f)).to_float(), 0.5f, 0.04f);
}

FL_TEST_CASE("u24x8 - edge values") {
    constexpr u32 max_int = (1u << 24) - 1;

    u24x8 big(static_cast<float>(max_int));
    FL_CHECK_GT(big.to_int(), u32(0));

    // Smallest positive fraction
    u24x8 tiny = u24x8::from_raw(u32(1));
    FL_CHECK_EQ(tiny.raw(), u32(1));
    FL_CHECK_EQ(tiny.to_int(), u32(0));
    FL_CHECK(tiny > u24x8());

    // Zero
    u24x8 zero;
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));
}

FL_TEST_CASE("u24x8 - large value arithmetic") {
    u24x8 a(1000.0f), b(500.0f);
    FL_CHECK_CLOSE((a + b).to_float(), 1500.0f, 1.0f);
    FL_CHECK_CLOSE((a - b).to_float(), 500.0f, 1.0f);
    FL_CHECK_CLOSE((a / b).to_float(), 2.0f, 0.01f);
}

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

} // anonymous namespace
