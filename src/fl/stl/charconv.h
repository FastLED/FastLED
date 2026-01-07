#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Character Conversion Functions (charconv.h equivalents)
//
// Provides character/string conversion utilities similar to C++17 <charconv>
// but adapted for embedded platforms and FastLED's needs.
//
// All functions are in the fl:: namespace.
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/stdint.h"  // For uint64_t, uint8_t
#include "fl/stl/cstddef.h" // For fl::size_t
#include "fl/str.h"      // For fl::string

namespace fl {

/// @brief Convert an integer value to hexadecimal string representation
/// @tparam T Integral type
/// @param value The integer value to convert
/// @param uppercase If true, use uppercase hex digits (A-F), otherwise lowercase (a-f)
/// @param pad_to_width If true, pad with leading zeros to full type width (default: false for minimal representation)
/// @return Hexadecimal string representation of the value
///
/// Example usage:
/// @code
/// fl::string hex = fl::to_hex(255, false);              // "ff" (minimal)
/// fl::string hex_upper = fl::to_hex(255, true);         // "FF" (minimal uppercase)
/// fl::string hex_padded = fl::to_hex(255, false, true); // "000000ff" (padded to int width)
/// fl::string hex_neg = fl::to_hex(-16, false);          // "-10"
/// @endcode
template<typename T>
fl::string to_hex(T value, bool uppercase = false, bool pad_to_width = false);

namespace detail {

/// @brief Integer width classification for hex conversion
enum class HexIntWidth : uint8_t {
    Width8 = 8,
    Width16 = 16,
    Width32 = 32,
    Width64 = 64
};

/// @brief Internal hex conversion function (implementation in charconv.cpp)
/// @param value The unsigned 64-bit value to convert
/// @param width The bit width classification of the original type
/// @param is_negative Whether the original value was negative (for signed types)
/// @param uppercase If true, use uppercase hex digits (A-F), otherwise lowercase (a-f)
/// @param pad_to_width If true, pad with leading zeros to full type width
/// @return Hexadecimal string representation
fl::string hex(uint64_t value, HexIntWidth width, bool is_negative, bool uppercase, bool pad_to_width);

/// @brief Compile-time integer width determination (default - triggers error)
template<size_t Size>
constexpr HexIntWidth get_hex_int_width() {
    static_assert(Size == 0, "Unsupported type size for hex conversion");
    return HexIntWidth::Width8; // Unreachable, but needed for compilation
}

/// @brief Specialization for 1-byte types (int8_t, uint8_t, char, etc.)
template<>
constexpr HexIntWidth get_hex_int_width<1>() {
    return HexIntWidth::Width8;
}

/// @brief Specialization for 2-byte types (int16_t, uint16_t, short, etc.)
template<>
constexpr HexIntWidth get_hex_int_width<2>() {
    return HexIntWidth::Width16;
}

/// @brief Specialization for 4-byte types (int32_t, uint32_t, int, etc.)
template<>
constexpr HexIntWidth get_hex_int_width<4>() {
    return HexIntWidth::Width32;
}

/// @brief Specialization for 8-byte types (int64_t, uint64_t, long long, etc.)
template<>
constexpr HexIntWidth get_hex_int_width<8>() {
    return HexIntWidth::Width64;
}

} // namespace detail

// Implementation of to_hex template function
template<typename T>
fl::string to_hex(T value, bool uppercase, bool pad_to_width) {
    // Determine width classification at compile time
    constexpr auto width = detail::get_hex_int_width<sizeof(T)>();

    // Handle signed types
    bool is_negative = false;
    uint64_t unsigned_value;

    // Check if type is signed and value is negative
    if (static_cast<int64_t>(value) < 0 && sizeof(T) <= 8) {
        // Only handle negative for types that fit in int64_t
        is_negative = true;
        // Convert to positive value for hex conversion
        unsigned_value = static_cast<uint64_t>(-static_cast<int64_t>(value));
    } else {
        unsigned_value = static_cast<uint64_t>(value);
    }

    return detail::hex(unsigned_value, width, is_negative, uppercase, pad_to_width);
}

// Low-level integer to string conversion functions
// These provide the foundational conversion logic for fl::string and fl::sstream

/// @brief Convert signed 32-bit integer to string buffer in given radix
/// @param value The integer value to convert
/// @param buffer Output buffer (must be at least 34 bytes for base 2, 12 for base 10, 12 for base 16)
/// @param radix Number base (2-36, typically 10 for decimal, 16 for hex, 8 for octal)
/// @return Number of characters written (excluding null terminator)
int itoa(int32_t value, char *buffer, int radix);

/// @brief Convert unsigned 32-bit integer to string buffer in given radix
/// @param value The unsigned integer value to convert
/// @param buffer Output buffer (must be at least 33 bytes for base 2, 11 for base 10, 9 for base 16)
/// @param radix Number base (2-36, typically 10 for decimal, 16 for hex, 8 for octal)
/// @return Number of characters written (excluding null terminator)
int utoa32(uint32_t value, char *buffer, int radix);

/// @brief Convert unsigned 64-bit integer to string buffer in given radix
/// @param value The unsigned 64-bit integer value to convert
/// @param buffer Output buffer (must be at least 65 bytes for base 2, 21 for base 10, 17 for base 16)
/// @param radix Number base (2-36, typically 10 for decimal, 16 for hex, 8 for octal)
/// @return Number of characters written (excluding null terminator)
int utoa64(uint64_t value, char *buffer, int radix);

/// @brief Convert floating point number to string buffer
/// @param value The float value to convert
/// @param buffer Output buffer (must be at least 64 bytes)
/// @param precision Number of decimal places (default: 2)
void ftoa(float value, char *buffer, int precision = 2);

} // namespace fl
