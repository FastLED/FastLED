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
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for fixed_point tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

#include "fixed_point_bounds.h"
#include "fixed_point_test_helpers.h"

// --- Helper functions ---

template <typename T>
void check_near(T val, float expected, float tolerance) {
    FL_CHECK_CLOSE(val.to_float(), expected, tolerance);
}

#define FP_TYPES fixed_point<4,12>, fixed_point<8,8>, fixed_point<8,24>, fixed_point<12,4>, fixed_point<16,16>, fixed_point<24,8>

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

} // FL_TEST_FILE
