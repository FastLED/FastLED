#include "fl/stl/math.h"
#include <math.h>

namespace fl {

// Standalone floor implementation for float
float floor_impl_float(float value) {
    if (value >= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i - (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone floor implementation for double
double floor_impl_double(double value) {
    if (value >= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i - (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone ceil implementation for float
float ceil_impl_float(float value) {
    if (value <= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i + (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone ceil implementation for double
double ceil_impl_double(double value) {
    if (value <= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i + (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone exp implementation using Taylor series
// e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + ...
float exp_impl_float(float value) {
    if (value > 10.0f)
        return 22026.465794806718f; // e^10 approx
    if (value < -10.0f)
        return 0.0000453999297625f; // e^-10 approx

    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 10; ++i) {
        term *= value / static_cast<float>(i);
        result += term;
    }
    return result;
}

double exp_impl_double(double value) {
    if (value > 10.0)
        return 22026.465794806718; // e^10 approx
    if (value < -10.0)
        return 0.0000453999297625; // e^-10 approx

    double result = 1.0;
    double term = 1.0;
    for (int i = 1; i < 10; ++i) {
        term *= value / static_cast<double>(i);
        result += term;
    }
    return result;
}

// Square root implementations using standard library
float sqrt_impl_float(float value) {
    return ::sqrtf(value);
}

double sqrt_impl_double(double value) {
    return ::sqrt(value);
}

// Sine implementations using standard library
float sin_impl_float(float value) {
    return ::sinf(value);
}

double sin_impl_double(double value) {
    return ::sin(value);
}

// Cosine implementations using standard library
float cos_impl_float(float value) {
    return ::cosf(value);
}

double cos_impl_double(double value) {
    return ::cos(value);
}

// Natural logarithm implementations using standard library
float log_impl_float(float value) {
    return ::logf(value);
}

double log_impl_double(double value) {
    return ::log(value);
}

// Base-10 logarithm implementations using standard library
float log10_impl_float(float value) {
    return ::log10f(value);
}

double log10_impl_double(double value) {
    return ::log10(value);
}

// Power implementations using standard library
float pow_impl_float(float base, float exponent) {
    return ::powf(base, exponent);
}

double pow_impl_double(double base, double exponent) {
    return ::pow(base, exponent);
}

// Absolute value implementations using standard library
float fabs_impl_float(float value) {
    return ::fabsf(value);
}

double fabs_impl_double(double value) {
    return ::fabs(value);
}

// Round to nearest long integer implementations using standard library
long lround_impl_float(float value) {
    return ::lroundf(value);
}

long lround_impl_double(double value) {
    return ::lround(value);
}

// Round to nearest floating-point value implementations using standard library
float round_impl_float(float value) {
    return ::roundf(value);
}

double round_impl_double(double value) {
    return ::round(value);
}

// Floating-point modulo implementations using standard library
float fmod_impl_float(float x, float y) {
    return ::fmodf(x, y);
}

double fmod_impl_double(double x, double y) {
    return ::fmod(x, y);
}

// Inverse tangent 2 implementations (atan2) using standard library
float atan2_impl_float(float y, float x) {
    return ::atan2f(y, x);
}

double atan2_impl_double(double y, double x) {
    return ::atan2(y, x);
}

// Hypotenuse implementations (hypot) using standard library
float hypot_impl_float(float x, float y) {
    return ::hypotf(x, y);
}

double hypot_impl_double(double x, double y) {
    return ::hypot(x, y);
}

// Inverse tangent implementations (atan) using standard library
float atan_impl_float(float value) {
    return ::atanf(value);
}

double atan_impl_double(double value) {
    return ::atan(value);
}

// Inverse sine implementations (asin) using standard library
float asin_impl_float(float value) {
    return ::asinf(value);
}

double asin_impl_double(double value) {
    return ::asin(value);
}

// Inverse cosine implementations (acos) using standard library
float acos_impl_float(float value) {
    return ::acosf(value);
}

double acos_impl_double(double value) {
    return ::acos(value);
}

// Tangent implementations (tan) using standard library
float tan_impl_float(float value) {
    return ::tanf(value);
}

double tan_impl_double(double value) {
    return ::tan(value);
}

} // namespace fl
