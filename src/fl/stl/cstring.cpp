///////////////////////////////////////////////////////////////////////////////
// FastLED C String Functions (cstring.cpp)
//
// Implementations of fl:: string and memory functions wrapping C library calls.
// This separation from the header reduces compilation overhead while maintaining
// a clean API in the fl:: namespace.
///////////////////////////////////////////////////////////////////////////////

#include "cstring.h"
#include "fl/stl/bit_cast.h"
// Include C string headers for standard C library functions
#include <string.h>  // ok include
#include <stdlib.h>  // ok include

// Include Arduino PROGMEM headers if on AVR
#if defined(ARDUINO) && defined(__AVR__)
#include <avr/pgmspace.h>
#endif

// C library string functions are available through the global namespace

namespace fl {

// ============================================================================
// Standard String Functions
// ============================================================================

size_t strlen(const char* s) noexcept {
    return ::strlen(s);
}

int strcmp(const char* s1, const char* s2) noexcept {
    return ::strcmp(s1, s2);
}

int strncmp(const char* s1, const char* s2, size_t n) noexcept {
    return ::strncmp(s1, s2, n);
}

char* strcpy(char* dest, const char* src) noexcept {
    return ::strcpy(dest, src);
}

char* strncpy(char* dest, const char* src, size_t n) noexcept {
    return ::strncpy(dest, src, n);
}

char* strcat(char* dest, const char* src) noexcept {
    return ::strcat(dest, src);
}

char* strncat(char* dest, const char* src, size_t n) noexcept {
    return ::strncat(dest, src, n);
}

const char* strstr(const char* haystack, const char* needle) noexcept {
    return ::strstr(haystack, needle);
}

char* strchr(char* s, int c) noexcept {
    return ::strchr(s, c);
}

const char* strchr(const char* s, int c) noexcept {
    return ::strchr(s, c);
}

char* strrchr(char* s, int c) noexcept {
    return ::strrchr(s, c);
}

const char* strrchr(const char* s, int c) noexcept {
    return ::strrchr(s, c);
}

size_t strspn(const char* s1, const char* s2) noexcept {
    return ::strspn(s1, s2);
}

size_t strcspn(const char* s1, const char* s2) noexcept {
    return ::strcspn(s1, s2);
}

char* strpbrk(char* s1, const char* s2) noexcept {
    return ::strpbrk(s1, s2);
}

const char* strpbrk(const char* s1, const char* s2) noexcept {
    return ::strpbrk(s1, s2);
}

char* strtok(char* s1, const char* s2) noexcept {
    return ::strtok(s1, s2);
}

char* strerror(int errnum) noexcept {
    return ::strerror(errnum);
}

// ============================================================================
// Memory Functions
// ============================================================================

void* memcpy(void* dest, const void* src, size_t n) noexcept {
    return ::memcpy(dest, src, n);
}

int memcmp(const void* s1, const void* s2, size_t n) noexcept {
    return ::memcmp(s1, s2, n);
}

void* memmove(void* dest, const void* src, size_t n) noexcept {
    return ::memmove(dest, src, n);
}

void* memset(void* s, int c, size_t n) noexcept {
    return ::memset(s, c, n);
}

void* memchr(void* s, int c, size_t n) noexcept {
    return ::memchr(s, c, n);
}

const void* memchr(const void* s, int c, size_t n) noexcept {
    return ::memchr(s, c, n);
}

// ============================================================================
// Arduino PROGMEM String Functions (platform-specific _P variants)
// ============================================================================
// Only available on platforms that support PROGMEM (AVR, some ARM boards)

#if defined(ARDUINO) && defined(__AVR__)

size_t strlen_P(detail::pgm_p s) noexcept {
    return ::strlen_P(fl::bit_cast<const char*>(s));
}

int strcmp_P(const char* a, detail::pgm_p b) noexcept {
    return ::strcmp_P(a, fl::bit_cast<const char*>(b));
}

int memcmp_P(const void* a, detail::pgm_p b, size_t n) noexcept {
    return ::memcmp_P(a, b, n);
}

void* memcpy_P(void* dest, detail::pgm_p src, size_t n) noexcept {
    return ::memcpy_P(dest, src, n);
}

#endif  // defined(ARDUINO) && defined(__AVR__)

}  // namespace fl
