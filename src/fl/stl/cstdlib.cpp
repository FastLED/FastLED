#include "fl/stl/cstdlib.h"
#include "fl/stl/cstring.h"

namespace fl {

// Helper function to check if a character is a digit in the given base
static bool isDigitInBase(char c, int base) {
    if (base <= 10) {
        return c >= '0' && c < ('0' + base);
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c >= 'a' && c < ('a' + base - 10)) {
        return true;
    }
    if (c >= 'A' && c < ('A' + base - 10)) {
        return true;
    }
    return false;
}

// Helper function to convert character to digit value
static int charToDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return 0;
}

long strtol(const char* str, char** endptr, int base) {
    if (!str) {
        if (endptr) {
            *endptr = const_cast<char*>(str);
        }
        return 0;
    }

    const char* p = str;
    long result = 0;
    bool negative = false;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        p++;
    }

    // Handle sign
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    // Auto-detect base if base is 0
    if (base == 0) {
        if (*p == '0') {
            if (*(p + 1) == 'x' || *(p + 1) == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        // Skip optional 0x/0X prefix for base 16
        if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
            p += 2;
        }
    }

    // Validate base
    if (base < 2 || base > 36) {
        if (endptr) {
            *endptr = const_cast<char*>(str);
        }
        return 0;
    }

    // Parse digits
    const char* start = p;
    while (*p && isDigitInBase(*p, base)) {
        result = result * base + charToDigit(*p);
        p++;
    }

    // Set endptr to point to first invalid character
    if (endptr) {
        if (p == start) {
            // No digits parsed
            *endptr = const_cast<char*>(str);
        } else {
            *endptr = const_cast<char*>(p);
        }
    }

    return negative ? -result : result;
}

unsigned long strtoul(const char* str, char** endptr, int base) {
    if (!str) {
        if (endptr) {
            *endptr = const_cast<char*>(str);
        }
        return 0;
    }

    const char* p = str;
    unsigned long result = 0;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        p++;
    }

    // Handle optional + sign (ignore - for unsigned)
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        // Negative values wrap around for unsigned
        p++;
    }

    // Auto-detect base if base is 0
    if (base == 0) {
        if (*p == '0') {
            if (*(p + 1) == 'x' || *(p + 1) == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        // Skip optional 0x/0X prefix for base 16
        if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
            p += 2;
        }
    }

    // Validate base
    if (base < 2 || base > 36) {
        if (endptr) {
            *endptr = const_cast<char*>(str);
        }
        return 0;
    }

    // Parse digits
    const char* start = p;
    while (*p && isDigitInBase(*p, base)) {
        result = result * base + charToDigit(*p);
        p++;
    }

    // Set endptr to point to first invalid character
    if (endptr) {
        if (p == start) {
            // No digits parsed
            *endptr = const_cast<char*>(str);
        } else {
            *endptr = const_cast<char*>(p);
        }
    }

    return result;
}

int atoi(const char* str) {
    return static_cast<int>(strtol(str, nullptr, 10));
}

long atol(const char* str) {
    return strtol(str, nullptr, 10);
}

double strtod(const char* str, char** endptr) {
    if (!str) {
        if (endptr) {
            *endptr = const_cast<char*>(str);
        }
        return 0.0;
    }

    const char* p = str;
    double result = 0.0;
    bool negative = false;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        p++;
    }

    // Handle sign
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    const char* start = p;

    // Parse integer part
    while (*p >= '0' && *p <= '9') {
        result = result * 10.0 + (*p - '0');
        p++;
    }

    // Parse fractional part
    if (*p == '.') {
        p++;
        double fraction = 0.1;
        while (*p >= '0' && *p <= '9') {
            result += (*p - '0') * fraction;
            fraction *= 0.1;
            p++;
        }
    }

    // Parse exponent
    if (*p == 'e' || *p == 'E') {
        p++;
        bool expNegative = false;
        if (*p == '-') {
            expNegative = true;
            p++;
        } else if (*p == '+') {
            p++;
        }

        int exp = 0;
        while (*p >= '0' && *p <= '9') {
            exp = exp * 10 + (*p - '0');
            p++;
        }

        double multiplier = 1.0;
        for (int i = 0; i < exp; i++) {
            multiplier *= 10.0;
        }

        if (expNegative) {
            result /= multiplier;
        } else {
            result *= multiplier;
        }
    }

    // Set endptr
    if (endptr) {
        if (p == start) {
            *endptr = const_cast<char*>(str);
        } else {
            *endptr = const_cast<char*>(p);
        }
    }

    return negative ? -result : result;
}

} // namespace fl
