///////////////////////////////////////////////////////////////////////////////
// FastLED C String Functions (cstring.cpp)
//
// Implementations of fl:: string and memory functions wrapping C library calls.
// This separation from the header reduces compilation overhead while maintaining
// a clean API in the fl:: namespace.
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/cstring.h"
#include "fl/stl/bit_cast.h"
#include "platforms/is_platform.h"
// Include C string headers for standard C library functions
// IWYU pragma: begin_keep
#include <string.h>  // okay banned header (STL wrapper implementation requires standard header)
#include <stdlib.h>  // okay banned header (STL wrapper implementation requires standard header)

// IWYU pragma: end_keep
// Include Arduino PROGMEM headers if on AVR
#if defined(ARDUINO) && defined(FL_IS_AVR)
// IWYU pragma: begin_keep
#include <avr/pgmspace.h>
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep
#endif

// C library string functions are available through the global namespace

namespace fl {

// ============================================================================
// Standard String Functions
// ============================================================================

size_t strlen(const char* s) FL_NO_EXCEPT {
    return ::strlen(s);
}

int strcmp(const char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strcmp(s1, s2);
}

int strncmp(const char* s1, const char* s2, size_t n) FL_NO_EXCEPT {
    return ::strncmp(s1, s2, n);
}

char* strcpy(char* dest, const char* src) FL_NO_EXCEPT {
    return ::strcpy(dest, src);
}

char* strncpy(char* dest, const char* src, size_t n) FL_NO_EXCEPT {
    // Unlike ::strncpy, this ALWAYS null-terminates: it copies at most n
    // characters from src, then writes a terminator at dest[copied]
    // (copied <= n). Callers pass n = capacity - 1 (dest sized >= n+1),
    // so the terminator is always in bounds. This closes the classic
    // strncpy hazard where a source of length >= n leaves dest
    // unterminated and downstream string walks run off the buffer
    // (FastLED#3588: an unterminated HTTP request URI let a query-strip
    // loop write a stray null byte one past a heap allocation).
    size_t i = 0;
    for (; i < n && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    // Preserve ::strncpy's zero-padding of the remaining destination
    // bytes so callers relying on that behavior are unaffected.
    for (size_t j = i + 1; j < n; ++j) {
        dest[j] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) FL_NO_EXCEPT {
    return ::strcat(dest, src);
}

char* strncat(char* dest, const char* src, size_t n) FL_NO_EXCEPT {
    return ::strncat(dest, src, n);
}

const char* strstr(const char* haystack, const char* needle) FL_NO_EXCEPT {
    return ::strstr(haystack, needle);
}

char* strchr(char* s, int c) FL_NO_EXCEPT {
    return ::strchr(s, c);
}

const char* strchr(const char* s, int c) FL_NO_EXCEPT {
    return ::strchr(s, c);
}

char* strrchr(char* s, int c) FL_NO_EXCEPT {
    return ::strrchr(s, c);
}

const char* strrchr(const char* s, int c) FL_NO_EXCEPT {
    return ::strrchr(s, c);
}

size_t strspn(const char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strspn(s1, s2);
}

size_t strcspn(const char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strcspn(s1, s2);
}

char* strpbrk(char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strpbrk(s1, s2);
}

const char* strpbrk(const char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strpbrk(s1, s2);
}

char* strtok(char* s1, const char* s2) FL_NO_EXCEPT {
    return ::strtok(s1, s2);
}

char* strerror(int errnum) FL_NO_EXCEPT {
    return ::strerror(errnum);
}

// ============================================================================
// Memory Functions
// ============================================================================

void* memcpy(void* dest, const void* src, size_t n) FL_NO_EXCEPT {
    if (n == 0) return dest;
    return ::memcpy(dest, src, n);
}

int memcmp(const void* s1, const void* s2, size_t n) FL_NO_EXCEPT {
    if (n == 0) return 0;
    return ::memcmp(s1, s2, n);
}

void* memmove(void* dest, const void* src, size_t n) FL_NO_EXCEPT {
    if (n == 0) return dest;
    return ::memmove(dest, src, n);
}

void* memset(void* s, int c, size_t n) FL_NO_EXCEPT {
    if (n == 0) return s;
    return ::memset(s, c, n);
}

void* memchr(void* s, int c, size_t n) FL_NO_EXCEPT {
    if (n == 0) return nullptr;
    return ::memchr(s, c, n);
}

const void* memchr(const void* s, int c, size_t n) FL_NO_EXCEPT {
    if (n == 0) return nullptr;
    return ::memchr(s, c, n);
}

// ============================================================================
// Arduino PROGMEM String Functions (platform-specific _P variants)
// ============================================================================
// Only available on platforms that support PROGMEM (AVR, some ARM boards)

#if defined(ARDUINO) && defined(FL_IS_AVR)

size_t strlen_P(detail::pgm_p s) FL_NO_EXCEPT {
    return ::strlen_P(fl::bit_cast<const char*>(s));
}

int strcmp_P(const char* a, detail::pgm_p b) FL_NO_EXCEPT {
    return ::strcmp_P(a, fl::bit_cast<const char*>(b));
}

int memcmp_P(const void* a, detail::pgm_p b, size_t n) FL_NO_EXCEPT {
    return ::memcmp_P(a, b, n);
}

void* memcpy_P(void* dest, detail::pgm_p src, size_t n) FL_NO_EXCEPT {
    return ::memcpy_P(dest, src, n);
}

#endif  // defined(ARDUINO) && defined(FL_IS_AVR)

}  // namespace fl
