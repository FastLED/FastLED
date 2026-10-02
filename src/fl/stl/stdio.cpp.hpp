#include "fl/stl/stdio.h"
#include "fl/stl/charconv.h"  // For fl::detail::hex, HexIntWidth

namespace fl { namespace printf_detail {

bool float_is_finite(float value) FL_NO_EXCEPT {
    if (value != value) {
        return false;
    }
    // FLT_MAX, spelled out. The old bound, 3.5e38f, overflows float to
    // infinity, so the check only worked because of that overflow.
    return value >= -3.40282347e38f && value <= 3.40282347e38f;
}

fl::string format_float_scientific(float value, int precision) FL_NO_EXCEPT {
    const bool negative = value < 0.0f;
    float magnitude = negative ? -value : value;
    int exponent = 0;
    while (magnitude >= 10.0f) {
        magnitude /= 10.0f;
        ++exponent;
    }
    while (magnitude > 0.0f && magnitude < 1.0f) {
        magnitude *= 10.0f;
        --exponent;
    }

    // Normalising put the mantissa in [1, 10), but *rounding* it to the
    // requested precision can carry it back out: 9.999e18 at precision 2
    // rounds to 10.00, and "10.00e+18" is not scientific notation. Checked
    // before formatting rather than patched after, so there is one place
    // where the exponent is decided.
    float half_step = 0.5f;
    for (int digit = 0; digit < precision; ++digit) {
        half_step /= 10.0f;
    }
    if (magnitude >= 10.0f - half_step) {
        magnitude /= 10.0f;
        ++exponent;
    }

    sstream stream;
    if (negative) {
        stream << "-";
    }
    // The mantissa is in [1, 10) now, so the ordinary path renders it.
    stream << format_float(magnitude, precision);
    stream << "e";
    if (exponent < 0) {
        stream << "-";
        exponent = -exponent;
    } else {
        stream << "+";
    }
    if (exponent < 10) {
        stream << "0";
    }
    stream << exponent;
    return stream.str();
}

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

// Same for float/double, which go through their own sstream overloads.
void stream_floating(sstream& temp, const ScalarArg& a) FL_NO_EXCEPT {
    if (a.kind == ScalarArg::kFloat) {
        temp << a.f;
    } else {
        temp << a.d;
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

// Float/double kinds. Only floating format_arg instantiations reference this.
void format_floating(sstream& stream, const FormatSpec& spec, const ScalarArg& a) FL_NO_EXCEPT {
    fl::string result;
    bool is_numeric = false;

    switch (spec.type) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'c':
            result = "<type_error>";
            break;

        case 'x': {
            is_numeric = true;
            const bool is_float = a.kind == ScalarArg::kFloat;
            result = is_float ? fl::to_hex(a.f, spec.uppercase)
                              : fl::to_hex(a.d, spec.uppercase);
            const bool nonzero = is_float ? a.f != 0.0f : a.d != 0.0;
            if (spec.alt_form && nonzero) {
                result = fl::string(spec.uppercase ? "0X" : "0x") + result;
            }
            break;
        }

        case 'f': {
            is_numeric = true;
            const float value =
                a.kind == ScalarArg::kFloat ? a.f : static_cast<float>(a.d);
            if (spec.precision >= 0) {
                result = format_float(value, spec.precision);
            } else {
                sstream temp;
                temp << value;
                result = temp.str();
            }
            break;
        }

        case 's': {
            sstream temp;
            stream_floating(temp, a);
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
