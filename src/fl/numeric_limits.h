#pragma once

#include "fl/type_traits.h"

namespace fl {

// Primary template - default for unsupported types
template<typename T> class numeric_limits {
public:
    // Type is not specialized, no meaningful limits
};

// Helper template to compute min/max for integral types
// Uses type traits and sizeof to avoid hardcoded values
template<typename T, bool IsSigned = is_signed<T>::value>
struct integral_limits_impl;

// Specialization for unsigned integer types
template<typename T>
struct integral_limits_impl<T, false> {
    // For unsigned types: max = (1 << bits) - 1, min = 0
    static constexpr T compute_max() {
        // Bit-shift approach: shift all bits to 1
        return static_cast<T>(~static_cast<T>(0));
    }
};

// Specialization for signed integer types
template<typename T>
struct integral_limits_impl<T, true> {
    // For signed types, we need both min and max
    // max = (1 << (bits-1)) - 1
    // min = -(1 << (bits-1))

    static constexpr T compute_max() {
        // Max is all bits set except the sign bit
        // This equals: (1 << (bits-1)) - 1
        T val = static_cast<T>(1) << (sizeof(T) * 8 - 1);
        return static_cast<T>(~val);
    }

    static constexpr T compute_min() {
        // Min is the sign bit set, rest are zeros
        return static_cast<T>(1) << (sizeof(T) * 8 - 1);
    }
};

// Specialization for int
template<> class numeric_limits<int> {
public:
    static constexpr int min() { return integral_limits_impl<int>::compute_min(); }
    static constexpr int max() { return integral_limits_impl<int>::compute_max(); }
};

// Specialization for unsigned int
template<> class numeric_limits<unsigned int> {
public:
    static constexpr unsigned int min() { return 0; }
    static constexpr unsigned int max() { return integral_limits_impl<unsigned int>::compute_max(); }
};

// Specialization for long
template<> class numeric_limits<long> {
public:
    static constexpr long min() { return integral_limits_impl<long>::compute_min(); }
    static constexpr long max() { return integral_limits_impl<long>::compute_max(); }
};

// Specialization for unsigned long
template<> class numeric_limits<unsigned long> {
public:
    static constexpr unsigned long min() { return 0; }
    static constexpr unsigned long max() { return integral_limits_impl<unsigned long>::compute_max(); }
};

// Specialization for long long
template<> class numeric_limits<long long> {
public:
    static constexpr long long min() { return integral_limits_impl<long long>::compute_min(); }
    static constexpr long long max() { return integral_limits_impl<long long>::compute_max(); }
};

// Specialization for unsigned long long
template<> class numeric_limits<unsigned long long> {
public:
    static constexpr unsigned long long min() { return 0; }
    static constexpr unsigned long long max() { return integral_limits_impl<unsigned long long>::compute_max(); }
};

// Specialization for float
template<> class numeric_limits<float> {
public:
    static constexpr float min() { return __FLT_MIN__; }
    static constexpr float max() { return __FLT_MAX__; }
};

// Specialization for double
template<> class numeric_limits<double> {
public:
    static constexpr double min() { return __DBL_MIN__; }
    static constexpr double max() { return __DBL_MAX__; }
};

} // namespace fl
