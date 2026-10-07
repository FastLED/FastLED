#include "fl/stl/basic_string.h" // ok no header - public declarations remain in parent directory.
#include "fl/stl/charconv.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"

namespace fl {

basic_string& basic_string::append(const float& val) FL_NO_EXCEPT {
    char buf[64] = {0};
    fl::ftoa(val, buf, 2);
    write(buf, fl::strlen(buf));
    return *this;
}

basic_string& basic_string::append(const float& val, int precision) FL_NO_EXCEPT {
    char buf[64] = {0};
    fl::ftoa(val, buf, precision);
    write(buf, fl::strlen(buf));
    return *this;
}

basic_string& basic_string::append(const double& val) FL_NO_EXCEPT {
    return append(static_cast<float>(val));
}

void ftoa(float value, char *buffer, int precision) FL_NO_EXCEPT {
    // Forward to printf_detail for now - implementation in str.cpp will be updated
    // to use this function instead of duplicating the logic
    fl::string result = fl::printf_detail::format_float(value, precision);
    fl::size len = result.length();
    if (len > 63) len = 63; // Leave room for null terminator
    for (fl::size i = 0; i < len; ++i) {
        buffer[i] = result[i];
    }
    buffer[len] = '\0';
}

} // namespace fl

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

} } // namespace fl::printf_detail
