#pragma once

#include "fl/strstream.h"
#include "fl/namespace.h"
#include "fl/type_traits.h"
#include "fl/str.h"
#include "fl/int.h"
#include "fl/io.h"  // For fl::print and fl::println

namespace fl {

/// @brief Printf-like formatting function that prints directly to the platform output
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// 
/// Supported format specifiers:
/// - %d, %i: integers (all integral types)
/// - %u: unsigned integers  
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// @endcode
template<typename... Args>
void printf(const char* format, const Args&... args);

/// @brief Snprintf-like formatting function that writes to a buffer
/// @param buffer Output buffer to write formatted string to
/// @param size Maximum number of characters to write (including null terminator)
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// @return Number of characters that would have been written if buffer was large enough
/// 
/// Supported format specifiers:
/// - %d, %i: integers (all integral types)
/// - %u: unsigned integers  
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::snprintf(buffer, sizeof(buffer), "Value: %d, Name: %s", 42, "test");
/// @endcode
template<typename... Args>
int snprintf(char* buffer, fl::size size, const char* format, const Args&... args);

/// @brief Sprintf-like formatting function that writes to a buffer
/// @param buffer Output buffer to write formatted string to
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// @return Number of characters written (excluding null terminator)
/// 
/// This function writes a formatted string to the provided buffer.
/// The buffer size is deduced at compile time from the array reference,
/// providing automatic safety against buffer overflows.
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::sprintf(buffer, "Value: %d, Name: %s", 42, "test");
/// @endcode
template<fl::size N, typename... Args>
int sprintf(char (&buffer)[N], const char* format, const Args&... args);


///////////////////// IMPLEMENTATION /////////////////////

namespace printf_detail {

// Helper to parse format specifiers and extract precision
struct FormatSpec {
    char type = '\0';          // Format character (d, f, s, etc.)
    int precision = -1;        // Precision for floating point
    bool uppercase = false;    // For hex formatting
    
    FormatSpec() = default;
    explicit FormatSpec(char t) : type(t) {}
    FormatSpec(char t, int prec) : type(t), precision(prec) {}
};

// Parse a format specifier from the format string
// Returns the format spec and advances the pointer past the specifier
inline FormatSpec parse_format_spec(const char*& format) {
    FormatSpec spec;
    
    if (*format != '%') {
        return spec;
    }
    
    ++format; // Skip the '%'
    
    // Handle literal '%'
    if (*format == '%') {
        spec.type = '%';
        ++format;
        return spec;
    }
    
    // Parse precision for floating point
    if (*format == '.') {
        ++format; // Skip the '.'
        spec.precision = 0;
        while (*format >= '0' && *format <= '9') {
            spec.precision = spec.precision * 10 + (*format - '0');
            ++format;
        }
    }
    
    // Get the format type
    spec.type = *format;
    if (spec.type == 'X') {
        spec.uppercase = true;
        spec.type = 'x'; // Normalize to lowercase for processing
    }
    
    ++format;
    return spec;
}

// Format floating point with specified precision
inline fl::string format_float(float value, int precision) {
    if (precision < 0) {
        // Default precision - use StrStream's default behavior
        StrStream stream;
        stream << value;
        return stream.str();
    }
    
    // Simple precision formatting
    // This is a basic implementation - could be enhanced
    if (precision == 0) {
        int int_part = static_cast<int>(value + 0.5f); // Round
        StrStream stream;
        stream << int_part;
        return stream.str();
    }
    
    // For non-zero precision, use basic rounding
    int multiplier = 1;
    for (int i = 0; i < precision; ++i) {
        multiplier *= 10;
    }
    
    int scaled = static_cast<int>(value * multiplier + 0.5f);
    int int_part = scaled / multiplier;
    int frac_part = scaled % multiplier;
    
    StrStream stream;
    stream << int_part;
    stream << ".";
    
    // Pad fractional part with leading zeros if needed
    int temp_multiplier = multiplier / 10;
    while (temp_multiplier > frac_part && temp_multiplier > 1) {
        stream << "0";
        temp_multiplier /= 10;
    }
    if (frac_part > 0) {
        stream << frac_part;
    }
    
    return stream.str();
}

// Convert integer to hex string - only for integral types
template<typename T>
typename fl::enable_if<fl::is_integral<T>::value, fl::string>::type 
to_hex(T value, bool uppercase) {
    if (value == 0) {
        return fl::string("0");
    }
    
    fl::string result;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    // Handle negative values for signed types
    bool negative = false;
    if (fl::is_signed<T>::value && value < 0) {
        negative = true;
        value = -value;
    }
    
    while (value > 0) {
        char ch = digits[value % 16];
        // Convert char to string since fl::string::append treats char as number
        char temp_ch_str[2] = {ch, '\0'};
        fl::string digit_str(temp_ch_str);
        // Use += since + operator is not defined for fl::string
        fl::string temp = digit_str;
        temp += result;
        result = temp;
        value /= 16;
    }
    
    if (negative) {
        fl::string minus_str("-");
        minus_str += result;
        result = minus_str;
    }
    
    return result;
}

// Non-integral types return error string
template<typename T>
typename fl::enable_if<!fl::is_integral<T>::value, fl::string>::type 
to_hex(T, bool) {
    return fl::string("<not_integral>");
}

// Format a single argument based on format specifier with better type handling
template<typename T>
void format_arg(StrStream& stream, const FormatSpec& spec, const T& arg) {
    switch (spec.type) {
        case 'd':
        case 'i':
            if (fl::is_integral<T>::value) {
                stream << arg;
            } else {
                stream << "<type_error>";
            }
            break;
            
        case 'u':
            if (fl::is_integral<T>::value) {
                // Convert unsigned manually since StrStream treats all as signed
                unsigned int val = static_cast<unsigned int>(arg);
                if (val == 0) {
                    stream << "0";
                } else {
                    fl::string result;
                    while (val > 0) {
                        char digit = '0' + (val % 10);
                        char temp_str[2] = {digit, '\0'};
                        fl::string digit_str(temp_str);
                        fl::string temp = digit_str;
                        temp += result;
                        result = temp;
                        val /= 10;
                    }
                    stream << result;
                }
            } else {
                stream << "<type_error>";
            }
            break;
            
        case 'f':
            if (fl::is_floating_point<T>::value) {
                if (spec.precision >= 0) {
                    stream << format_float(static_cast<float>(arg), spec.precision);
                } else {
                    stream << arg;
                }
            } else {
                stream << "<type_error>";
            }
            break;
            
        case 'c':
            if (fl::is_integral<T>::value) {
                char ch = static_cast<char>(arg);
                // Convert char to string since StrStream treats char as number
                char temp_str[2] = {ch, '\0'};
                stream << temp_str;
            } else {
                stream << "<type_error>";
            }
            break;
            
        case 'x':
            stream << to_hex(arg, spec.uppercase);
            break;
            
        case 's':
            stream << arg; // StrStream handles string conversion
            break;
            
        default:
            stream << "<unknown_format>";
            break;
    }
}

// Specialized format_arg for const char* (string literals)
inline void format_arg(StrStream& stream, const FormatSpec& spec, const char* arg) {
    switch (spec.type) {
        case 's':
            stream << arg;
            break;
        case 'x':
            stream << "<string_not_hex>";
            break;
        case 'd':
        case 'i':
        case 'u':
        case 'f':
        case 'c':
            stream << "<type_error>";
            break;
        default:
            stream << "<unknown_format>";
            break;
    }
}

// Specialized format_arg for char arrays (string literals like "hello")
template<fl::size N>
void format_arg(StrStream& stream, const FormatSpec& spec, const char (&arg)[N]) {
    format_arg(stream, spec, static_cast<const char*>(arg));
}

// Base case: no more arguments
inline void format_impl(StrStream& stream, const char* format) {
    while (*format) {
        if (*format == '%') {
            FormatSpec spec = parse_format_spec(format);
            if (spec.type == '%') {
                stream << "%";
            } else {
                // No argument for format specifier
                stream << "<missing_arg>";
            }
        } else {
            // Create a single-character string since StrStream treats char as number
            char temp_str[2] = {*format, '\0'};
            stream << temp_str;
            ++format;
        }
    }
}

// Recursive case: process one argument and continue
template<typename T, typename... Args>
void format_impl(StrStream& stream, const char* format, const T& first, const Args&... rest) {
    while (*format) {
        if (*format == '%') {
            FormatSpec spec = parse_format_spec(format);
            if (spec.type == '%') {
                stream << "%";
            } else {
                // Format the first argument and continue with the rest
                format_arg(stream, spec, first);
                format_impl(stream, format, rest...);
                return;
            }
        } else {
            // Create a single-character string since StrStream treats char as number
            char temp_str[2] = {*format, '\0'};
            stream << temp_str;
            ++format;
        }
    }
    
    // If we get here, there are unused arguments
    // This is not an error in printf, so we just ignore them
}

}

/// @brief Printf-like formatting function that prints directly to the platform output
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// 
/// Supported format specifiers:
/// - %d, %i: integers (all integral types)
/// - %u: unsigned integers  
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// @endcode
template<typename... Args>
void printf(const char* format, const Args&... args) {
    StrStream stream;
    printf_detail::format_impl(stream, format, args...);
    fl::print(stream.str().c_str());
}

/// @brief Snprintf-like formatting function that writes to a buffer
/// @param buffer Output buffer to write formatted string to
/// @param size Maximum number of characters to write (including null terminator)
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// @return Number of characters that would have been written if buffer was large enough
/// 
/// Supported format specifiers:
/// - %d, %i: integers (all integral types)
/// - %u: unsigned integers  
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::snprintf(buffer, sizeof(buffer), "Value: %d, Name: %s", 42, "test");
/// @endcode
template<typename... Args>
int snprintf(char* buffer, fl::size size, const char* format, const Args&... args) {
    // Handle null buffer or zero size
    if (!buffer || size == 0) {
        return 0;
    }
    
    // Format to internal string stream
    StrStream stream;
    printf_detail::format_impl(stream, format, args...);
    fl::string result = stream.str();
    
    // Get the formatted string length
    fl::size formatted_len = result.size();
    
    // Copy to buffer, ensuring null termination
    fl::size copy_len = (formatted_len < size - 1) ? formatted_len : size - 1;
    
    // Copy characters
    for (fl::size i = 0; i < copy_len; ++i) {
        buffer[i] = result[i];
    }
    
    // Null terminate
    buffer[copy_len] = '\0';
    
    // Return the number of characters actually written (excluding null terminator)
    // This respects the buffer size limit instead of returning the full formatted length
    return static_cast<int>(copy_len);
}

/// @brief Sprintf-like formatting function that writes to a buffer
/// @param buffer Output buffer to write formatted string to
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// @return Number of characters written (excluding null terminator)
/// 
/// This function writes a formatted string to the provided buffer.
/// The buffer size is deduced at compile time from the array reference,
/// providing automatic safety against buffer overflows.
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::sprintf(buffer, "Value: %d, Name: %s", 42, "test");
/// @endcode
template<fl::size N, typename... Args>
int sprintf(char (&buffer)[N], const char* format, const Args&... args) {
    // Use the compile-time known buffer size for safety
    return snprintf(buffer, N, format, args...);
}

} // namespace fl
