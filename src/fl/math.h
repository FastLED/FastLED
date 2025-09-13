#pragma once

#include "fl/has_include.h"

// Include math headers with better ESP32C2 compatibility
#ifndef FASTLED_HAS_EXP
#if FL_HAS_INCLUDE(<cmath>)
  #define FASTLED_HAS_EXP 1
  #include <cmath>  // ok include
#elif FL_HAS_INCLUDE(<math.h>)
  #define FASTLED_HAS_EXP 1
  #include <math.h>  // ok include
#else
  #define FASTLED_HAS_EXP 0
#endif
#endif  // !FASTLED_HAS_EXP


#include "fl/clamp.h"
#include "fl/map_range.h"
#include "fl/math_macros.h"

namespace fl {

template <typename T> inline T floor(T value) {
    if (value >= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::floor(static_cast<float>(value)));
}

template <typename T> inline T ceil(T value) {
    if (value <= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::ceil(static_cast<float>(value)));
}

// Exponential function - binds to standard library exp if available
template <typename T> inline T exp(T value) {
#if FASTLED_HAS_EXP
    return static_cast<T>(::exp(static_cast<double>(value)));
#else
    // Fallback implementation using Taylor series approximation
    // e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + x⁵/5! + ...
    // This is a simple approximation for small values
    double x = static_cast<double>(value);
    if (x > 10.0)
        return static_cast<T>(22026.465794806718); // e^10 approx
    if (x < -10.0)
        return static_cast<T>(0.0000453999297625); // e^-10 approx

    double result = 1.0;
    double term = 1.0;
    for (int i = 1; i < 10; ++i) {
        term *= x / i;
        result += term;
    }
    return static_cast<T>(result);
#endif
}

// Constexpr version for compile-time evaluation (compatible with older C++
// standards)
constexpr int ceil_constexpr(float value) {
    return static_cast<int>((value > static_cast<float>(static_cast<int>(value)))
                                ? static_cast<int>(value) + 1
                                : static_cast<int>(value));
}

// Arduino will define this in the global namespace as macros, so we can't
// define them ourselves.
// template <typename T>
// inline T abs(T value) {
//     return (value < 0) ? -value : value;
// }

// template <typename T>
// inline T min(T a, T b) {
//     return (a < b) ? a : b;
// }

// template <typename T>
// inline T max(T a, T b) {
//     return (a > b) ? a : b;
// }

} // namespace fl
