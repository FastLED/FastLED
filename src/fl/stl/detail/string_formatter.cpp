#include <string.h>  // ok include - for strlen

#include "fl/stl/detail/string_formatter.h"

namespace fl {

namespace string_functions {

// Note: ftoa, itoa, utoa32, utoa64 have been moved to fl::charconv
// These functions are now deprecated and delegate to the new API

static float atoff(const char *str, size len) {
    float result = 0.0f;   // The resulting number
    float sign = 1.0f;     // Positive or negative
    float fraction = 0.0f; // Fractional part
    float divisor = 1.0f;  // Divisor for the fractional part
    int isFractional = 0;  // Whether the current part is fractional

    size pos = 0; // Current position in the string

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

} // namespace string_functions

// StringFormatter non-template implementations
// Template implementations are inline in string_formatter.h

float StringFormatter::parseFloat(const char *str, size len) {
    return string_functions::atoff(str, len);
}

int StringFormatter::parseInt(const char *str, size len) {
    float f = parseFloat(str, len);
    return static_cast<int>(f);
}

int StringFormatter::parseInt(const char *str) {
    return parseInt(str, strlen(str));
}

} // namespace fl
