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

#include "fl/stl/stdint.h"

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

// fl::strchr - wrapper for strchr (find first occurrence of character)
char* strchr(char* s, int c) noexcept;

// fl::strchr - const version wrapper for strchr
const char* strchr(const char* s, int c) noexcept;

// fl::strrchr - wrapper for strrchr (find last occurrence of character)
char* strrchr(char* s, int c) noexcept;

// fl::strrchr - const version wrapper for strrchr
const char* strrchr(const char* s, int c) noexcept;

// fl::strspn - wrapper for strspn (length of initial segment matching characters)
size_t strspn(const char* s1, const char* s2) noexcept;

// fl::strcspn - wrapper for strcspn (length of initial segment not matching characters)
size_t strcspn(const char* s1, const char* s2) noexcept;

// fl::strpbrk - wrapper for strpbrk (find first occurrence of any character from set)
char* strpbrk(char* s1, const char* s2) noexcept;

// fl::strpbrk - const version wrapper for strpbrk
const char* strpbrk(const char* s1, const char* s2) noexcept;

// fl::strtok - wrapper for strtok (tokenize string, note: not thread-safe)
char* strtok(char* s1, const char* s2) noexcept;

// fl::strerror - wrapper for strerror (convert error number to string)
char* strerror(int errnum) noexcept;

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
// Legacy compatibility functions (aliases)
// ============================================================================

// fl::memfill - legacy name for memset
inline void* memfill(void* s, int c, size_t n) noexcept {
    return memset(s, c, n);
}

// fl::memcopy - legacy name for memcpy
inline void* memcopy(void* dest, const void* src, size_t n) noexcept {
    return memcpy(dest, src, n);
}

// ============================================================================
// Arduino PROGMEM String Functions (platform-specific _P variants)
// ============================================================================
// These are used when strings are stored in program memory on embedded platforms

// Forward declarations for pgm_p type (defined in json.hpp)
namespace detail {
    typedef const void* pgm_p;
}

// Only declare PROGMEM functions on platforms that support them
#if defined(ARDUINO) && defined(__AVR__)

// fl::strlen_P - strlen for PROGMEM strings
size_t strlen_P(detail::pgm_p s) noexcept;

// fl::strcmp_P - strcmp with PROGMEM string
int strcmp_P(const char* a, detail::pgm_p b) noexcept;

// fl::memcmp_P - memcmp with PROGMEM memory
int memcmp_P(const void* a, detail::pgm_p b, size_t n) noexcept;

// fl::memcpy_P - memcpy from PROGMEM
void* memcpy_P(void* dest, detail::pgm_p src, size_t n) noexcept;

#endif  // defined(ARDUINO) && defined(__AVR__)

}  // namespace fl
