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

// Forward declarations for qsort helpers
namespace detail {
    void qsort_swap(char* a, char* b, size_t size);
    void qsort_impl(char* base, size_t nmemb, size_t size, qsort_compare_fn compar);
}

// Swap two elements of given size
void detail::qsort_swap(char* a, char* b, size_t size) {
    if (a == b) return;
    constexpr size_t STACK_BUF_SIZE = 64;
    if (size <= STACK_BUF_SIZE) {
        char tmp[STACK_BUF_SIZE];
        memcpy(tmp, a, size);
        memcpy(a, b, size);
        memcpy(b, tmp, size);
    } else {
        for (size_t i = 0; i < size; ++i) {
            char t = a[i];
            a[i] = b[i];
            b[i] = t;
        }
    }
}

// Insertion sort for small arrays (stable and efficient for small n)
static void qsort_insertion_sort(char* base, size_t nmemb, size_t size, qsort_compare_fn compar) {
    for (size_t i = 1; i < nmemb; ++i) {
        // Save current element in temp buffer
        char temp[256]; // Stack buffer for elements up to 256 bytes
        char* elem_i = base + i * size;

        if (size <= sizeof(temp)) {
            memcpy(temp, elem_i, size);

            size_t j = i;
            while (j > 0 && compar(temp, base + (j - 1) * size) < 0) {
                memcpy(base + j * size, base + (j - 1) * size, size);
                --j;
            }

            memcpy(base + j * size, temp, size);
        } else {
            // For large elements, use swaps instead of shifts
            size_t j = i;
            while (j > 0 && compar(elem_i, base + (j - 1) * size) < 0) {
                detail::qsort_swap(base + j * size, base + (j - 1) * size, size);
                --j;
            }
        }
    }
}

// Partition function for quicksort
static size_t qsort_partition(char* base, size_t nmemb, size_t size, qsort_compare_fn compar) {
    // Use median-of-three for pivot selection
    size_t mid = nmemb / 2;
    size_t last = nmemb - 1;

    char* first_elem = base;
    char* mid_elem = base + mid * size;
    char* last_elem = base + last * size;

    // Sort first, middle, last to find median
    if (compar(mid_elem, first_elem) < 0) {
        detail::qsort_swap(first_elem, mid_elem, size);
    }
    if (compar(last_elem, first_elem) < 0) {
        detail::qsort_swap(first_elem, last_elem, size);
    }
    if (compar(last_elem, mid_elem) < 0) {
        detail::qsort_swap(mid_elem, last_elem, size);
    }

    // Now mid_elem contains the median, swap it to end-1 position
    detail::qsort_swap(mid_elem, base + (last - 1) * size, size);
    char* pivot = base + (last - 1) * size;

    // Partition
    size_t i = 0;
    for (size_t j = 0; j < last - 1; ++j) {
        if (compar(base + j * size, pivot) < 0) {
            if (i != j) {
                detail::qsort_swap(base + i * size, base + j * size, size);
            }
            ++i;
        }
    }

    // Move pivot to its final position
    detail::qsort_swap(base + i * size, pivot, size);
    return i;
}

// Recursive quicksort implementation
void detail::qsort_impl(char* base, size_t nmemb, size_t size, qsort_compare_fn compar) {
    if (nmemb <= 1) {
        return;
    }

    // Use insertion sort for small arrays (threshold of 16)
    if (nmemb <= 16) {
        qsort_insertion_sort(base, nmemb, size, compar);
        return;
    }

    size_t pivot_idx = qsort_partition(base, nmemb, size, compar);

    // Sort left partition
    qsort_impl(base, pivot_idx, size, compar);

    // Sort right partition
    if (pivot_idx + 1 < nmemb) {
        qsort_impl(base + (pivot_idx + 1) * size, nmemb - pivot_idx - 1, size, compar);
    }
}

// qsort - Direct implementation without using fl::sort to avoid proxy iterator issues
void qsort(void* base, size_t nmemb, size_t size, qsort_compare_fn compar) {
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }

    char* arr = static_cast<char*>(base);
    detail::qsort_impl(arr, nmemb, size, compar);
}

} // namespace fl
