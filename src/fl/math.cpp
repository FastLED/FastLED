#include "fl/math.h"

namespace fl {

// Standalone floor implementation for float
float floor_impl(float value) {
    if (value >= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i - (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone floor implementation for double
double floor_impl(double value) {
    if (value >= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i - (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone ceil implementation for float
float ceil_impl(float value) {
    if (value <= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i + (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone ceil implementation for double
double ceil_impl(double value) {
    if (value <= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i + (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone exp implementation using Taylor series
// e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + ...
float exp_impl(float value) {
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

double exp_impl(double value) {
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

} // namespace fl
