#pragma once

/*
Provides numeric_limits for fundamental types, similar to <limits> from the C++ standard library.
This avoids including <limits> which can slow down compilation.
Follows the same pattern as fl/type_traits.h to provide essential type information.
*/

#include "fl/stl/stdint.h"
#include "fl/undef.h"
#include "fl/stl/math.h"

namespace fl {

// Helper template to compute integer limits based on size and signedness
namespace detail {
    // Compute number of value bits (excludes sign bit for signed types)
    // C++11 compatible: use constexpr function instead of variable template
    template<typename T>
    constexpr int integer_digits_func() {
        return (sizeof(T) * 8) - (T(-1) < T(0) ? 1 : 0);
    }

    template<typename T>
    struct integer_digits_helper {
        static constexpr int value = (sizeof(T) * 8) - (T(-1) < T(0) ? 1 : 0);
    };

    // Compute digits10: floor(log10(2^digits))
    // For 8 bits: 2, 16 bits: 4, 32 bits: 9, 64 bits: 19
    // C++11 compatible: use constexpr function instead of variable template
    template<typename T>
    constexpr int integer_digits10_func() {
        return sizeof(T) == 1 ? 2 :
               sizeof(T) == 2 ? 4 :
               sizeof(T) == 4 ? 9 :
               sizeof(T) == 8 ? 19 : 0;
    }

    template<typename T>
    struct integer_digits10_helper {
        static constexpr int value =
            sizeof(T) == 1 ? 2 :
            sizeof(T) == 2 ? 4 :
            sizeof(T) == 4 ? 9 :
            sizeof(T) == 8 ? 19 : 0;
    };

    // Compute minimum value based on signedness
    template<typename T, bool IsSigned = (T(-1) < T(0))>
    struct integer_min_helper;

    template<typename T>
    struct integer_min_helper<T, true> {  // Signed
        static constexpr T value() noexcept {
            // For signed: -(2^(bits-1))
            // Use the pattern: -MAX - 1 to avoid overflow
            return sizeof(T) == 1 ? T(-128) :
                   sizeof(T) == 2 ? T(-32768) :
                   sizeof(T) == 4 ? T(-2147483647 - 1) :
                   sizeof(T) == 8 ? T(-9223372036854775807LL - 1) : T(0);
        }
    };

    template<typename T>
    struct integer_min_helper<T, false> {  // Unsigned
        static constexpr T value() noexcept { return T(0); }
    };

    // Compute maximum value based on signedness and size
    template<typename T, bool IsSigned = (T(-1) < T(0))>
    struct integer_max_helper;

    template<typename T>
    struct integer_max_helper<T, true> {  // Signed
        static constexpr T value() noexcept {
            // For signed: 2^(bits-1) - 1
            return sizeof(T) == 1 ? T(127) :
                   sizeof(T) == 2 ? T(32767) :
                   sizeof(T) == 4 ? T(2147483647) :
                   sizeof(T) == 8 ? T(9223372036854775807LL) : T(0);
        }
    };

    template<typename T>
    struct integer_max_helper<T, false> {  // Unsigned
        static constexpr T value() noexcept {
            // For unsigned: 2^bits - 1
            return sizeof(T) == 1 ? T(255) :
                   sizeof(T) == 2 ? T(65535) :
                   sizeof(T) == 4 ? T(4294967295U) :
                   sizeof(T) == 8 ? T(18446744073709551615ULL) : T(0);
        }
    };
}

// Primary template - default numeric_limits for unknown types
template <typename T>
struct numeric_limits {
    enum : bool { is_specialized = false };
    enum : bool { is_signed = false };
    enum : bool { is_integer = false };
    enum : bool { is_exact = false };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    enum : int { digits = 0 };
    enum : int { digits10 = 0 };

    static constexpr T (min)() noexcept { return T(); }
    static constexpr T (max)() noexcept { return T(); }
    static constexpr T (lowest)() noexcept { return T(); }
    static constexpr T epsilon() noexcept { return T(); }
    static constexpr T round_error() noexcept { return T(); }
    static constexpr T infinity() noexcept { return T(); }
    static constexpr T quiet_NaN() noexcept { return T(); }
    static constexpr T signaling_NaN() noexcept { return T(); }
    static constexpr T denorm_min() noexcept { return T(); }
};

// Specialization for bool
template <>
struct numeric_limits<bool> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    enum : int { digits = 1 };
    enum : int { digits10 = 0 };

    static constexpr bool (min)() noexcept { return false; }
    static constexpr bool (max)() noexcept { return true; }
    static constexpr bool (lowest)() noexcept { return false; }
    static constexpr bool epsilon() noexcept { return false; }
    static constexpr bool round_error() noexcept { return false; }
};

// Specialization for char
template <>
struct numeric_limits<char> {
    enum : bool { is_specialized = true };
    static constexpr bool is_signed = (char(-1) < char(0));
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<char>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<char>::value;

    static constexpr char (min)() noexcept { return detail::integer_min_helper<char>::value(); }
    static constexpr char (max)() noexcept { return detail::integer_max_helper<char>::value(); }
    static constexpr char (lowest)() noexcept { return (min)(); }
    static constexpr char epsilon() noexcept { return 0; }
    static constexpr char round_error() noexcept { return 0; }
};

// Specialization for signed char (i8)
template <>
struct numeric_limits<signed char> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<signed char>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<signed char>::value;

    static constexpr signed char (min)() noexcept { return detail::integer_min_helper<signed char>::value(); }
    static constexpr signed char (max)() noexcept { return detail::integer_max_helper<signed char>::value(); }
    static constexpr signed char (lowest)() noexcept { return (min)(); }
    static constexpr signed char epsilon() noexcept { return 0; }
    static constexpr signed char round_error() noexcept { return 0; }
};

// Specialization for unsigned char (u8)
template <>
struct numeric_limits<unsigned char> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<unsigned char>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<unsigned char>::value;

    static constexpr unsigned char (min)() noexcept { return detail::integer_min_helper<unsigned char>::value(); }
    static constexpr unsigned char (max)() noexcept { return detail::integer_max_helper<unsigned char>::value(); }
    static constexpr unsigned char (lowest)() noexcept { return 0; }
    static constexpr unsigned char epsilon() noexcept { return 0; }
    static constexpr unsigned char round_error() noexcept { return 0; }
};

// Protect against min/max macros from Arduino and other platforms
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

// Specialization for short (i16)
template <>
struct numeric_limits<short> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<short>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<short>::value;

    static constexpr short min() noexcept { return detail::integer_min_helper<short>::value(); }
    static constexpr short max() noexcept { return detail::integer_max_helper<short>::value(); }
    static constexpr short lowest() noexcept { return min(); }
    static constexpr short epsilon() noexcept { return 0; }
    static constexpr short round_error() noexcept { return 0; }
};

// Specialization for unsigned short (u16)
template <>
struct numeric_limits<unsigned short> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<unsigned short>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<unsigned short>::value;

    static constexpr unsigned short min() noexcept { return detail::integer_min_helper<unsigned short>::value(); }
    static constexpr unsigned short max() noexcept { return detail::integer_max_helper<unsigned short>::value(); }
    static constexpr unsigned short lowest() noexcept { return 0; }
    static constexpr unsigned short epsilon() noexcept { return 0; }
    static constexpr unsigned short round_error() noexcept { return 0; }
};

// Specialization for int
template <>
struct numeric_limits<int> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    // int can be 16-bit (AVR) or 32-bit (most platforms) depending on platform
    static constexpr int digits = detail::integer_digits_helper<int>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<int>::value;

    static constexpr int min() noexcept {
        return detail::integer_min_helper<int>::value();
    }
    static constexpr int max() noexcept {
        return detail::integer_max_helper<int>::value();
    }
    static constexpr int lowest() noexcept { return min(); }
    static constexpr int epsilon() noexcept { return 0; }
    static constexpr int round_error() noexcept { return 0; }
};

// Specialization for unsigned int
template <>
struct numeric_limits<unsigned int> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    // unsigned int can be 16-bit (AVR) or 32-bit (most platforms) depending on platform
    static constexpr int digits = detail::integer_digits_helper<unsigned int>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<unsigned int>::value;

    static constexpr unsigned int min() noexcept {
        return detail::integer_min_helper<unsigned int>::value();
    }
    static constexpr unsigned int max() noexcept {
        return detail::integer_max_helper<unsigned int>::value();
    }
    static constexpr unsigned int lowest() noexcept { return 0; }
    static constexpr unsigned int epsilon() noexcept { return 0; }
    static constexpr unsigned int round_error() noexcept { return 0; }
};

// Specialization for long
template <>
struct numeric_limits<long> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    // long can be 32-bit or 64-bit depending on platform
    static constexpr int digits = detail::integer_digits_helper<long>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<long>::value;

    static constexpr long min() noexcept {
        return detail::integer_min_helper<long>::value();
    }
    static constexpr long max() noexcept {
        return detail::integer_max_helper<long>::value();
    }
    static constexpr long lowest() noexcept { return min(); }
    static constexpr long epsilon() noexcept { return 0; }
    static constexpr long round_error() noexcept { return 0; }
};

// Specialization for unsigned long
template <>
struct numeric_limits<unsigned long> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    // unsigned long can be 32-bit or 64-bit depending on platform
    static constexpr int digits = detail::integer_digits_helper<unsigned long>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<unsigned long>::value;

    static constexpr unsigned long min() noexcept {
        return detail::integer_min_helper<unsigned long>::value();
    }
    static constexpr unsigned long max() noexcept {
        return detail::integer_max_helper<unsigned long>::value();
    }
    static constexpr unsigned long lowest() noexcept { return 0; }
    static constexpr unsigned long epsilon() noexcept { return 0; }
    static constexpr unsigned long round_error() noexcept { return 0; }
};

// Specialization for long long (i64)
template <>
struct numeric_limits<long long> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<long long>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<long long>::value;

    static constexpr long long min() noexcept { return detail::integer_min_helper<long long>::value(); }
    static constexpr long long max() noexcept { return detail::integer_max_helper<long long>::value(); }
    static constexpr long long lowest() noexcept { return min(); }
    static constexpr long long epsilon() noexcept { return 0; }
    static constexpr long long round_error() noexcept { return 0; }
};

// Specialization for unsigned long long (u64)
template <>
struct numeric_limits<unsigned long long> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = false };
    enum : bool { is_integer = true };
    enum : bool { is_exact = true };
    enum : bool { has_infinity = false };
    enum : bool { has_quiet_NaN = false };
    enum : bool { has_signaling_NaN = false };
    static constexpr int digits = detail::integer_digits_helper<unsigned long long>::value;
    static constexpr int digits10 = detail::integer_digits10_helper<unsigned long long>::value;

    static constexpr unsigned long long min() noexcept { return detail::integer_min_helper<unsigned long long>::value(); }
    static constexpr unsigned long long max() noexcept { return detail::integer_max_helper<unsigned long long>::value(); }
    static constexpr unsigned long long lowest() noexcept { return 0; }
    static constexpr unsigned long long epsilon() noexcept { return 0; }
    static constexpr unsigned long long round_error() noexcept { return 0; }
};

// Specialization for float
template <>
struct numeric_limits<float> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = false };
    enum : bool { is_exact = false };
    enum : bool { has_infinity = true };
    enum : bool { has_quiet_NaN = true };
    enum : bool { has_signaling_NaN = true };
    enum : int { digits = 24 };       // __FLT_MANT_DIG__ is typically 24
    enum : int { digits10 = 6 };      // __FLT_DIG__ is typically 6
    static constexpr int max_digits10 = 9;  // Maximum digits for round-trip
    static constexpr int max_exponent = 128;   // __FLT_MAX_EXP__ is typically 128
    static constexpr int max_exponent10 = 38;  // __FLT_MAX_10_EXP__ is typically 38
    static constexpr int min_exponent = -125;  // __FLT_MIN_EXP__ is typically -125
    static constexpr int min_exponent10 = -37; // __FLT_MIN_10_EXP__ is typically -37

    // Use compiler built-in constants (GCC/Clang) instead of hardcoded literals
    static constexpr float min() noexcept { return __FLT_MIN__; }
    static constexpr float max() noexcept { return __FLT_MAX__; }
    static constexpr float lowest() noexcept { return -__FLT_MAX__; }
    static constexpr float epsilon() noexcept { return __FLT_EPSILON__; }
    static constexpr float round_error() noexcept { return 0.5F; }

    static constexpr float infinity() noexcept {
        return __builtin_huge_valf();
    }
    static constexpr float quiet_NaN() noexcept {
        return __builtin_nanf("");
    }
    static constexpr float signaling_NaN() noexcept {
        return __builtin_nansf("");
    }
    // Use compiler built-in for denormalized minimum (platform-specific)
    static constexpr float denorm_min() noexcept {
        return __FLT_DENORM_MIN__;
    }
};

// Specialization for double
template <>
struct numeric_limits<double> {
    enum : bool { is_specialized = true };
    enum : bool { is_signed = true };
    enum : bool { is_integer = false };
    enum : bool { is_exact = false };
    enum : bool { has_infinity = true };
    enum : bool { has_quiet_NaN = true };
    enum : bool { has_signaling_NaN = true };
    enum : int { digits = 53 };       // __DBL_MANT_DIG__ is typically 53
    enum : int { digits10 = 15 };     // __DBL_DIG__ is typically 15
    static constexpr int max_digits10 = 17; // Maximum digits for round-trip
    static constexpr int max_exponent = 1024;   // __DBL_MAX_EXP__ is typically 1024
    static constexpr int max_exponent10 = 308;  // __DBL_MAX_10_EXP__ is typically 308
    static constexpr int min_exponent = -1021;  // __DBL_MIN_EXP__ is typically -1021
    static constexpr int min_exponent10 = -307; // __DBL_MIN_10_EXP__ is typically -307

    // Use compiler built-in constants (GCC/Clang) instead of hardcoded literals
    // This avoids overflow warnings on platforms with strict compile-time float handling
    static constexpr double min() noexcept { return __DBL_MIN__; }
    static constexpr double max() noexcept { return __DBL_MAX__; }
    static constexpr double lowest() noexcept { return -__DBL_MAX__; }
    static constexpr double epsilon() noexcept { return __DBL_EPSILON__; }
    static constexpr double round_error() noexcept { return 0.5; }

    static constexpr double infinity() noexcept {
        return __builtin_huge_val();
    }
    static constexpr double quiet_NaN() noexcept {
        return __builtin_nan("");
    }
    static constexpr double signaling_NaN() noexcept {
        return __builtin_nans("");
    }
    // Use compiler built-in for denormalized minimum (platform-specific)
    static constexpr double denorm_min() noexcept {
        return __DBL_DENORM_MIN__;
    }
};

// long double specialization removed

// Restore min/max macros if they were defined
#pragma pop_macro("max")
#pragma pop_macro("min")

} // namespace fl
