#pragma once

/// @file fl/stl/ratio.h
/// @brief FastLED ratio implementation - compile-time rational arithmetic
///
/// This header provides ratio types for representing compile-time rational numbers,
/// used primarily for duration period types in fl::chrono.

#include "fl/int.h"

namespace fl {

/// @brief Compile-time rational arithmetic
/// @tparam Num Numerator
/// @tparam Denom Denominator (must be non-zero)
///
/// Represents a compile-time rational number Num/Denom.
/// Used for duration period types (e.g., milliseconds = 1/1000 second).
template<fl::i64 Num, fl::i64 Denom = 1>
struct ratio {
    static_assert(Denom != 0, "ratio denominator cannot be zero");

    static constexpr fl::i64 num = Num;
    static constexpr fl::i64 den = Denom;
};

/// @brief Divide two ratios at compile time
/// @tparam R1 First ratio
/// @tparam R2 Second ratio
///
/// Computes R1 / R2 = (R1::num * R2::den) / (R1::den * R2::num)
template<typename R1, typename R2>
using ratio_divide = ratio<
    R1::num * R2::den,
    R1::den * R2::num
>;

/// @brief Multiply two ratios at compile time
/// @tparam R1 First ratio
/// @tparam R2 Second ratio
///
/// Computes R1 * R2 = (R1::num * R2::num) / (R1::den * R2::den)
template<typename R1, typename R2>
using ratio_multiply = ratio<
    R1::num * R2::num,
    R1::den * R2::den
>;

// Common SI ratio types
using nano  = ratio<1, 1000000000>;     ///< 1/1,000,000,000
using micro = ratio<1, 1000000>;         ///< 1/1,000,000
using milli = ratio<1, 1000>;            ///< 1/1,000
using centi = ratio<1, 100>;             ///< 1/100
using deci  = ratio<1, 10>;              ///< 1/10
using deca  = ratio<10, 1>;              ///< 10/1
using hecto = ratio<100, 1>;             ///< 100/1
using kilo  = ratio<1000, 1>;            ///< 1,000/1
using mega  = ratio<1000000, 1>;         ///< 1,000,000/1
using giga  = ratio<1000000000, 1>;      ///< 1,000,000,000/1

} // namespace fl
