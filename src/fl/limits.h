#pragma once

/*
Provides numeric_limits for fundamental types, similar to <limits> from the C++ standard library.
This avoids including <limits> which can slow down compilation.
Follows the same pattern as fl/type_traits.h to provide essential type information.
*/

#include "fl/stdint.h"
#include "fl/namespace.h"

namespace fl {

// Helper template to compute integer limits based on size and signedness
namespace detail {
    // Compute number of value bits (excludes sign bit for signed types)
    template<typename T>
    static constexpr int integer_digits = (sizeof(T) * 8) - (T(-1) < T(0) ? 1 : 0);

    // Compute digits10: floor(log10(2^digits))
    // For 8 bits: 2, 16 bits: 4, 32 bits: 9, 64 bits: 19
    template<typename T>
    static constexpr int integer_digits10 =
        sizeof(T) == 1 ? 2 :
        sizeof(T) == 2 ? 4 :
        sizeof(T) == 4 ? 9 :
        sizeof(T) == 8 ? 19 : 0;

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
    static constexpr bool is_specialized = false;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = 0;
    static constexpr int digits10 = 0;

    static constexpr T min() noexcept { return T(); }
    static constexpr T max() noexcept { return T(); }
    static constexpr T lowest() noexcept { return T(); }
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
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = 1;
    static constexpr int digits10 = 0;

    static constexpr bool min() noexcept { return false; }
    static constexpr bool max() noexcept { return true; }
    static constexpr bool lowest() noexcept { return false; }
    static constexpr bool epsilon() noexcept { return false; }
    static constexpr bool round_error() noexcept { return false; }
};

// Specialization for char
template <>
struct numeric_limits<char> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = (char(-1) < char(0));
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<char>;
    static constexpr int digits10 = detail::integer_digits10<char>;

    static constexpr char min() noexcept { return detail::integer_min_helper<char>::value(); }
    static constexpr char max() noexcept { return detail::integer_max_helper<char>::value(); }
    static constexpr char lowest() noexcept { return min(); }
    static constexpr char epsilon() noexcept { return 0; }
    static constexpr char round_error() noexcept { return 0; }
};

// Specialization for signed char (i8)
template <>
struct numeric_limits<signed char> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<signed char>;
    static constexpr int digits10 = detail::integer_digits10<signed char>;

    static constexpr signed char min() noexcept { return detail::integer_min_helper<signed char>::value(); }
    static constexpr signed char max() noexcept { return detail::integer_max_helper<signed char>::value(); }
    static constexpr signed char lowest() noexcept { return min(); }
    static constexpr signed char epsilon() noexcept { return 0; }
    static constexpr signed char round_error() noexcept { return 0; }
};

// Specialization for unsigned char (u8)
template <>
struct numeric_limits<unsigned char> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<unsigned char>;
    static constexpr int digits10 = detail::integer_digits10<unsigned char>;

    static constexpr unsigned char min() noexcept { return detail::integer_min_helper<unsigned char>::value(); }
    static constexpr unsigned char max() noexcept { return detail::integer_max_helper<unsigned char>::value(); }
    static constexpr unsigned char lowest() noexcept { return 0; }
    static constexpr unsigned char epsilon() noexcept { return 0; }
    static constexpr unsigned char round_error() noexcept { return 0; }
};

// Specialization for short (i16)
template <>
struct numeric_limits<short> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<short>;
    static constexpr int digits10 = detail::integer_digits10<short>;

    static constexpr short min() noexcept { return detail::integer_min_helper<short>::value(); }
    static constexpr short max() noexcept { return detail::integer_max_helper<short>::value(); }
    static constexpr short lowest() noexcept { return min(); }
    static constexpr short epsilon() noexcept { return 0; }
    static constexpr short round_error() noexcept { return 0; }
};

// Specialization for unsigned short (u16)
template <>
struct numeric_limits<unsigned short> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<unsigned short>;
    static constexpr int digits10 = detail::integer_digits10<unsigned short>;

    static constexpr unsigned short min() noexcept { return detail::integer_min_helper<unsigned short>::value(); }
    static constexpr unsigned short max() noexcept { return detail::integer_max_helper<unsigned short>::value(); }
    static constexpr unsigned short lowest() noexcept { return 0; }
    static constexpr unsigned short epsilon() noexcept { return 0; }
    static constexpr unsigned short round_error() noexcept { return 0; }
};

// Specialization for int
template <>
struct numeric_limits<int> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    // int can be 16-bit (AVR) or 32-bit (most platforms) depending on platform
    static constexpr int digits = detail::integer_digits<int>;
    static constexpr int digits10 = detail::integer_digits10<int>;

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
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    // unsigned int can be 16-bit (AVR) or 32-bit (most platforms) depending on platform
    static constexpr int digits = detail::integer_digits<unsigned int>;
    static constexpr int digits10 = detail::integer_digits10<unsigned int>;

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
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    // long can be 32-bit or 64-bit depending on platform
    static constexpr int digits = detail::integer_digits<long>;
    static constexpr int digits10 = detail::integer_digits10<long>;

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
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    // unsigned long can be 32-bit or 64-bit depending on platform
    static constexpr int digits = detail::integer_digits<unsigned long>;
    static constexpr int digits10 = detail::integer_digits10<unsigned long>;

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
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<long long>;
    static constexpr int digits10 = detail::integer_digits10<long long>;

    static constexpr long long min() noexcept { return detail::integer_min_helper<long long>::value(); }
    static constexpr long long max() noexcept { return detail::integer_max_helper<long long>::value(); }
    static constexpr long long lowest() noexcept { return min(); }
    static constexpr long long epsilon() noexcept { return 0; }
    static constexpr long long round_error() noexcept { return 0; }
};

// Specialization for unsigned long long (u64)
template <>
struct numeric_limits<unsigned long long> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr int digits = detail::integer_digits<unsigned long long>;
    static constexpr int digits10 = detail::integer_digits10<unsigned long long>;

    static constexpr unsigned long long min() noexcept { return detail::integer_min_helper<unsigned long long>::value(); }
    static constexpr unsigned long long max() noexcept { return detail::integer_max_helper<unsigned long long>::value(); }
    static constexpr unsigned long long lowest() noexcept { return 0; }
    static constexpr unsigned long long epsilon() noexcept { return 0; }
    static constexpr unsigned long long round_error() noexcept { return 0; }
};

// Specialization for float
template <>
struct numeric_limits<float> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr int digits = 24;       // FLT_MANT_DIG
    static constexpr int digits10 = 6;      // FLT_DIG
    static constexpr int max_digits10 = 9;  // Maximum digits for round-trip
    static constexpr int max_exponent = 128;   // FLT_MAX_EXP
    static constexpr int max_exponent10 = 38;  // FLT_MAX_10_EXP
    static constexpr int min_exponent = -125;  // FLT_MIN_EXP
    static constexpr int min_exponent10 = -37; // FLT_MIN_10_EXP

    static constexpr float min() noexcept { return 1.17549435e-38F; }  // FLT_MIN
    static constexpr float max() noexcept { return 3.40282347e+38F; }  // FLT_MAX
    static constexpr float lowest() noexcept { return -3.40282347e+38F; }
    static constexpr float epsilon() noexcept { return 1.19209290e-07F; }  // FLT_EPSILON
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
    static constexpr float denorm_min() noexcept { return 1.40129846e-45F; }  // FLT_TRUE_MIN
};

// Specialization for double
template <>
struct numeric_limits<double> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr int digits = 53;       // DBL_MANT_DIG
    static constexpr int digits10 = 15;     // DBL_DIG
    static constexpr int max_digits10 = 17; // Maximum digits for round-trip
    static constexpr int max_exponent = 1024;   // DBL_MAX_EXP
    static constexpr int max_exponent10 = 308;  // DBL_MAX_10_EXP
    static constexpr int min_exponent = -1021;  // DBL_MIN_EXP
    static constexpr int min_exponent10 = -307; // DBL_MIN_10_EXP

    static constexpr double min() noexcept { return 2.2250738585072014e-308; }  // DBL_MIN
    static constexpr double max() noexcept { return 1.7976931348623157e+308; }  // DBL_MAX
    static constexpr double lowest() noexcept { return -1.7976931348623157e+308; }
    static constexpr double epsilon() noexcept { return 2.2204460492503131e-16; }  // DBL_EPSILON
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
    static constexpr double denorm_min() noexcept { return 4.9406564584124654e-324; }  // DBL_TRUE_MIN
};

// Specialization for long double
template <>
struct numeric_limits<long double> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    // long double precision varies by platform (64-bit, 80-bit, or 128-bit)
    static constexpr int digits = sizeof(long double) == 16 ? 113 :
                                  sizeof(long double) == 12 ? 64 : 53;
    static constexpr int digits10 = sizeof(long double) == 16 ? 33 :
                                    sizeof(long double) == 12 ? 18 : 15;
    static constexpr int max_digits10 = sizeof(long double) == 16 ? 36 :
                                        sizeof(long double) == 12 ? 21 : 17;

    static constexpr long double min() noexcept {
        // LDBL_MIN - varies by platform
        return sizeof(long double) == 16 ? 3.36210314311209350626e-4932L :
               sizeof(long double) == 12 ? 3.36210314311209350626e-4932L :
               2.2250738585072014e-308L;
    }
    static constexpr long double max() noexcept {
        // LDBL_MAX - varies by platform
        return sizeof(long double) == 16 ? 1.18973149535723176502e+4932L :
               sizeof(long double) == 12 ? 1.18973149535723176502e+4932L :
               1.7976931348623157e+308L;
    }
    static constexpr long double lowest() noexcept { return -max(); }
    static constexpr long double epsilon() noexcept {
        // LDBL_EPSILON - varies by platform
        return sizeof(long double) == 16 ? 1.92592994438723585305e-34L :
               sizeof(long double) == 12 ? 1.08420217248550443401e-19L :
               2.2204460492503131e-16L;
    }
    static constexpr long double round_error() noexcept { return 0.5L; }

    static constexpr long double infinity() noexcept {
        return __builtin_huge_vall();
    }
    static constexpr long double quiet_NaN() noexcept {
        return __builtin_nanl("");
    }
    static constexpr long double signaling_NaN() noexcept {
        return __builtin_nansl("");
    }
    static constexpr long double denorm_min() noexcept {
        // Conservative denorm_min - platform-specific
        // 128-bit: theoretical minimum is ~3.6e-4951 but may not be representable on all platforms
        // 80-bit: ~3.6e-4951
        // 64-bit (same as double): ~4.9e-324
        return sizeof(long double) == 16 ? 3.64519953188247460253e-4951L :
               sizeof(long double) == 12 ? 3.64519953188247460253e-4951L :
               4.9406564584124654e-324L;
    }
};

} // namespace fl
