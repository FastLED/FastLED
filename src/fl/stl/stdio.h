#pragma once

#include "fl/int.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstdio.h"  // For fl::print and fl::println
#include "fl/stl/string.h"  // For fl::string, to_hex
#include "fl/stl/strstream.h"
#include "fl/stl/type_traits.h"

namespace fl {

/// @brief Printf-like formatting function that prints directly to the platform output
/// @param format Format string with placeholders like "%d", "%s", "%f" etc.
/// @param args Arguments to format
/// 
/// Supported format specifiers:
/// - %d, %i: integers (all integral types)
/// - %u: unsigned integers
/// - %o: octal integers
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %p: pointers (formatted as 0x... hex)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// int* ptr = &value;
/// fl::printf("Pointer: %p", ptr);
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
/// - %o: octal integers
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %p: pointers (formatted as 0x... hex)
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
    int width = 0;             // Minimum field width
    bool uppercase = false;    // For hex formatting
    bool left_align = false;   // '-' flag: left-align
    bool zero_pad = false;     // '0' flag: zero-padding
    bool show_sign = false;    // '+' flag: always show sign
    bool space_sign = false;   // ' ' flag: space for positive
    bool alt_form = false;     // '#' flag: alternate form (0x, 0)

    FormatSpec() = default;
    explicit FormatSpec(char t) : type(t) {}
    FormatSpec(char t, int prec) : type(t), precision(prec) {}
};

// Parse a format specifier from the format string
// Returns the format spec and advances the pointer past the specifier
// Format: %[flags][width][.precision][length]type
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

    // Parse flags: -, +, space, #, 0 (can be in any order)
    bool parsing_flags = true;
    while (parsing_flags) {
        switch (*format) {
            case '-':
                spec.left_align = true;
                ++format;
                break;
            case '+':
                spec.show_sign = true;
                ++format;
                break;
            case ' ':
                spec.space_sign = true;
                ++format;
                break;
            case '#':
                spec.alt_form = true;
                ++format;
                break;
            case '0':
                spec.zero_pad = true;
                ++format;
                break;
            default:
                parsing_flags = false;
                break;
        }
    }

    // Parse width (decimal number)
    if (*format >= '0' && *format <= '9') {
        spec.width = 0;
        while (*format >= '0' && *format <= '9') {
            spec.width = spec.width * 10 + (*format - '0');
            ++format;
        }
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

    // Skip length modifiers (l, ll, h, hh, L, z, t, j)
    // We don't use them in our simple printf, but we need to skip them
    // to get to the actual type character
    if (*format == 'h' || *format == 'l') {
        char first = *format;
        ++format;
        // Check for double character modifiers (hh, ll)
        if (*format == first) {
            ++format;
        }
    } else if (*format == 'L' || *format == 'z' || *format == 't' || *format == 'j') {
        ++format;
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

// Convert unsigned integer to octal string
template<typename T>
inline fl::string to_octal(T value) {
    if (value == 0) {
        return "0";
    }

    char buffer[32]; // Enough for 64-bit octal
    int pos = 31;
    buffer[pos] = '\0';

    unsigned long long val = static_cast<unsigned long long>(value);
    while (val > 0) {
        buffer[--pos] = '0' + (val & 7);
        val >>= 3;
    }

    return fl::string(&buffer[pos]);
}

// Apply width and padding to a string based on format spec
inline fl::string apply_width(const fl::string& str, const FormatSpec& spec, bool is_numeric = false) {
    int len = static_cast<int>(str.length());

    // No width specified or content already wider
    if (spec.width <= len) {
        return str;
    }

    int padding = spec.width - len;
    char pad_char = ' ';

    // Zero-padding only for numeric types and right-align
    if (spec.zero_pad && is_numeric && !spec.left_align) {
        pad_char = '0';

        // Handle sign for zero-padding: move sign to front
        if (!str.empty() && (str[0] == '-' || str[0] == '+' || str[0] == ' ')) {
            fl::string result;
            result += str[0]; // Sign first
            for (int i = 0; i < padding; ++i) {
                result += pad_char;
            }
            for (size_t i = 1; i < str.length(); ++i) {
                result += str[i];
            }
            return result;
        }

        // Handle 0x prefix for zero-padding
        if (str.length() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            fl::string result;
            result += str[0]; // '0'
            result += str[1]; // 'x' or 'X'
            for (int i = 0; i < padding; ++i) {
                result += pad_char;
            }
            for (size_t i = 2; i < str.length(); ++i) {
                result += str[i];
            }
            return result;
        }
    }

    fl::string result;
    if (spec.left_align) {
        // Left-align: content then padding
        result = str;
        for (int i = 0; i < padding; ++i) {
            result += pad_char;
        }
    } else {
        // Right-align: padding then content
        for (int i = 0; i < padding; ++i) {
            result += pad_char;
        }
        result += str;
    }

    return result;
}

// Format floating point with specified precision
inline fl::string format_float(float value, int precision) {
    if (precision < 0) {
        // Default precision - use sstream's default behavior
        sstream stream;
        stream << value;
        return stream.str();
    }
    
    // Simple precision formatting
    // This is a basic implementation - could be enhanced
    if (precision == 0) {
        int int_part = static_cast<int>(value + 0.5f); // Round
        sstream stream;
        stream << int_part;
        return stream.str();
    }
    
    // For non-zero precision, use basic rounding
    int multiplier = 1;
    for (int i = 0; i < precision; ++i) {
        multiplier *= 10;
    }

    // Handle rounding correctly for both positive and negative numbers
    float scaled_float = value * multiplier;
    int scaled;
    if (scaled_float >= 0) {
        scaled = static_cast<int>(scaled_float + 0.5f);
    } else {
        scaled = static_cast<int>(scaled_float - 0.5f);
    }

    int int_part = scaled / multiplier;
    int frac_part = scaled % multiplier;

    // For negative numbers, frac_part will be negative, so take absolute value
    if (frac_part < 0) {
        frac_part = -frac_part;
    }

    sstream stream;
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

// Helper to convert pointer to fl::uptr (only instantiated for pointer types)
template<typename T>
typename fl::enable_if<fl::is_pointer<T>::value, fl::uptr>::type
pointer_to_uptr(const T& ptr) {
    const void* vptr = static_cast<const void*>(ptr);
    return reinterpret_cast<fl::uptr>(vptr); // ok reinterpret cast
}

// Overload for non-pointer types (never called at runtime, but needed for compilation)
template<typename T>
typename fl::enable_if<!fl::is_pointer<T>::value, fl::uptr>::type
pointer_to_uptr(const T&) {
    return 0; // Never executed - runtime check prevents this
}

// Format non-pointer types (d, i, u, o, x, X, f, c, s)
template<typename T>
typename fl::enable_if<!fl::is_pointer<T>::value>::type
format_arg(sstream& stream, const FormatSpec& spec, const T& arg) {
    fl::string result;
    bool is_numeric = false;

    switch (spec.type) {
        case 'd':
        case 'i': {
            if (!fl::is_integral<T>::value) {
                result = "<type_error>";
                break;
            }
            is_numeric = true;

            // Convert to string
            sstream temp;
            temp << arg;
            result = temp.str();

            // Handle sign flags
            bool is_negative = !result.empty() && result[0] == '-';
            if (!is_negative) {
                if (spec.show_sign) {
                    result = fl::string("+") + result;
                } else if (spec.space_sign) {
                    result = fl::string(" ") + result;
                }
            }
            break;
        }

        case 'u': {
            if (!fl::is_integral<T>::value) {
                result = "<type_error>";
                break;
            }
            is_numeric = true;

            // Convert to string
            sstream temp;
            temp << arg;
            result = temp.str();
            break;
        }

        case 'o': {
            if (!fl::is_integral<T>::value) {
                result = "<type_error>";
                break;
            }
            is_numeric = true;

            // Convert to octal
            result = to_octal(arg);

            // Alternate form: prefix with 0 (but not for zero itself)
            if (spec.alt_form && arg != 0) {
                result = fl::string("0") + result;
            }
            break;
        }

        case 'x': {
            is_numeric = true;

            // Convert to hex
            result = fl::to_hex(arg, spec.uppercase);

            // Alternate form: prefix with 0x or 0X
            if (spec.alt_form && arg != 0) {
                result = fl::string(spec.uppercase ? "0X" : "0x") + result;
            }
            break;
        }

        case 'f': {
            if (!fl::is_floating_point<T>::value) {
                result = "<type_error>";
                break;
            }
            is_numeric = true;

            if (spec.precision >= 0) {
                result = format_float(static_cast<float>(arg), spec.precision);
            } else {
                sstream temp;
                temp << static_cast<float>(arg);
                result = temp.str();
            }
            break;
        }

        case 'c': {
            if (!fl::is_integral<T>::value) {
                result = "<type_error>";
                break;
            }

            char ch = static_cast<char>(arg);
            char temp_str[2] = {ch, '\0'};
            result = temp_str;
            break;
        }

        case 's': {
            sstream temp;
            temp << arg;
            result = temp.str();
            break;
        }

        default:
            result = "<unknown_format>";
            break;
    }

    // Apply width and padding
    result = apply_width(result, spec, is_numeric);

    // Output final result
    stream << result;
}

// Format pointer types (only handles 'p' format)
template<typename T>
typename fl::enable_if<fl::is_pointer<T>::value>::type
format_arg(sstream& stream, const FormatSpec& spec, const T& arg) {
    fl::string result;
    bool is_numeric = false;

    if (spec.type == 'p') {
        // Pointer format - always prefix with "0x"
        is_numeric = true;

        // Convert pointer to integer address using SFINAE helper
        fl::uptr addr = pointer_to_uptr(arg);

        // Format as hex with 0x prefix (always use lowercase for standard compatibility)
        result = fl::string("0x") + fl::to_hex(addr, false);
    } else {
        result = "<type_error>";
    }

    // Apply width and padding
    result = apply_width(result, spec, is_numeric);

    // Output final result
    stream << result;
}

// Specialized format_arg for const char* (string literals)
inline void format_arg(sstream& stream, const FormatSpec& spec, const char* arg) {
    fl::string result;

    bool is_numeric = false;

    switch (spec.type) {
        case 's':
            if (arg) {
                result = arg;
            } else {
                result = "(null)";
            }
            break;
        case 'p': {
            // Pointer format for const char*
            is_numeric = true;
            const void* ptr = static_cast<const void*>(arg);
            fl::uptr addr = reinterpret_cast<fl::uptr>(ptr); // ok reinterpret cast
            result = fl::string("0x") + fl::to_hex(addr, false);
            break;
        }
        case 'x':
            result = "<string_not_hex>";
            break;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'f':
        case 'c':
            result = "<type_error>";
            break;
        default:
            result = "<unknown_format>";
            break;
    }

    // Apply width and padding
    result = apply_width(result, spec, is_numeric);
    stream << result;
}

// Specialized format_arg for char arrays (string literals like "hello")
template<fl::size N>
void format_arg(sstream& stream, const FormatSpec& spec, const char (&arg)[N]) {
    format_arg(stream, spec, static_cast<const char*>(arg));
}

// Base case: no more arguments
inline void format_impl(sstream& stream, const char* format) {
    while (*format) {
        if (*format == '%') {
            FormatSpec spec = parse_format_spec(format);
            if (spec.type == '%') {
                stream << "%";
                continue;
            } else {
                // No argument for format specifier
                stream << "<missing_arg>";
                continue;
            }
        } else {
            // Create a single-character string since sstream treats char as number
            char temp_str[2] = {*format, '\0'};
            stream << temp_str;
            ++format;
        }
    }
}

// Recursive case: process one argument and continue
template<typename T, typename... Args>
void format_impl(sstream& stream, const char* format, const T& first, const Args&... rest) {
    while (*format) {
        if (*format == '%') {
            FormatSpec spec = parse_format_spec(format);
            if (spec.type == '%') {
                stream << "%";
                continue;
            } else {
                // Format the first argument and continue with the rest
                format_arg(stream, spec, first);
                format_impl(stream, format, rest...);
                return;
            }
        } else {
            // Create a single-character string since sstream treats char as number
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
/// - %o: octal integers
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %p: pointers (formatted as 0x... hex)
/// - %%: literal % character
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// @endcode
template<typename... Args>
void printf(const char* format, const Args&... args) {
    sstream stream;
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
/// - %o: octal integers
/// - %f: floating point numbers
/// - %s: strings (const char*, fl::string)
/// - %c: characters
/// - %x: hexadecimal (lowercase)
/// - %X: hexadecimal (uppercase)
/// - %p: pointers (formatted as 0x... hex)
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
    sstream stream;
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
