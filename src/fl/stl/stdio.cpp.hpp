#include "fl/stl/stdio.h"

namespace fl { namespace printf_detail {

bool float_is_finite(float value) FL_NO_EXCEPT {
    if (value != value) {
        return false;
    }
    return value > -3.5e38f && value < 3.5e38f;
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
