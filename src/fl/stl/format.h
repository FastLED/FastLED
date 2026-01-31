#pragma once

/// @file format.h
/// @brief C++20 std::format-style string formatting for fl::string
///
/// Provides fl::format() function with Python/fmtlib-style {} placeholders.
///
/// Basic usage:
///   fl::format("Hello {}!", "World")           -> "Hello World!"
///   fl::format("{} + {} = {}", 2, 3, 5)        -> "2 + 3 = 5"
///
/// Indexed arguments:
///   fl::format("{1} before {0}", "A", "B")     -> "B before A"
///
/// Format specifiers (after colon):
///   fl::format("{:d}", 42)                     -> "42"        (decimal)
///   fl::format("{:x}", 255)                    -> "ff"        (hex lowercase)
///   fl::format("{:X}", 255)                    -> "FF"        (hex uppercase)
///   fl::format("{:b}", 5)                      -> "101"       (binary)
///   fl::format("{:o}", 8)                      -> "10"        (octal)
///   fl::format("{:f}", 3.14)                   -> "3.140000"  (fixed float)
///   fl::format("{:.2f}", 3.14159)              -> "3.14"      (precision)
///   fl::format("{:c}", 65)                     -> "A"         (character)
///
/// Width and alignment:
///   fl::format("{:10}", 42)                    -> "        42" (right-align default)
///   fl::format("{:<10}", 42)                   -> "42        " (left-align)
///   fl::format("{:>10}", 42)                   -> "        42" (right-align)
///   fl::format("{:^10}", 42)                   -> "    42    " (center)
///   fl::format("{:*^10}", 42)                  -> "****42****" (fill char)
///   fl::format("{:05}", 42)                    -> "00042"      (zero-pad)
///
/// Signs and prefixes:
///   fl::format("{:+}", 42)                     -> "+42"       (always show sign)
///   fl::format("{:#x}", 255)                   -> "0xff"      (alternate form)
///   fl::format("{:#b}", 5)                     -> "0b101"     (binary prefix)
///
/// Escaping braces:
///   fl::format("{{}}}")                        -> "{}"

#include "fl/stl/string.h"
#include "fl/stl/cstring.h"
#include "fl/stl/charconv.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

namespace fl {

namespace format_detail {

// Format specification parsed from {:...}
struct FormatSpec {
    char fill = ' ';           // Fill character for alignment
    char align = '\0';         // '<' left, '>' right, '^' center, '\0' default
    char sign = '-';           // '+' always, '-' negative only, ' ' space for positive
    bool alternate = false;    // '#' alternate form (0x, 0b, etc.)
    bool zero_pad = false;     // '0' zero padding
    int width = 0;             // Minimum field width
    int precision = -1;        // Precision for floats (-1 = default)
    char type = '\0';          // 'd', 'x', 'X', 'b', 'o', 'f', 's', 'c', etc.
};

// Parse a format specification from the string after ':'
// Returns pointer past the parsed spec (should be at '}')
inline const char* parse_format_spec(const char* p, FormatSpec& spec) {
    if (!p || *p == '}') return p;

    // Check for fill and align (fill char followed by align char)
    // Align chars are: < > ^
    if (p[0] && p[1] && (p[1] == '<' || p[1] == '>' || p[1] == '^')) {
        spec.fill = p[0];
        spec.align = p[1];
        p += 2;
    } else if (*p == '<' || *p == '>' || *p == '^') {
        spec.align = *p++;
    }

    // Sign: + - or space
    if (*p == '+' || *p == '-' || *p == ' ') {
        spec.sign = *p++;
    }

    // Alternate form: #
    if (*p == '#') {
        spec.alternate = true;
        ++p;
    }

    // Zero padding: 0 (only if no alignment specified)
    if (*p == '0' && spec.align == '\0') {
        spec.zero_pad = true;
        ++p;
    }

    // Width
    while (*p >= '0' && *p <= '9') {
        spec.width = spec.width * 10 + (*p - '0');
        ++p;
    }

    // Precision: .N
    if (*p == '.') {
        ++p;
        spec.precision = 0;
        while (*p >= '0' && *p <= '9') {
            spec.precision = spec.precision * 10 + (*p - '0');
            ++p;
        }
    }

    // Type specifier
    if (*p && *p != '}') {
        spec.type = *p++;
    }

    return p;
}

// Apply width/alignment to a formatted string
inline void apply_width_align(fl::string& result, const fl::string& value, const FormatSpec& spec) {
    fl::size value_len = value.size();

    if (spec.width <= 0 || static_cast<fl::size>(spec.width) <= value_len) {
        result.append(value);
        return;
    }

    fl::size padding = static_cast<fl::size>(spec.width) - value_len;
    char fill = spec.fill;

    char align = spec.align;
    if (align == '\0') {
        align = '>'; // Default right-align for numbers
    }

    if (align == '<') {
        // Left align: value then padding
        result.append(value);
        for (fl::size i = 0; i < padding; ++i) {
            result.append(fill);
        }
    } else if (align == '>') {
        // Right align: padding then value
        for (fl::size i = 0; i < padding; ++i) {
            result.append(fill);
        }
        result.append(value);
    } else if (align == '^') {
        // Center align
        fl::size left_pad = padding / 2;
        fl::size right_pad = padding - left_pad;
        for (fl::size i = 0; i < left_pad; ++i) {
            result.append(fill);
        }
        result.append(value);
        for (fl::size i = 0; i < right_pad; ++i) {
            result.append(fill);
        }
    }
}

// Format an integer value
template <typename T>
fl::string format_integer(T value, const FormatSpec& spec) {
    char buf[68]; // Enough for 64-bit binary + prefix
    char* p = buf + sizeof(buf);
    *--p = '\0';

    bool negative = false;
    using UnsignedT = typename fl::make_unsigned<T>::type;
    UnsignedT uval;

    if (fl::is_signed<T>::value && value < 0) {
        negative = true;
        uval = static_cast<UnsignedT>(-(value + 1)) + 1; // Safe negation
    } else {
        uval = static_cast<UnsignedT>(value);
    }

    char type = spec.type ? spec.type : 'd';
    int base = 10;
    const char* digits = "0123456789abcdef";

    switch (type) {
        case 'b': case 'B': base = 2; break;
        case 'o': base = 8; break;
        case 'x': base = 16; digits = "0123456789abcdef"; break;
        case 'X': base = 16; digits = "0123456789ABCDEF"; break;
        case 'c': {
            // Character output
            char ch = static_cast<char>(value);
            return fl::string(&ch, 1);
        }
        default: base = 10; break;
    }

    // Convert to string (in reverse)
    if (uval == 0) {
        *--p = '0';
    } else {
        while (uval > 0) {
            *--p = digits[uval % base];
            uval /= base;
        }
    }

    // Build prefix
    fl::string prefix;
    if (negative) {
        prefix.append('-');
    } else if (spec.sign == '+') {
        prefix.append('+');
    } else if (spec.sign == ' ') {
        prefix.append(' ');
    }

    if (spec.alternate && base != 10) {
        if (base == 16) {
            prefix.append('0');
            prefix.append('x');  // Always lowercase 'x' in prefix (digits control case)
        } else if (base == 2) {
            prefix.append('0');
            prefix.append('b');  // Always lowercase 'b' in prefix
        } else if (base == 8 && *p != '0') {
            prefix.append('0');
        }
    }

    fl::string num_str(p);

    // Handle zero padding
    if (spec.zero_pad && spec.width > 0) {
        int num_width = spec.width - static_cast<int>(prefix.size());
        if (num_width > static_cast<int>(num_str.size())) {
            fl::string padded;
            for (int i = 0; i < num_width - static_cast<int>(num_str.size()); ++i) {
                padded.append('0');
            }
            padded.append(num_str);
            return fl::string(prefix) += padded;
        }
    }

    return fl::string(prefix) += num_str;
}

// Format a floating point value
inline fl::string format_float(double value, const FormatSpec& spec) {
    int precision = spec.precision >= 0 ? spec.precision : 6;

    fl::string result;

    // Handle sign
    if (value < 0) {
        result.append('-');
        value = -value;
    } else if (spec.sign == '+') {
        result.append('+');
    } else if (spec.sign == ' ') {
        result.append(' ');
    }

    // Format the number
    char buf[64];
    fl::ftoa(static_cast<float>(value), buf, precision);
    result.append(buf);

    return result;
}

// Format a pointer
inline fl::string format_pointer(const void* ptr, const FormatSpec& spec) {
    fl::uptr addr = fl::ptr_to_int(const_cast<void*>(ptr));

    FormatSpec hex_spec = spec;
    hex_spec.type = 'x';
    hex_spec.alternate = true;

    return format_integer(addr, hex_spec);
}

// Format a string value
inline fl::string format_string(const char* value, const FormatSpec& spec) {
    if (!value) value = "(null)";

    fl::string str(value);

    // Apply precision as max length for strings
    if (spec.precision >= 0 && static_cast<fl::size>(spec.precision) < str.size()) {
        str = fl::string(value, static_cast<fl::size>(spec.precision));
    }

    return str;
}

// Type-erased argument holder
class FormatArg {
public:
    enum class Type { None, Int, UInt, Long, ULong, LongLong, ULongLong,
                      Double, Char, CString, String, Pointer };

    FormatArg() : mType(Type::None) {}
    FormatArg(int v) : mType(Type::Int) { mData.i = v; }
    FormatArg(unsigned int v) : mType(Type::UInt) { mData.u = v; }
    FormatArg(long v) : mType(Type::Long) { mData.l = v; }
    FormatArg(unsigned long v) : mType(Type::ULong) { mData.ul = v; }
    FormatArg(long long v) : mType(Type::LongLong) { mData.ll = v; }
    FormatArg(unsigned long long v) : mType(Type::ULongLong) { mData.ull = v; }
    FormatArg(double v) : mType(Type::Double) { mData.d = v; }
    FormatArg(float v) : mType(Type::Double) { mData.d = v; }
    FormatArg(char v) : mType(Type::Char) { mData.c = v; }
    FormatArg(const char* v) : mType(Type::CString) { mData.s = v; }
    FormatArg(const fl::string& v) : mType(Type::String) { mData.str = &v; }
    FormatArg(const void* v) : mType(Type::Pointer) { mData.p = v; }

    // Short/byte types promote to int
    FormatArg(short v) : mType(Type::Int) { mData.i = v; }
    FormatArg(unsigned short v) : mType(Type::UInt) { mData.u = v; }
    FormatArg(signed char v) : mType(Type::Int) { mData.i = v; }
    FormatArg(unsigned char v) : mType(Type::UInt) { mData.u = v; }

    fl::string format(const FormatSpec& spec) const {
        switch (mType) {
            case Type::Int:       return format_integer(mData.i, spec);
            case Type::UInt:      return format_integer(mData.u, spec);
            case Type::Long:      return format_integer(mData.l, spec);
            case Type::ULong:     return format_integer(mData.ul, spec);
            case Type::LongLong:  return format_integer(mData.ll, spec);
            case Type::ULongLong: return format_integer(mData.ull, spec);
            case Type::Double:    return format_float(mData.d, spec);
            case Type::Char: {
                if (spec.type == 'd' || spec.type == 'x' || spec.type == 'X' ||
                    spec.type == 'b' || spec.type == 'o') {
                    return format_integer(static_cast<int>(mData.c), spec);
                }
                char buf[2] = {mData.c, '\0'};
                return fl::string(buf);
            }
            case Type::CString:   return format_string(mData.s, spec);
            case Type::String:    return format_string(mData.str->c_str(), spec);
            case Type::Pointer:   return format_pointer(mData.p, spec);
            case Type::None:
            default:              return fl::string("<invalid>");
        }
    }

    bool valid() const { return mType != Type::None; }

private:
    Type mType;
    union {
        int i;
        unsigned int u;
        long l;
        unsigned long ul;
        long long ll;
        unsigned long long ull;
        double d;
        char c;
        const char* s;
        const fl::string* str;
        const void* p;
    } mData;
};

// Core formatting implementation
inline fl::string format_impl(const char* fmt, const FormatArg* args, int num_args) {
    fl::string result;
    const char* p = fmt;
    int auto_index = 0;

    while (*p) {
        if (*p == '{') {
            if (*(p + 1) == '{') {
                // Escaped {{
                result.append('{');
                p += 2;
                continue;
            }

            ++p; // Skip '{'

            // Parse argument index
            int arg_index = -1;
            if (*p >= '0' && *p <= '9') {
                arg_index = 0;
                while (*p >= '0' && *p <= '9') {
                    arg_index = arg_index * 10 + (*p - '0');
                    ++p;
                }
            } else {
                arg_index = auto_index++;
            }

            // Parse format spec
            FormatSpec spec;
            if (*p == ':') {
                ++p;
                p = parse_format_spec(p, spec);
            }

            // Skip to '}'
            while (*p && *p != '}') ++p;
            if (*p == '}') ++p;

            // Format the argument
            if (arg_index >= 0 && arg_index < num_args) {
                fl::string formatted = args[arg_index].format(spec);
                apply_width_align(result, formatted, spec);
            } else {
                result.append("<out_of_range>");
            }
        } else if (*p == '}') {
            if (*(p + 1) == '}') {
                // Escaped }}
                result.append('}');
                p += 2;
            } else {
                result.append(*p++);
            }
        } else {
            // Regular character - find next special char
            const char* next = p;
            while (*next && *next != '{' && *next != '}') {
                ++next;
            }
            result.append(p, static_cast<fl::size>(next - p));
            p = next;
        }
    }

    return result;
}

} // namespace format_detail

/// Format with no arguments
inline fl::string format(const char* fmt) {
    return format_detail::format_impl(fmt, nullptr, 0);
}

/// Format with variadic arguments
template <typename... Args>
fl::string format(const char* fmt, const Args&... args) {
    format_detail::FormatArg arg_array[] = { format_detail::FormatArg(args)... };
    return format_detail::format_impl(fmt, arg_array, static_cast<int>(sizeof...(Args)));
}

} // namespace fl
