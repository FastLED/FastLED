///////////////////////////////////////////////////////////////////////////////
// FastLED C String Functions (cstring.cpp)
//
// Implementations of fl:: string and memory functions wrapping C library calls.
// This separation from the header reduces compilation overhead while maintaining
// a clean API in the fl:: namespace.
///////////////////////////////////////////////////////////////////////////////

#include "cstring.h"
// C library string functions are available through the global namespace
// without explicitly including <cstring> (they're typically in <cstdlib> or available globally)

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

size_t strlen_P(detail::pgm_p s) noexcept {
    return ::strlen_P(reinterpret_cast<const char*>(s));
}

int strcmp_P(const char* a, detail::pgm_p b) noexcept {
    return ::strcmp_P(a, reinterpret_cast<const char*>(b));
}

int memcmp_P(const void* a, detail::pgm_p b, size_t n) noexcept {
    return ::memcmp_P(a, b, n);
}

void* memcpy_P(void* dest, detail::pgm_p src, size_t n) noexcept {
    return ::memcpy_P(dest, src, n);
}

}  // namespace fl
