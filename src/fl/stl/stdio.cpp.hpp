#include "fl/stl/stdio.h"
#include "fl/stl/charconv.h"  // For fl::detail::hex, HexIntWidth

namespace fl { namespace printf_detail {

namespace {

// Streams an integral value exactly as `sstream << original_type` would:
// char as a character, bool as true/false, integers in decimal.
void stream_integral(sstream& temp, const ScalarArg& a) FL_NO_EXCEPT {
    switch (a.kind) {
        case ScalarArg::kUnsigned: temp << a.u; break;
        case ScalarArg::kBool: temp << (a.u != 0); break;
        case ScalarArg::kChar: temp << static_cast<char>(a.u); break;
        default: temp << a.s; break;
    }
}

void emit_padded(sstream& stream, const FormatSpec& spec, const fl::string& result,
                 bool is_numeric) FL_NO_EXCEPT {
    stream << apply_width(result, spec, is_numeric);
}

} // namespace

// Integral kinds. Must not reference float/double code: integer-only
// programs link only this half (FastLED#4671).
void format_integral(sstream& stream, const FormatSpec& spec, const ScalarArg& a) FL_NO_EXCEPT {
    fl::string result;
    bool is_numeric = false;

    switch (spec.type) {
        case 'd':
        case 'i': {
            is_numeric = true;
            sstream temp;
            stream_integral(temp, a);
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
            is_numeric = true;
            sstream temp;
            stream_integral(temp, a);
            result = temp.str();
            break;
        }

        case 'o': {
            is_numeric = true;
            result = to_octal(a.u);
            // Alternate form: prefix with 0 (but not for zero itself)
            if (spec.alt_form && a.u != 0) {
                result = fl::string("0") + result;
            }
            break;
        }

        case 'x': {
            is_numeric = true;
            // Mirrors fl::to_hex<T> for the original type.
            const bool negative = a.s < 0;
            const fl::u64 magnitude =
                negative ? static_cast<fl::u64>(0) - a.u : a.u;  // no INT64_MIN UB
            result = fl::detail::hex(
                magnitude,
                static_cast<fl::detail::HexIntWidth>(a.size * 8),
                negative, spec.uppercase, false);
            if (spec.alt_form && a.u != 0) {
                result = fl::string(spec.uppercase ? "0X" : "0x") + result;
            }
            break;
        }

        case 'f':
            result = "<type_error>";
            break;

        case 'c': {
            char temp_str[2] = {static_cast<char>(a.u), '\0'};
            result = temp_str;
            break;
        }

        case 's': {
            sstream temp;
            stream_integral(temp, a);
            result = temp.str();
            break;
        }

        default:
            result = "<unknown_format>";
            break;
    }
    emit_padded(stream, spec, result, is_numeric);
}

void format_arg(sstream& stream, const FormatSpec& spec, const fl::string& arg) FL_NO_EXCEPT {
    format_arg(stream, spec, arg.c_str());
}

void format_arg(sstream& stream, const FormatSpec& spec, const fl::string_view& arg) FL_NO_EXCEPT {
    format_arg(stream, spec, fl::string(arg).c_str());
}

} } // namespace fl::printf_detail

namespace fl {

void printf(const char* format) FL_NO_EXCEPT {
    char output[64];
    fl::size used = 0;
    const auto flush_buffer = [&]() {
        if (used == 0) {
            return;
        }
        output[used] = '\0';
        fl::print(output);
        // `append` resumes writing only after this shared cursor is reset.
        used = 0;
    };
    const auto append = [&](const char* text) {
        while (*text) {
            if (used == sizeof(output) - 1) {
                flush_buffer();
            }
            output[used++] = *text++;
        }
    };

    while (*format) {
        if (*format == '%') {
            printf_detail::FormatSpec spec =
                printf_detail::parse_format_spec(format);
            append(spec.type == '%' ? "%" : "<missing_arg>");
            continue;
        }
        if (format[0] == '{' && format[1] == '{') {
            append("{");
            format += 2;
            continue;
        }
        if (format[0] == '}' && format[1] == '}') {
            append("}");
            format += 2;
            continue;
        }
        if (format[0] == '{' && format[1] == '}') {
            append("<missing_arg>");
            format += 2;
            continue;
        }
        if (used == sizeof(output) - 1) {
            flush_buffer();
        }
        output[used++] = *format++;
    }
    flush_buffer();
}

int snprintf(char* buffer, fl::size size, const char* format) FL_NO_EXCEPT {
    if (!buffer || size == 0) {
        return 0;
    }

    fl::size written = 0;
    const auto append_char = [&](char value) {
        if (written < size - 1) {
            buffer[written++] = value;
        }
    };
    const auto append_text = [&](const char* text) {
        while (*text && written < size - 1) {
            buffer[written++] = *text++;
        }
    };

    while (*format && written < size - 1) {
        if (*format == '%') {
            printf_detail::FormatSpec spec =
                printf_detail::parse_format_spec(format);
            append_text(spec.type == '%' ? "%" : "<missing_arg>");
            continue;
        }
        if (format[0] == '{' && format[1] == '{') {
            append_char('{');
            format += 2;
            continue;
        }
        if (format[0] == '}' && format[1] == '}') {
            append_char('}');
            format += 2;
            continue;
        }
        if (format[0] == '{' && format[1] == '}') {
            append_text("<missing_arg>");
            format += 2;
            continue;
        }
        append_char(*format++);
    }
    buffer[written] = '\0';
    return static_cast<int>(written);
}

} // namespace fl
