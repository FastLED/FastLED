///////////////////////////////////////////////////////////////////////////////
// FastLED Character Conversion Functions Implementation
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/charconv.h"
#include "fl/str.h"
#include "fl/stl/stdio.h"

namespace fl {
namespace detail {

fl::string hex(uint64_t value, HexIntWidth width, bool is_negative, bool uppercase, bool pad_to_width) {
    // Determine target width in hex characters based on integer bit width
    size_t target_width = 0;
    switch (width) {
        case HexIntWidth::Width8:  target_width = 2; break;   // 8 bits = 2 hex chars
        case HexIntWidth::Width16: target_width = 4; break;   // 16 bits = 4 hex chars
        case HexIntWidth::Width32: target_width = 8; break;   // 32 bits = 8 hex chars
        case HexIntWidth::Width64: target_width = 16; break;  // 64 bits = 16 hex chars
    }

    fl::string result;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    // Convert value to hex string (minimal representation)
    uint64_t temp_value = value;
    if (temp_value == 0) {
        // Special case: zero should be "0" not empty string
        result = fl::string("0");
    } else {
        while (temp_value > 0) {
            char ch = digits[temp_value % 16];
            // Convert char to string since fl::string::append treats char as number
            char temp_ch_str[2] = {ch, '\0'};
            fl::string digit_str(temp_ch_str);
            // Use += since + operator is not defined for fl::string
            fl::string temp = digit_str;
            temp += result;
            result = temp;
            temp_value /= 16;
        }
    }

    // Pad with leading zeros to target width (only if padding is requested)
    if (pad_to_width) {
        while (result.size() < target_width) {
            fl::string zero_str("0");
            zero_str += result;
            result = zero_str;
        }
    }

    // Add negative sign if needed
    if (is_negative) {
        fl::string minus_str("-");
        minus_str += result;
        result = minus_str;
    }

    return result;
}

} // namespace detail

// Public API implementations for integer to string conversion
// Moved from string_functions namespace in str.cpp for better organization

int itoa(int32_t value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    unsigned v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (unsigned)value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp)
        *sp++ = *--tp;

    *sp = '\0';  // Null-terminate the string
    return len;
}

int itoa64(int64_t value, char *sp, int radix) {
    char tmp[32]; // Buffer for 64-bit integer (max 65 chars for base 2 + sign)
    char *tp = tmp;
    int i;
    uint64_t v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (uint64_t)value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp)
        *sp++ = *--tp;

    *sp = '\0';  // Null-terminate the string
    return len;
}

int utoa32(uint32_t value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    uint32_t v = value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    while (tp > tmp)
        *sp++ = *--tp;

    *sp = '\0';  // Null-terminate the string
    return len;
}

int utoa64(uint64_t value, char *sp, int radix) {
    char tmp[32]; // larger buffer for 64-bit values
    char *tp = tmp;
    int i;
    uint64_t v = value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    while (tp > tmp)
        *sp++ = *--tp;

    *sp = '\0';  // Null-terminate the string
    return len;
}

void ftoa(float value, char *buffer, int precision) {
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

// Parse functions - moved from StringFormatter
float parseFloat(const char *str, fl::size len) {
    float result = 0.0f;   // The resulting number
    float sign = 1.0f;     // Positive or negative
    float fraction = 0.0f; // Fractional part
    float divisor = 1.0f;  // Divisor for the fractional part
    int isFractional = 0;  // Whether the current part is fractional

    fl::size pos = 0; // Current position in the string

    // Handle empty input
    if (len == 0) {
        return 0.0f;
    }

    // Skip leading whitespace (manual check instead of isspace)
    while (pos < len &&
           (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' ||
            str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v')) {
        pos++;
    }

    // Handle optional sign
    if (pos < len && str[pos] == '-') {
        sign = -1.0f;
        pos++;
    } else if (pos < len && str[pos] == '+') {
        pos++;
    }

    // Main parsing loop
    while (pos < len) {
        if (str[pos] >= '0' && str[pos] <= '9') {
            if (isFractional) {
                divisor *= 10.0f;
                fraction += (str[pos] - '0') / divisor;
            } else {
                result = result * 10.0f + (str[pos] - '0');
            }
        } else if (str[pos] == '.' && !isFractional) {
            isFractional = 1;
        } else {
            // Stop parsing at invalid characters
            break;
        }
        pos++;
    }

    // Combine integer and fractional parts
    result = result + fraction;

    // Apply the sign
    return sign * result;
}

int parseInt(const char *str, fl::size len) {
    float f = parseFloat(str, len);
    return static_cast<int>(f);
}

int parseInt(const char *str) {
    // Calculate length manually to avoid strlen dependency
    fl::size len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return parseInt(str, len);
}

} // namespace fl
