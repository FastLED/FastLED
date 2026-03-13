#pragma once

// Centralized math functions for FastLED.
//
// All math lives here: basic ops (min/max/abs/clamp), transcendentals
// (sin/cos/exp/log/pow/sqrt), rounding, angle conversion, and constants.
//
// Type dispatch:
//   float        — explicit f-suffix overloads (sinf, cosf, etc.)
//   double       — explicit non-template overloads (sin, cos, etc.)
//   integral     — template promotes to float, returns float
//   fixed-point  — overloads in fl/stl/fixed_point.h (SFINAE-guarded)
//
// Backward-compatible headers fl/math_macros.h and fl/clamp.h still exist
// and forward here.

#include "fl/stl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/undef.h"  // IWYU pragma: keep

// ===== Constants =============================================================

#ifndef FL_PI
#define FL_PI 3.1415926535897932384626433832795
#endif

#ifndef FL_E
#define FL_E 2.71828182845904523536
#endif

#ifndef FL_M_PI
#define FL_M_PI FL_PI
#endif

#ifndef FL_EPSILON_F
#define FL_EPSILON_F 1.19209290e-07F
#endif

#ifndef FL_EPSILON_D
#define FL_EPSILON_D 2.2204460492503131e-16
#endif

#ifndef FL_INFINITY_FLOAT
#define FL_INFINITY_FLOAT (1.0f / 0.0f)
#endif

#ifndef FL_INFINITY_DOUBLE
#define FL_INFINITY_DOUBLE (1.0 / 0.0)
#endif

#ifndef FL_FLT_MAX
#define FL_FLT_MAX 3.402823466e+38F
#endif

namespace fl {

// ===== Basic: min, max, abs, clamp ===========================================

template <typename T> constexpr inline T abs(T value) {
    return value < 0 ? -value : value;
}

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(sign-compare)
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

template <typename T, typename U> constexpr inline common_type_t<T, U> min(T a, U b) {
    return (a < b) ? a : b;
}

template <typename T, typename U> constexpr inline common_type_t<T, U> max(T a, U b) {
    return (a > b) ? a : b;
}

FL_DISABLE_WARNING_POP

template <typename T> FASTLED_FORCE_INLINE T clamp(T value, T lo, T hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

// ===== Approximate equality ===================================================

template <typename T, typename U>
constexpr inline bool almost_equal(T a, T b, U tolerance) {
    return fl::abs(a - b) < tolerance;
}

inline bool almost_equal(float a, float b) {
    return fl::abs(a - b) < FL_EPSILON_F;
}

inline bool almost_equal(double a, double b) {
    return fl::abs(a - b) < FL_EPSILON_F;
}

// ===== Legacy macros (prefer fl:: functions directly) ========================

#ifndef FL_MAX
#define FL_MAX(a, b) fl::max(a, b)
#endif

#ifndef FL_MIN
#define FL_MIN(a, b) fl::min(a, b)
#endif

#ifndef FL_ABS
#define FL_ABS(x) fl::abs(x)
#endif

#ifndef FL_ALMOST_EQUAL
#define FL_ALMOST_EQUAL(a, b, small) fl::almost_equal(a, b, small)
#endif

#ifndef FL_ALMOST_EQUAL_FLOAT
#define FL_ALMOST_EQUAL_FLOAT(a, b) fl::almost_equal(static_cast<float>(a), static_cast<float>(b))
#endif

#ifndef FL_ALMOST_EQUAL_EPSILON
#define FL_ALMOST_EQUAL_EPSILON(a, b, epsilon) fl::almost_equal(a, b, epsilon)
#endif

#ifndef FL_ALMOST_EQUAL_DOUBLE
#define FL_ALMOST_EQUAL_DOUBLE(a, b) fl::almost_equal(static_cast<double>(a), static_cast<double>(b))
#endif

// ===== Map range =============================================================

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)
FL_DISABLE_WARNING(double-promotion)
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION

namespace map_range_detail {

// Primary template for map_range_math
template <typename T, typename U> struct map_range_math {
    static U map(T value, T in_min, T in_max, U out_min, U out_max) {
        if (in_min == in_max)
            return out_min;
        return out_min +
               (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }
};

// Specialization for u8 -> u8
template <> struct map_range_math<u8, u8> {
    static u8 map(u8 value, u8 in_min, u8 in_max,
                       u8 out_min, u8 out_max);
};

// Specialization for u16 -> u16
template <> struct map_range_math<u16, u16> {
    static u16 map(u16 value, u16 in_min, u16 in_max,
                        u16 out_min, u16 out_max);
};

// Equality comparison helpers
template <typename T> bool equals(T a, T b) { return a == b; }
inline bool equals(float a, float b) { return fl::almost_equal(a, b); }
inline bool equals(double d, double d2) { return fl::almost_equal(d, d2); }

} // namespace map_range_detail

template <typename T, typename U>
FASTLED_FORCE_INLINE U map_range(T value, T in_min, T in_max, U out_min,
                                 U out_max) {
    if (map_range_detail::equals(value, in_min)) {
        return out_min;
    }
    if (map_range_detail::equals(value, in_max)) {
        return out_max;
    }
    return map_range_detail::map_range_math<T, U>::map(value, in_min, in_max, out_min, out_max);
}

template <typename T, typename U>
FASTLED_FORCE_INLINE U map_range_clamped(T value, T in_min, T in_max, U out_min,
                                         U out_max) {
    value = clamp(value, in_min, in_max);
    return map_range<T, U>(value, in_min, in_max, out_min, out_max);
}

namespace map_range_detail {

inline u8 map_range_math<u8, u8>::map(u8 value, u8 in_min, u8 in_max,
                   u8 out_min, u8 out_max) {
    if (value == in_min) {
        return out_min;
    }
    if (value == in_max) {
        return out_max;
    }
    i16 v16 = value;
    i16 in_min16 = in_min;
    i16 in_max16 = in_max;
    i16 out_min16 = out_min;
    i16 out_max16 = out_max;
    i16 out16 = fl::map_range<i16, i16>(v16, in_min16, in_max16,
                                                  out_min16, out_max16);
    if (out16 < 0) {
        out16 = 0;
    } else if (out16 > 255) {
        out16 = 255;
    }
    return static_cast<u8>(out16);
}

inline u16 map_range_math<u16, u16>::map(u16 value, u16 in_min, u16 in_max,
                    u16 out_min, u16 out_max) {
    if (value == in_min) {
        return out_min;
    }
    if (value == in_max) {
        return out_max;
    }
    u32 v32 = value;
    u32 in_min32 = in_min;
    u32 in_max32 = in_max;
    u32 out_min32 = out_min;
    u32 out_max32 = out_max;
    u32 out32 = fl::map_range<u32, u32>(v32, in_min32, in_max32,
                                                  out_min32, out_max32);
    if (out32 > 65535) {
        out32 = 65535;
    }
    return static_cast<u16>(out32);
}

} // namespace map_range_detail

FL_DISABLE_WARNING_POP

// ===== Forward declarations (implementations in math.cpp.hpp) ================

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
float ldexp_impl_float(float value, int exp);
double ldexp_impl_double(double value, int exp);

// ===== Constexpr helpers =====================================================

constexpr int ceil_constexpr(float value) {
    return static_cast<int>((value > static_cast<float>(static_cast<int>(value)))
                                ? static_cast<int>(value) + 1
                                : static_cast<int>(value));
}

// ===== Rounding ==============================================================
//
// Pattern for each function:
//   1. Explicit float version (f-suffix, e.g. floorf)
//   2. Explicit double version (no suffix, e.g. floor)
//   3. Integral promotion template — accepts integer types, returns float
//
// Fixed-point overloads live in fl/stl/fixed_point.h and are guarded by
// enable_if<is_fixed_point<T>>, so there is no ambiguity.

// floor
inline float floorf(float value) { return floor_impl_float(value); }
inline double floor(double value) { return floor_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
floor(T value) { return floor_impl_float(static_cast<float>(value)); }

// ceil
inline float ceilf(float value) { return ceil_impl_float(value); }
inline double ceil(double value) { return ceil_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
ceil(T value) { return ceil_impl_float(static_cast<float>(value)); }

// round — template form avoids "using fl::round" conflict with ::round
inline float roundf(float value) { return round_impl_float(value); }
template<typename T>
inline typename enable_if<!is_integral<T>::value, T>::type
round(T value) {
    return static_cast<T>(round_impl_float(static_cast<float>(value)));
}
template<>
inline double round<double>(double value) {
    return round_impl_double(value);
}
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
round(T value) { return static_cast<float>(value); }

// lround
inline long lroundf(float value) { return lround_impl_float(value); }
inline long lround(double value) { return lround_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, long>::type
lround(T value) { return static_cast<long>(value); }

// fmod — template form avoids "using fl::fmod" conflict with ::fmod
inline float fmodf(float x, float y) { return fmod_impl_float(x, y); }
template<typename T>
inline typename enable_if<!is_integral<T>::value, T>::type
fmod(T x, T y) {
    return static_cast<T>(fmod_impl_float(static_cast<float>(x), static_cast<float>(y)));
}
template<>
inline double fmod<double>(double x, double y) {
    return fmod_impl_double(x, y);
}
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
fmod(T x, T y) { return fmod_impl_float(static_cast<float>(x), static_cast<float>(y)); }

// ===== Trigonometry ==========================================================

// sin
inline float sinf(float value) { return sin_impl_float(value); }
inline double sin(double value) { return sin_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
sin(T value) { return sin_impl_float(static_cast<float>(value)); }

// cos
inline float cosf(float value) { return cos_impl_float(value); }
inline double cos(double value) { return cos_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
cos(T value) { return cos_impl_float(static_cast<float>(value)); }

// tan
inline float tanf(float value) { return tan_impl_float(value); }
inline double tan(double value) { return tan_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
tan(T value) { return tan_impl_float(static_cast<float>(value)); }

// ===== Inverse trigonometry ==================================================

// asin
inline float asinf(float value) { return asin_impl_float(value); }
inline double asin(double value) { return asin_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
asin(T value) { return asin_impl_float(static_cast<float>(value)); }

// acos
inline float acosf(float value) { return acos_impl_float(value); }
inline double acos(double value) { return acos_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
acos(T value) { return acos_impl_float(static_cast<float>(value)); }

// atan
inline float atanf(float value) { return atan_impl_float(value); }
inline double atan(double value) { return atan_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
atan(T value) { return atan_impl_float(static_cast<float>(value)); }

// atan2
inline float atan2f(float y, float x) { return atan2_impl_float(y, x); }
inline double atan2(double y, double x) { return atan2_impl_double(y, x); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
atan2(T y, T x) { return atan2_impl_float(static_cast<float>(y), static_cast<float>(x)); }

// ===== Exponential / logarithmic =============================================

// exp
inline float expf(float value) { return exp_impl_float(value); }
inline double exp(double value) { return exp_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
exp(T value) { return exp_impl_float(static_cast<float>(value)); }

// log (natural)
inline float logf(float value) { return log_impl_float(value); }
inline double log(double value) { return log_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
log(T value) { return log_impl_float(static_cast<float>(value)); }

// log10
inline float log10f(float value) { return log10_impl_float(value); }
inline double log10(double value) { return log10_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
log10(T value) { return log10_impl_float(static_cast<float>(value)); }

// log2
inline float log2f(float value) { return log_impl_float(value) / log_impl_float(2.0f); }
inline double log2(double value) { return log_impl_double(value) / log_impl_double(2.0); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
log2(T value) { return log_impl_float(static_cast<float>(value)) / log_impl_float(2.0f); }

// pow
inline float powf(float base, float exponent) { return pow_impl_float(base, exponent); }
inline double pow(double base, double exponent) { return pow_impl_double(base, exponent); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
pow(T base, T exponent) { return pow_impl_float(static_cast<float>(base), static_cast<float>(exponent)); }

// ===== Roots / distance ======================================================

namespace sqrt_detail {
// Type trait to detect if T has a static sqrt method
template<typename T, typename = void>
struct has_static_sqrt : false_type {};

template<typename T>
struct has_static_sqrt<T, decltype(static_cast<void>(T::sqrt(declval<T>())))> : true_type {};
}

// sqrt
inline float sqrtf(float value) { return sqrt_impl_float(value); }
inline float sqrt(float value) { return sqrt_impl_float(value); }
inline double sqrt(double value) { return sqrt_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
sqrt(T value) { return sqrt_impl_float(static_cast<float>(value)); }

// Generic sqrt that auto-binds to types with static sqrt method
// If T doesn't have a static sqrt, this overload is discarded (SFINAE)
template<typename T>
inline typename enable_if<sqrt_detail::has_static_sqrt<T>::value, decltype(T::sqrt(declval<T>()))>::type
sqrt(T value) {
    return T::sqrt(value);
}

namespace floor_detail {
// Type trait to detect if T has a static floor method
template<typename T, typename = void>
struct has_static_floor : false_type {};

template<typename T>
struct has_static_floor<T, decltype(static_cast<void>(T::floor(declval<T>())))> : true_type {};
}

// Generic floor that auto-binds to types with static floor method
template<typename T>
inline typename enable_if<floor_detail::has_static_floor<T>::value, decltype(T::floor(declval<T>()))>::type
floor(T value) {
    return T::floor(value);
}

namespace ceil_detail {
// Type trait to detect if T has a static ceil method
template<typename T, typename = void>
struct has_static_ceil : false_type {};

template<typename T>
struct has_static_ceil<T, decltype(static_cast<void>(T::ceil(declval<T>())))> : true_type {};
}

// Generic ceil that auto-binds to types with static ceil method
template<typename T>
inline typename enable_if<ceil_detail::has_static_ceil<T>::value, decltype(T::ceil(declval<T>()))>::type
ceil(T value) {
    return T::ceil(value);
}

// hypot
inline float hypotf(float x, float y) { return hypot_impl_float(x, y); }
inline double hypot(double x, double y) { return hypot_impl_double(x, y); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
hypot(T x, T y) { return hypot_impl_float(static_cast<float>(x), static_cast<float>(y)); }

// ===== Floating-point utilities ==============================================

// fabs
inline float fabsf(float value) { return fabs_impl_float(value); }
inline double fabs(double value) { return fabs_impl_double(value); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
fabs(T value) { return fabs_impl_float(static_cast<float>(value)); }

// ldexp — multiply by power of 2: value * 2^exp
inline float ldexpf(float value, int exp) { return ldexp_impl_float(value, exp); }
inline double ldexp(double value, int exp) { return ldexp_impl_double(value, exp); }
template<typename T> inline typename enable_if<is_integral<T>::value, float>::type
ldexp(T value, int exp) { return ldexp_impl_float(static_cast<float>(value), exp); }

// ===== Angle conversion ======================================================

template<typename T>
constexpr inline T radians(T deg) {
    return deg * static_cast<T>(0.017453292519943295); // PI / 180
}

template<typename T>
constexpr inline T degrees(T rad) {
    return rad * static_cast<T>(57.29577951308232); // 180 / PI
}

// ===== sincos ================================================================

// sincos(angle, &out_sin, &out_cos) for floating-point types
template <typename T>
inline typename enable_if<is_floating_point<T>::value>::type
sincos(T angle, T& out_sin, T& out_cos) {
    out_sin = static_cast<T>(fl::sinf(static_cast<float>(angle)));
    out_cos = static_cast<T>(fl::cosf(static_cast<float>(angle)));
}

// sincos(angle, &out_sin, &out_cos) for integral types
template <typename T>
inline typename enable_if<is_integral<T>::value>::type
sincos(T angle, float& out_sin, float& out_cos) {
    out_sin = fl::sinf(static_cast<float>(angle));
    out_cos = fl::cosf(static_cast<float>(angle));
}

} // namespace fl
