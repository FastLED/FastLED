// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

#include "fl/stl/int.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstdio.h"  // For fl::print and fl::println
#include "fl/stl/string.h"  // For fl::string, to_hex
#include "fl/stl/strstream.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

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
/// Generic placeholder:
/// - {}: formats the next argument by picking the natural specifier for its
///   C++ type, so callers do not have to choose between %d / %u / %ld / %s /
///   %p or call .c_str(). Supported argument types are the same set the %
///   specifiers support: all integral types (%d), bool (as "true"/"false"),
///   char (as a character, %c), float/double (%f), const char* and
///   fl::string / fl::string_view (%s), pointers (%p), and unscoped enums
///   (as their underlying integer). Scoped enums (enum class) do not compile
///   through {} or %d; cast at the call site.
///   Passing any other type to {} is a compile error - {} deliberately does
///   not fall back to a generic streaming path, to keep code size flat.
/// - {{: literal { character
/// - }}: literal } character
/// - A lone { or } that is not part of the above passes through literally.
/// - A {} with no remaining argument emits <missing_arg>, matching the
///   behavior of a % specifier with no remaining argument.
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// int* ptr = &value;
/// fl::printf("Pointer: %p", ptr);
/// @endcode
template<typename... Args>
void printf(const char* format, const Args&... args) FL_NO_EXCEPT;

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
/// Generic placeholder:
/// - {}: formats the next argument by picking the natural specifier for its
///   C++ type, so callers do not have to choose between %d / %u / %ld / %s /
///   %p or call .c_str(). Supported argument types are the same set the %
///   specifiers support: all integral types (%d), bool (as "true"/"false"),
///   char (as a character, %c), float/double (%f), const char* and
///   fl::string / fl::string_view (%s), pointers (%p), and unscoped enums
///   (as their underlying integer). Scoped enums (enum class) do not compile
///   through {} or %d; cast at the call site.
///   Passing any other type to {} is a compile error - {} deliberately does
///   not fall back to a generic streaming path, to keep code size flat.
/// - {{: literal { character
/// - }}: literal } character
/// - A lone { or } that is not part of the above passes through literally.
/// - A {} with no remaining argument emits <missing_arg>, matching the
///   behavior of a % specifier with no remaining argument.
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::snprintf(buffer, sizeof(buffer), "Value: %d, Name: %s", 42, "test");
/// @endcode
template<typename... Args>
int snprintf(char* buffer, fl::size size, const char* format, const Args&... args) FL_NO_EXCEPT;

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
int sprintf(char (&buffer)[N], const char* format, const Args&... args) FL_NO_EXCEPT;


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
    explicit FormatSpec(char t) FL_NO_EXCEPT : type(t) {}
    FormatSpec(char t, int prec) FL_NO_EXCEPT : type(t), precision(prec) {}
};

// Parse a format specifier from the format string
// Returns the format spec and advances the pointer past the specifier
// Format: %[flags][width][.precision][length]type
inline FormatSpec parse_format_spec(const char*& format) FL_NO_EXCEPT {
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

    // Only advance past the type character if there actually is one. A format
    // string ending in a bare '%' leaves spec.type == '\0'; advancing here
    // would step past the NUL terminator and make the caller's loop read out
    // of bounds.
    if (spec.type != '\0') {
        ++format;
    }
    return spec;
}

// Convert unsigned integer to octal string
template<typename T>
inline fl::string to_octal(T value) FL_NO_EXCEPT {
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
inline fl::string apply_width(const fl::string& str, const FormatSpec& spec, bool is_numeric = false) FL_NO_EXCEPT {
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
inline fl::string format_float(float value, int precision) FL_NO_EXCEPT {
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
    // Preserve sign for values like -0.5 that round to int_part=0 (printed
    // without sign by the integer write below).
    if (value < 0 && int_part == 0) {
        stream << "-";
    }
    stream << int_part;
    stream << ".";

    // Emit exactly `precision` fractional digits, zero-padded on the left.
    // The previous implementation stopped padding once temp_multiplier dropped
    // to 1, which produced "0.0"/"1.0" instead of "0.00"/"1.00" for any
    // integer-valued input. It also omitted the digits entirely when
    // frac_part == 0.
    int temp_multiplier = multiplier / 10;
    while (temp_multiplier > 0) {
        int digit = (frac_part / temp_multiplier) % 10;
        stream << static_cast<char>('0' + digit);
        temp_multiplier /= 10;
    }

    return stream.str();
}

// Helper to convert pointer to fl::uptr (only instantiated for pointer types)
template<typename T>
typename fl::enable_if<fl::is_pointer<T>::value, fl::uptr>::type
pointer_to_uptr(const T& ptr) FL_NO_EXCEPT {
    const void* vptr = static_cast<const void*>(ptr);
    return reinterpret_cast<fl::uptr>(vptr); // ok reinterpret cast
}

// Overload for non-pointer types (never called at runtime, but needed for compilation)
template<typename T>
typename fl::enable_if<!fl::is_pointer<T>::value, fl::uptr>::type
pointer_to_uptr(const T&) FL_NO_EXCEPT {
    return 0; // Never executed - runtime check prevents this
}

// Format non-pointer types (d, i, u, o, x, X, f, c, s)
template<typename T>
typename fl::enable_if<!fl::is_pointer<T>::value>::type
format_arg(sstream& stream, const FormatSpec& spec, const T& arg) FL_NO_EXCEPT {
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
format_arg(sstream& stream, const FormatSpec& spec, const T& arg) FL_NO_EXCEPT {
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
inline void format_arg(sstream& stream, const FormatSpec& spec, const char* arg) FL_NO_EXCEPT {
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

void format_arg(sstream& stream, const FormatSpec& spec, const fl::string& arg) FL_NO_EXCEPT;
void format_arg(sstream& stream, const FormatSpec& spec, const fl::string_view& arg) FL_NO_EXCEPT;

///////////////// GENERIC "{}" PLACEHOLDER SUPPORT /////////////////

// The generic "{}" placeholder saves the caller from hand-picking between
// %d / %u / %ld / %s / %p and from writing .c_str(): the natural specifier for
// the argument's C++ type is selected automatically and then handed to the
// SAME format_arg machinery the '%' path uses. That reuse is deliberate - it
// shares instantiations with the '%' path: a TU compiled with '{}' emits an
// identical symbol table to the same TU written with the hand-picked '%'
// specifier (verified with nm over 10 argument types).
//
// Caveat, so nobody is surprised: format_impl's '{}' branch is compiled even
// for call sites that only use '%', because the format string is a runtime
// pointer and the branch cannot be discarded. Those call sites therefore emit
// small format_arg_generic<T> forwarders they did not previously have. Each is
// a one-line tail call into an already-existing format_arg<T> and inlines at
// -Os, but the cost is paid tree-wide, not only by '{}' users.
//
// "{}" is intentionally constrained to the type set '%' already supports
// (integral, bool, char, float/double, const char*, fl::string,
// fl::string_view, pointers). It does NOT fall back to sstream::operator<<
// for arbitrary types: that would re-introduce the per-call-site operator<<
// chain instantiations that issue #3158 removed (measured at ~5-10 KB on
// ESP32-S3, with sstream::operator<<(char) alone at 1.25 KB per #3078).
// Passing an unsupported type to "{}" is therefore a compile error, not a
// silent code-size regression.

// Integral types (including bool) format like "%d". The 'd' path stringifies
// through the same code as %d/%u, so signed and unsigned both render exactly
// as they do today; bool keeps its "true"/"false" rendering.
template<typename T>
typename fl::enable_if<fl::is_integral<T>::value>::type
format_arg_generic(sstream& stream, const T& arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('d'), arg);
}

// Enums format as their underlying integer, like "%d" and like sstream's own
// is_enum overload. This overload is REQUIRED for correctness, not just
// convenience: an unscoped enum fails is_integral, but implicitly converts to
// char, so without it overload resolution silently selects the char overload
// below and `fl::printf("{}", GREEN)` with `enum Color { GREEN = 66 }` prints
// "B" instead of "66". Same trap applies to any type with an implicit
// conversion to char.
template<typename T>
typename fl::enable_if<fl::is_enum<T>::value>::type
format_arg_generic(sstream& stream, const T& arg) FL_NO_EXCEPT {
    using underlying_t = typename fl::underlying_type<T>::type;
    format_arg(stream, FormatSpec('d'), static_cast<underlying_t>(arg));
}

// Floating point types format like "%f" (default precision).
template<typename T>
typename fl::enable_if<fl::is_floating_point<T>::value>::type
format_arg_generic(sstream& stream, const T& arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('f'), arg);
}

// Pointers format like "%p" ("0x" + lowercase hex address).
// Character pointers are handled by the more specialized overloads below.
template<typename T>
typename fl::enable_if<fl::is_pointer<T>::value>::type
format_arg_generic(sstream& stream, const T& arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('p'), arg);
}

// char renders as a character, like "%c" (not as its numeric value).
inline void format_arg_generic(sstream& stream, char arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('c'), arg);
}

// C strings render their contents, like "%s" (including the "(null)"
// sentinel for a null pointer).
inline void format_arg_generic(sstream& stream, const char* arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('s'), arg);
}

inline void format_arg_generic(sstream& stream, char* arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('s'), static_cast<const char*>(arg));
}

// NOTE: no const char(&)[N] overload here on purpose. Array-to-pointer is an
// lvalue transformation excluded from ICS ranking, so a string literal is an
// Exact Match for BOTH an array template and the non-template const char*
// overload above -- and the non-template wins the tiebreak. An array overload
// would be dead code, and adding one would only risk a per-N instantiation.

// fl::string / fl::string_view render like "%s" - no .c_str() needed.
inline void format_arg_generic(sstream& stream, const fl::string& arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('s'), arg);
}

inline void format_arg_generic(sstream& stream, const fl::string_view& arg) FL_NO_EXCEPT {
    format_arg(stream, FormatSpec('s'), arg);
}


// Handle a '{' or '}' found in the format string.
// Returns true if a generic "{}" placeholder was consumed, in which case the
// caller is responsible for emitting an argument. Otherwise the brace text was
// written to `stream` literally. Either way `format` is advanced past the
// characters that were consumed, so callers must `continue` (never fall through
// to an extra ++format) after calling this.
//
// Rules:
// - "{}" is a generic placeholder
// - "{{" is a literal '{' and "}}" is a literal '}'
// - a lone '{' or '}' passes through literally rather than being an error
inline bool parse_brace(sstream& stream, const char*& format) FL_NO_EXCEPT {
    if (*format == '{') {
        if (format[1] == '}') {
            format += 2;
            return true;
        }
        if (format[1] == '{') {
            stream << "{";
            format += 2;
            return false;
        }
        stream << "{";
        ++format;
        return false;
    }

    // *format == '}'
    if (format[1] == '}') {
        stream << "}";
        format += 2;
        return false;
    }
    stream << "}";
    ++format;
    return false;
}

// Specialized format_arg for char arrays (string literals like "hello")
template<fl::size N>
void format_arg(sstream& stream, const FormatSpec& spec, const char (&arg)[N]) FL_NO_EXCEPT {
    format_arg(stream, spec, static_cast<const char*>(arg));
}

// Base case: no more arguments
inline void format_impl(sstream& stream, const char* format) FL_NO_EXCEPT {
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
        } else if (*format == '{' || *format == '}') {
            if (parse_brace(stream, format)) {
                // Generic placeholder with no argument left to consume.
                stream << "<missing_arg>";
            }
            continue;
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
void format_impl(sstream& stream, const char* format, const T& first, const Args&... rest) FL_NO_EXCEPT {
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
        } else if (*format == '{' || *format == '}') {
            if (parse_brace(stream, format)) {
                // Generic placeholder: format the first argument by
                // picking the natural specifier for its type (NOT through
                // sstream::operator<<, which is what this design avoids) and
                // continue with the rest.
                format_arg_generic(stream, first);
                format_impl(stream, format, rest...);
                return;
            }
            continue;
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
/// Generic placeholder:
/// - {}: formats the next argument by picking the natural specifier for its
///   C++ type, so callers do not have to choose between %d / %u / %ld / %s /
///   %p or call .c_str(). Supported argument types are the same set the %
///   specifiers support: all integral types (%d), bool (as "true"/"false"),
///   char (as a character, %c), float/double (%f), const char* and
///   fl::string / fl::string_view (%s), pointers (%p), and unscoped enums
///   (as their underlying integer). Scoped enums (enum class) do not compile
///   through {} or %d; cast at the call site.
///   Passing any other type to {} is a compile error - {} deliberately does
///   not fall back to a generic streaming path, to keep code size flat.
/// - {{: literal { character
/// - }}: literal } character
/// - A lone { or } that is not part of the above passes through literally.
/// - A {} with no remaining argument emits <missing_arg>, matching the
///   behavior of a % specifier with no remaining argument.
///
/// Example usage:
/// @code
/// fl::printf("Value: %d, Name: %s", 42, "test");
/// fl::printf("Float: %.2f", 3.14159);
/// @endcode
template<typename... Args>
void printf(const char* format, const Args&... args) FL_NO_EXCEPT {
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
/// Generic placeholder:
/// - {}: formats the next argument by picking the natural specifier for its
///   C++ type, so callers do not have to choose between %d / %u / %ld / %s /
///   %p or call .c_str(). Supported argument types are the same set the %
///   specifiers support: all integral types (%d), bool (as "true"/"false"),
///   char (as a character, %c), float/double (%f), const char* and
///   fl::string / fl::string_view (%s), pointers (%p), and unscoped enums
///   (as their underlying integer). Scoped enums (enum class) do not compile
///   through {} or %d; cast at the call site.
///   Passing any other type to {} is a compile error - {} deliberately does
///   not fall back to a generic streaming path, to keep code size flat.
/// - {{: literal { character
/// - }}: literal } character
/// - A lone { or } that is not part of the above passes through literally.
/// - A {} with no remaining argument emits <missing_arg>, matching the
///   behavior of a % specifier with no remaining argument.
///
/// Example usage:
/// @code
/// char buffer[100];
/// int len = fl::snprintf(buffer, sizeof(buffer), "Value: %d, Name: %s", 42, "test");
/// @endcode
template<typename... Args>
int snprintf(char* buffer, fl::size size, const char* format, const Args&... args) FL_NO_EXCEPT {
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
int sprintf(char (&buffer)[N], const char* format, const Args&... args) FL_NO_EXCEPT {
    // Use the compile-time known buffer size for safety
    return snprintf(buffer, N, format, args...);
}

} // namespace fl
