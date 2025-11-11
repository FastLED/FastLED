#pragma once

#include "fl/clamp.h"
#include "fl/math_macros.h"
#include "fl/undef.h"

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
float round_impl_float(float value);
double round_impl_double(double value);
float fmod_impl_float(float x, float y);
double fmod_impl_double(double x, double y);
float atan2_impl_float(float y, float x);
double atan2_impl_double(double y, double x);
float hypot_impl_float(float x, float y);
double hypot_impl_double(double x, double y);
float atan_impl_float(float value);
double atan_impl_double(double value);
float asin_impl_float(float value);
double asin_impl_double(double value);
float acos_impl_float(float value);
double acos_impl_double(double value);
float tan_impl_float(float value);
double tan_impl_double(double value);

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
template<typename T> inline T floor(T value) { return floor_impl_float(value); }

// Ceiling functions
inline float ceilf(float value) { return ceil_impl_float(value); }
inline double ceil(double value) { return ceil_impl_double(value); }
template<typename T> inline T ceil(T value) { return ceil_impl_float(value); }

// Square root functions
inline float sqrtf(float value) { return sqrt_impl_float(value); }
inline double sqrt(double value) { return sqrt_impl_double(value); }
template<typename T> inline T sqrt(T value) { return sqrt_impl_float(value); }

// Exponential functions
inline float expf(float value) { return exp_impl_float(value); }
inline double exp(double value) { return exp_impl_double(value); }
template<typename T> inline T exp(T value) { return exp_impl_float(value); }

// Sine functions
inline float sinf(float value) { return sin_impl_float(value); }
inline double sin(double value) { return sin_impl_double(value); }
template<typename T> inline T sin(T value) { return sin_impl_float(value); }

// Cosine functions
inline float cosf(float value) { return cos_impl_float(value); }
inline double cos(double value) { return cos_impl_double(value); }
template<typename T> inline T cos(T value) { return cos_impl_float(value); }

// Natural logarithm functions
inline float logf(float value) { return log_impl_float(value); }
inline double log(double value) { return log_impl_double(value); }
template<typename T> inline T log(T value) { return log_impl_float(value); }

// Base-10 logarithm functions
inline float log10f(float value) { return log10_impl_float(value); }
inline double log10(double value) { return log10_impl_double(value); }
template<typename T> inline T log10(T value) { return log10_impl_float(value); }

// Base-2 logarithm functions (implemented using natural log)
inline float log2f(float value) { return log_impl_float(value) / log_impl_float(2.0f); }
inline double log2(double value) { return log_impl_double(value) / log_impl_double(2.0); }
template<typename T> inline T log2(T value) { return log_impl_float(value) / log_impl_float(2.0f); }

// Power functions
inline float powf(float base, float exponent) { return pow_impl_float(base, exponent); }
inline double pow(double base, double exponent) { return pow_impl_double(base, exponent); }
template<typename T> inline T pow(T base, T exponent) { return pow_impl_float(base, exponent); }

// Absolute value functions (floating point)
inline float fabsf(float value) { return fabs_impl_float(value); }
inline double fabs(double value) { return fabs_impl_double(value); }
template<typename T> inline T fabs(T value) { return fabs_impl_float(value); }

// Round to nearest long integer
inline long lroundf(float value) { return lround_impl_float(value); }
inline long lround(double value) { return lround_impl_double(value); }
template<typename T> inline long lround(T value) { return lround_impl_float(value); }

// Round to nearest floating-point value
// Arduino defines round as a macro, so we need to undefine it first
inline float roundf(float value) { return round_impl_float(value); }
// Template overload for exact type matching - avoids ambiguity with ::round
// when using "using fl::round;" and calling round with float/double arguments
// Template has higher priority than implicit float->double conversion
template<typename T>
inline T round(T value) {
    // Delegate to float implementation for all types
    // This will only be instantiated for types other than double (double uses specialized template below)
    return static_cast<T>(round_impl_float(static_cast<float>(value)));
}
// Explicit specialization for double to use double implementation
template<>
inline double round<double>(double value) {
    return round_impl_double(value);
}

// Floating-point modulo (remainder)
inline float fmodf(float x, float y) { return fmod_impl_float(x, y); }
inline double fmod(double x, double y) { return fmod_impl_double(x, y); }
// Template overload for exact type matching - avoids ambiguity with ::fmod
// when using "using fl::fmod;" and calling fmod with float arguments
// Template has higher priority than implicit float->double conversion
template<typename T>
inline T fmod(T x, T y) {
    // Delegate to float or double implementation based on type
    // This will only be instantiated for float (double uses the non-template overload above)
    return fmod_impl_float(static_cast<float>(x), static_cast<float>(y));
}

// Inverse tangent functions (atan2)
inline float atan2f(float y, float x) { return atan2_impl_float(y, x); }
inline double atan2(double y, double x) { return atan2_impl_double(y, x); }
template<typename T> inline T atan2(T y, T x) { return atan2_impl_float(y, x); }

// Hypotenuse calculation
inline float hypotf(float x, float y) { return hypot_impl_float(x, y); }
inline double hypot(double x, double y) { return hypot_impl_double(x, y); }
template<typename T> inline T hypot(T x, T y) { return hypot_impl_float(x, y); }

// Inverse tangent functions (atan)
inline float atanf(float value) { return atan_impl_float(value); }
inline double atan(double value) { return atan_impl_double(value); }
template<typename T> inline T atan(T value) { return atan_impl_float(value); }

// Inverse sine functions (asin)
inline float asinf(float value) { return asin_impl_float(value); }
inline double asin(double value) { return asin_impl_double(value); }
template<typename T> inline T asin(T value) { return asin_impl_float(value); }

// Inverse cosine functions (acos)
inline float acosf(float value) { return acos_impl_float(value); }
inline double acos(double value) { return acos_impl_double(value); }
template<typename T> inline T acos(T value) { return acos_impl_float(value); }

// Tangent functions (tan)
inline float tanf(float value) { return tan_impl_float(value); }
inline double tan(double value) { return tan_impl_double(value); }
template<typename T> inline T tan(T value) { return tan_impl_float(value); }

} // namespace fl
