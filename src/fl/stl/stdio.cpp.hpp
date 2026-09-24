#include "fl/stl/stdio.h"

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
