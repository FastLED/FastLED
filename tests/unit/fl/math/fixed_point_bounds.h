#pragma once

// ============================================================================
// Type-Specific Accuracy Bounds (Template Specializations)
// ============================================================================
// Measured accuracy values × 1.2 safety margin for regression detection.
// Measured from sweep tests across function input ranges.
//
// Usage: include this header from WITHIN each test file's anonymous namespace,
// so each translation unit gets its own private copy of the bounds table.
// This avoids ODR concerns with the out-of-line constexpr definitions, which
// remain required for C++11/C++14 compatibility on this codebase.
//
// Example:
//   FL_TEST_FILE(FL_FILEPATH) {
//   using namespace fl;
//   namespace {
//   #include "fixed_point_bounds.h"
//   // ... tests that use accuracy_bounds<T>::sin_max etc ...
//   } // anonymous namespace
//   }
//
// Extracted from tests/fl/math/fixed_point.cpp as part of #3127 / #3130.

template<typename T> struct accuracy_bounds;

template<> struct accuracy_bounds<fl::s4x12> {
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

template<> struct accuracy_bounds<fl::s8x8> {
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

template<> struct accuracy_bounds<fl::s8x24> {
    // Measured: sin=0.000031 atan=0.000288 sqrt=0.000001
    //           pow(x,0.5)=0.000497 pow(x,2.0)=0.009703 pow(x,3.0)=0.0534172
    //           smooth=0.000000
    static constexpr float sin_max = 0.00004f;      // measured: 0.000031
    static constexpr float cos_max = 0.00004f;      // measured: ~0.000031
    static constexpr float atan_max = 0.00035f;     // measured: 0.000288
    static constexpr float atan2_max = 0.00035f;    // measured: 0.000288
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

template<> struct accuracy_bounds<fl::s12x4> {
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

template<> struct accuracy_bounds<fl::s16x16> {
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

template<> struct accuracy_bounds<fl::s24x8> {
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
constexpr float accuracy_bounds<fl::s4x12>::sin_max;
constexpr float accuracy_bounds<fl::s4x12>::cos_max;
constexpr float accuracy_bounds<fl::s4x12>::atan_max;
constexpr float accuracy_bounds<fl::s4x12>::atan2_max;
constexpr float accuracy_bounds<fl::s4x12>::asin_max;
constexpr float accuracy_bounds<fl::s4x12>::acos_max;
constexpr float accuracy_bounds<fl::s4x12>::sqrt_max;
constexpr float accuracy_bounds<fl::s4x12>::rsqrt_max;
constexpr float accuracy_bounds<fl::s4x12>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s4x12>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s4x12>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s4x12>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s4x12>::smoothstep_max;

constexpr float accuracy_bounds<fl::s8x8>::sin_max;
constexpr float accuracy_bounds<fl::s8x8>::cos_max;
constexpr float accuracy_bounds<fl::s8x8>::atan_max;
constexpr float accuracy_bounds<fl::s8x8>::atan2_max;
constexpr float accuracy_bounds<fl::s8x8>::asin_max;
constexpr float accuracy_bounds<fl::s8x8>::acos_max;
constexpr float accuracy_bounds<fl::s8x8>::sqrt_max;
constexpr float accuracy_bounds<fl::s8x8>::rsqrt_max;
constexpr float accuracy_bounds<fl::s8x8>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s8x8>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s8x8>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s8x8>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s8x8>::smoothstep_max;

constexpr float accuracy_bounds<fl::s8x24>::sin_max;
constexpr float accuracy_bounds<fl::s8x24>::cos_max;
constexpr float accuracy_bounds<fl::s8x24>::atan_max;
constexpr float accuracy_bounds<fl::s8x24>::atan2_max;
constexpr float accuracy_bounds<fl::s8x24>::asin_max;
constexpr float accuracy_bounds<fl::s8x24>::acos_max;
constexpr float accuracy_bounds<fl::s8x24>::sqrt_max;
constexpr float accuracy_bounds<fl::s8x24>::rsqrt_max;
constexpr float accuracy_bounds<fl::s8x24>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s8x24>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s8x24>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s8x24>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s8x24>::smoothstep_max;

constexpr float accuracy_bounds<fl::s12x4>::sin_max;
constexpr float accuracy_bounds<fl::s12x4>::cos_max;
constexpr float accuracy_bounds<fl::s12x4>::atan_max;
constexpr float accuracy_bounds<fl::s12x4>::atan2_max;
constexpr float accuracy_bounds<fl::s12x4>::asin_max;
constexpr float accuracy_bounds<fl::s12x4>::acos_max;
constexpr float accuracy_bounds<fl::s12x4>::sqrt_max;
constexpr float accuracy_bounds<fl::s12x4>::rsqrt_max;
constexpr float accuracy_bounds<fl::s12x4>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s12x4>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s12x4>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s12x4>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s12x4>::smoothstep_max;

constexpr float accuracy_bounds<fl::s16x16>::sin_max;
constexpr float accuracy_bounds<fl::s16x16>::cos_max;
constexpr float accuracy_bounds<fl::s16x16>::atan_max;
constexpr float accuracy_bounds<fl::s16x16>::atan2_max;
constexpr float accuracy_bounds<fl::s16x16>::asin_max;
constexpr float accuracy_bounds<fl::s16x16>::acos_max;
constexpr float accuracy_bounds<fl::s16x16>::sqrt_max;
constexpr float accuracy_bounds<fl::s16x16>::rsqrt_max;
constexpr float accuracy_bounds<fl::s16x16>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s16x16>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s16x16>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s16x16>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s16x16>::smoothstep_max;

constexpr float accuracy_bounds<fl::s24x8>::sin_max;
constexpr float accuracy_bounds<fl::s24x8>::cos_max;
constexpr float accuracy_bounds<fl::s24x8>::atan_max;
constexpr float accuracy_bounds<fl::s24x8>::atan2_max;
constexpr float accuracy_bounds<fl::s24x8>::asin_max;
constexpr float accuracy_bounds<fl::s24x8>::acos_max;
constexpr float accuracy_bounds<fl::s24x8>::sqrt_max;
constexpr float accuracy_bounds<fl::s24x8>::rsqrt_max;
constexpr float accuracy_bounds<fl::s24x8>::pow_x_0_5_max;
constexpr float accuracy_bounds<fl::s24x8>::pow_x_0_6_max;
constexpr float accuracy_bounds<fl::s24x8>::pow_x_2_0_max;
constexpr float accuracy_bounds<fl::s24x8>::pow_x_3_0_max;
constexpr float accuracy_bounds<fl::s24x8>::smoothstep_max;
