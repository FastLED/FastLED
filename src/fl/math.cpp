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

// Standalone sqrt implementation using Newton-Raphson method
// sqrt(x) = y where y*y = x
// Iterative formula: y_new = (y + x/y) / 2
float sqrt_impl(float value) {
    if (value < 0.0f)
        return 0.0f; // or NaN, but 0 is safer for embedded
    if (value == 0.0f)
        return 0.0f;

    // Initial guess using bit manipulation for fast approximation
    float guess = value * 0.5f;
    if (guess == 0.0f)
        guess = value;

    // Newton-Raphson iterations (5 iterations gives good accuracy)
    for (int i = 0; i < 5; ++i) {
        guess = (guess + value / guess) * 0.5f;
    }
    return guess;
}

double sqrt_impl(double value) {
    if (value < 0.0)
        return 0.0; // or NaN, but 0 is safer for embedded
    if (value == 0.0)
        return 0.0;

    // Initial guess
    double guess = value * 0.5;
    if (guess == 0.0)
        guess = value;

    // Newton-Raphson iterations (6 iterations for double precision)
    for (int i = 0; i < 6; ++i) {
        guess = (guess + value / guess) * 0.5;
    }
    return guess;
}

} // namespace fl
