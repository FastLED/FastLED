#pragma once

#include "fl/clamp.h"
#include "fl/math_macros.h"

namespace fl {

// Forward declarations of implementation functions defined in math.cpp
float floor_impl_float(float value);
double floor_impl_double(double value);
float ceil_impl_float(float value);
double ceil_impl_double(double value);
float exp_impl_float(float value);
double exp_impl_double(double value);
float sqrt_impl_float(float value);
double sqrt_impl_double(double value);
float sin_impl_float(float value);
double sin_impl_double(double value);
float cos_impl_float(float value);
double cos_impl_double(double value);
float log_impl_float(float value);
double log_impl_double(double value);
float log10_impl_float(float value);
double log10_impl_double(double value);
float pow_impl_float(float base, float exponent);
double pow_impl_double(double base, double exponent);
float fabs_impl_float(float value);
double fabs_impl_double(double value);
long lround_impl_float(float value);
long lround_impl_double(double value);
float fmod_impl_float(float x, float y);
double fmod_impl_double(double x, double y);

template <typename T> inline T floor(T value) {
    if (value >= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(floor_impl_float(static_cast<float>(value)));
}

template <typename T> inline T ceil(T value) {
    if (value <= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(ceil_impl_float(static_cast<float>(value)));
}

// Exponential function using custom implementation
template <typename T> inline T exp(T value) {
    return static_cast<T>(exp_impl_double(static_cast<double>(value)));
}

// Square root using Newton-Raphson method
template <typename T> inline T sqrt(T value) {
    return static_cast<T>(sqrt_impl_float(static_cast<float>(value)));
}

// Constexpr version for compile-time evaluation (compatible with older C++
// standards)
constexpr int ceil_constexpr(float value) {
    return static_cast<int>((value > static_cast<float>(static_cast<int>(value)))
                                ? static_cast<int>(value) + 1
                                : static_cast<int>(value));
}

// Explicit float and double versions for all math functions
// Following standard C naming: 'f' suffix for float, no suffix for double

// Floor functions
inline float floorf(float value) { return floor_impl_float(value); }
inline double floor(double value) { return floor_impl_double(value); }

// Ceiling functions
inline float ceilf(float value) { return ceil_impl_float(value); }
inline double ceil(double value) { return ceil_impl_double(value); }

// Square root functions
inline float sqrtf(float value) { return sqrt_impl_float(value); }
inline double sqrt(double value) { return sqrt_impl_double(value); }

// Exponential functions
inline float expf(float value) { return exp_impl_float(value); }
inline double exp(double value) { return exp_impl_double(value); }

// Sine functions
inline float sinf(float value) { return sin_impl_float(value); }
inline double sin(double value) { return sin_impl_double(value); }

// Cosine functions
inline float cosf(float value) { return cos_impl_float(value); }
inline double cos(double value) { return cos_impl_double(value); }

// Natural logarithm functions
inline float logf(float value) { return log_impl_float(value); }
inline double log(double value) { return log_impl_double(value); }

// Base-10 logarithm functions
inline float log10f(float value) { return log10_impl_float(value); }
inline double log10(double value) { return log10_impl_double(value); }

// Power functions
inline float powf(float base, float exponent) { return pow_impl_float(base, exponent); }
inline double pow(double base, double exponent) { return pow_impl_double(base, exponent); }

// Absolute value functions (floating point)
inline float fabsf(float value) { return fabs_impl_float(value); }
inline double fabs(double value) { return fabs_impl_double(value); }

// Round to nearest long integer
inline long lroundf(float value) { return lround_impl_float(value); }
inline long lround(double value) { return lround_impl_double(value); }

// Floating-point modulo (remainder)
inline float fmodf(float x, float y) { return fmod_impl_float(x, y); }
inline double fmod(double x, double y) { return fmod_impl_double(x, y); }

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
