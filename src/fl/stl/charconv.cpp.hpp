///////////////////////////////////////////////////////////////////////////////
// FastLED Character Conversion Functions Implementation
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/charconv.h"
#include "fl/stl/string.h"
#include "fl/stl/stdio.h"
#include "fl/stl/limits.h"

namespace fl {


// Public API implementations for integer to string conversion
// Moved from string_functions namespace in str.cpp for better organization

int itoa(i32 value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    u32 v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -static_cast<u32>(value);
    else
        v = static_cast<u32>(value);

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

int itoa64(i64 value, char *sp, int radix) {
    char tmp[32]; // Buffer for 64-bit integer (max 65 chars for base 2 + sign)
    char *tp = tmp;
    int i;
    u64 v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -static_cast<u64>(value);
    else
        v = static_cast<u64>(value);

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

int utoa32(u32 value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    u32 v = value;

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

int utoa64(u64 value, char *sp, int radix) {
    char tmp[32]; // larger buffer for 64-bit values
    char *tp = tmp;
    int i;
    u64 v = value;

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
    unsigned int result = 0;
    int sign = 1;
    fl::size pos = 0;

    // Handle empty input
    if (len == 0) {
        return 0;
    }

    // Skip leading whitespace
    while (pos < len &&
           (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' ||
            str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v')) {
        pos++;
    }

    // Handle optional sign
    if (pos < len && str[pos] == '-') {
        sign = -1;
        pos++;
    } else if (pos < len && str[pos] == '+') {
        pos++;
    }

    const unsigned int positiveLimit =
        static_cast<unsigned int>(fl::numeric_limits<int>::max());
    const unsigned int limit = positiveLimit + (sign < 0 ? 1u : 0u);

    // Accumulate a magnitude, saturating inputs outside the int range.
    while (pos < len && str[pos] >= '0' && str[pos] <= '9') {
        const unsigned int digit = static_cast<unsigned int>(str[pos] - '0');
        if (result > (limit - digit) / 10u) {
            return sign < 0 ? fl::numeric_limits<int>::min()
                            : fl::numeric_limits<int>::max();
        }
        result = result * 10u + digit;
        pos++;
    }

    if (sign < 0 && result == positiveLimit + 1u) {
        return fl::numeric_limits<int>::min();
    }
    const int magnitude = static_cast<int>(result);
    return sign < 0 ? -magnitude : magnitude;
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
