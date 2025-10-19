#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED C String Functions (cstring.h equivalents)
//
// Provides wrappers for standard C string functions (memcpy, strlen, strcmp, etc.)
// while avoiding the slow <cstring> include that impacts compilation time.
//
// All functions are in the fl:: namespace for type safety and clarity.
// Implementations are in cstring.cpp
///////////////////////////////////////////////////////////////////////////////

#include "fl/stdint.h"

namespace fl {

// ============================================================================
// Standard String Functions
// ============================================================================

// fl::strlen - wrapper for strlen
size_t strlen(const char* s) noexcept;

// fl::strcmp - wrapper for strcmp
int strcmp(const char* s1, const char* s2) noexcept;

// fl::strncmp - wrapper for strncmp
int strncmp(const char* s1, const char* s2, size_t n) noexcept;

// fl::strcpy - wrapper for strcpy (use with caution - prefer strncpy)
char* strcpy(char* dest, const char* src) noexcept;

// fl::strncpy - wrapper for strncpy
char* strncpy(char* dest, const char* src, size_t n) noexcept;

// fl::strcat - wrapper for strcat (use with caution - prefer strncat)
char* strcat(char* dest, const char* src) noexcept;

// fl::strncat - wrapper for strncat
char* strncat(char* dest, const char* src, size_t n) noexcept;

// fl::strstr - wrapper for strstr
const char* strstr(const char* haystack, const char* needle) noexcept;

// ============================================================================
// Memory Functions
// ============================================================================

// fl::memcpy - wrapper for memcpy
void* memcpy(void* dest, const void* src, size_t n) noexcept;

// fl::memcmp - wrapper for memcmp
int memcmp(const void* s1, const void* s2, size_t n) noexcept;

// fl::memmove - wrapper for memmove (handles overlapping memory)
void* memmove(void* dest, const void* src, size_t n) noexcept;

// fl::memset - wrapper for memset
void* memset(void* s, int c, size_t n) noexcept;

// fl::memchr - wrapper for memchr
void* memchr(void* s, int c, size_t n) noexcept;

// fl::memchr - const version wrapper for memchr
const void* memchr(const void* s, int c, size_t n) noexcept;

// ============================================================================
// Arduino PROGMEM String Functions (platform-specific _P variants)
// ============================================================================
// These are used when strings are stored in program memory on embedded platforms

// Forward declarations for pgm_p type (defined in json.hpp)
namespace detail {
    typedef const void* pgm_p;
}

// fl::strlen_P - strlen for PROGMEM strings
size_t strlen_P(detail::pgm_p s) noexcept;

// fl::strcmp_P - strcmp with PROGMEM string
int strcmp_P(const char* a, detail::pgm_p b) noexcept;

// fl::memcmp_P - memcmp with PROGMEM memory
int memcmp_P(const void* a, detail::pgm_p b, size_t n) noexcept;

// fl::memcpy_P - memcpy from PROGMEM
void* memcpy_P(void* dest, detail::pgm_p src, size_t n) noexcept;

}  // namespace fl
