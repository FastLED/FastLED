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
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {
// Implementations. The public fl::name functions below are templates with an
// unused defaulted parameter, so wherever the C library version is also
// visible (ADL on an fl type, or a using-directive for fl) the non-template
// ::name wins the tie instead of the call being ambiguous (FastLED#4827).
namespace libc_impl {
size_t strlen(const char* s) FL_NO_EXCEPT;
int strcmp(const char* s1, const char* s2) FL_NO_EXCEPT;
int strncmp(const char* s1, const char* s2, size_t n) FL_NO_EXCEPT;
char* strcpy(char* dest, const char* src) FL_NO_EXCEPT;
char* strncpy(char* dest, const char* src, size_t n) FL_NO_EXCEPT;
char* strcat(char* dest, const char* src) FL_NO_EXCEPT;
char* strncat(char* dest, const char* src, size_t n) FL_NO_EXCEPT;
const char* strstr(const char* haystack, const char* needle) FL_NO_EXCEPT;
char* strchr(char* s, int c) FL_NO_EXCEPT;
const char* strchr(const char* s, int c) FL_NO_EXCEPT;
char* strrchr(char* s, int c) FL_NO_EXCEPT;
const char* strrchr(const char* s, int c) FL_NO_EXCEPT;
size_t strspn(const char* s1, const char* s2) FL_NO_EXCEPT;
size_t strcspn(const char* s1, const char* s2) FL_NO_EXCEPT;
char* strpbrk(char* s1, const char* s2) FL_NO_EXCEPT;
const char* strpbrk(const char* s1, const char* s2) FL_NO_EXCEPT;
char* strtok(char* s1, const char* s2) FL_NO_EXCEPT;
char* strerror(int errnum) FL_NO_EXCEPT;
void* memcpy(void* dest, const void* src, size_t n) FL_NO_EXCEPT;
int memcmp(const void* s1, const void* s2, size_t n) FL_NO_EXCEPT;
void* memmove(void* dest, const void* src, size_t n) FL_NO_EXCEPT;
void* memset(void* s, int c, size_t n) FL_NO_EXCEPT;
void* memchr(void* s, int c, size_t n) FL_NO_EXCEPT;
const void* memchr(const void* s, int c, size_t n) FL_NO_EXCEPT;
}  // namespace libc_impl


// ============================================================================
// Standard String Functions
// ============================================================================

// fl::strlen - wrapper for strlen
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE size_t strlen(const char* s) FL_NO_EXCEPT { return libc_impl::strlen(s); }

// fl::strcmp - wrapper for strcmp
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE int strcmp(const char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strcmp(s1, s2); }

// fl::strncmp - wrapper for strncmp
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE int strncmp(const char* s1, const char* s2, size_t n) FL_NO_EXCEPT { return libc_impl::strncmp(s1, s2, n); }

// fl::strcpy - wrapper for strcpy (use with caution - prefer strncpy)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strcpy(char* dest, const char* src) FL_NO_EXCEPT { return libc_impl::strcpy(dest, src); }

// fl::strncpy - bounded string copy that ALWAYS null-terminates.
// Copies at most n chars from src, then writes a terminator at
// dest[copied] (copied <= n). Pass n = capacity - 1 (dest sized >= n+1).
// Differs from ::strncpy, which leaves dest unterminated when
// strlen(src) >= n. Remaining destination bytes up to n are zero-padded.
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strncpy(char* dest, const char* src, size_t n) FL_NO_EXCEPT { return libc_impl::strncpy(dest, src, n); }

// fl::strcat - wrapper for strcat (use with caution - prefer strncat)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strcat(char* dest, const char* src) FL_NO_EXCEPT { return libc_impl::strcat(dest, src); }

// fl::strncat - wrapper for strncat
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strncat(char* dest, const char* src, size_t n) FL_NO_EXCEPT { return libc_impl::strncat(dest, src, n); }

// fl::strstr - wrapper for strstr
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const char* strstr(const char* haystack, const char* needle) FL_NO_EXCEPT { return libc_impl::strstr(haystack, needle); }

// fl::strchr - wrapper for strchr (find first occurrence of character)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strchr(char* s, int c) FL_NO_EXCEPT { return libc_impl::strchr(s, c); }

// fl::strchr - const version wrapper for strchr
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const char* strchr(const char* s, int c) FL_NO_EXCEPT { return libc_impl::strchr(s, c); }

// fl::strrchr - wrapper for strrchr (find last occurrence of character)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strrchr(char* s, int c) FL_NO_EXCEPT { return libc_impl::strrchr(s, c); }

// fl::strrchr - const version wrapper for strrchr
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const char* strrchr(const char* s, int c) FL_NO_EXCEPT { return libc_impl::strrchr(s, c); }

// fl::strspn - wrapper for strspn (length of initial segment matching characters)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE size_t strspn(const char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strspn(s1, s2); }

// fl::strcspn - wrapper for strcspn (length of initial segment not matching characters)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE size_t strcspn(const char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strcspn(s1, s2); }

// fl::strpbrk - wrapper for strpbrk (find first occurrence of any character from set)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strpbrk(char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strpbrk(s1, s2); }

// fl::strpbrk - const version wrapper for strpbrk
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const char* strpbrk(const char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strpbrk(s1, s2); }

// fl::strtok - wrapper for strtok (tokenize string, note: not thread-safe)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strtok(char* s1, const char* s2) FL_NO_EXCEPT { return libc_impl::strtok(s1, s2); }

// fl::strerror - wrapper for strerror (convert error number to string)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE char* strerror(int errnum) FL_NO_EXCEPT { return libc_impl::strerror(errnum); }

// ============================================================================
// Memory Functions
// ============================================================================

// fl::memcpy - wrapper for memcpy
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void* memcpy(void* dest, const void* src, size_t n) FL_NO_EXCEPT { return libc_impl::memcpy(dest, src, n); }

// fl::memcmp - wrapper for memcmp
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE int memcmp(const void* s1, const void* s2, size_t n) FL_NO_EXCEPT { return libc_impl::memcmp(s1, s2, n); }

// fl::memmove - wrapper for memmove (handles overlapping memory)
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void* memmove(void* dest, const void* src, size_t n) FL_NO_EXCEPT { return libc_impl::memmove(dest, src, n); }

// fl::memset - wrapper for memset
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void* memset(void* s, int c, size_t n) FL_NO_EXCEPT { return libc_impl::memset(s, c, n); }

// fl::memchr - wrapper for memchr
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void* memchr(void* s, int c, size_t n) FL_NO_EXCEPT { return libc_impl::memchr(s, c, n); }

// fl::memchr - const version wrapper for memchr
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const void* memchr(const void* s, int c, size_t n) FL_NO_EXCEPT { return libc_impl::memchr(s, c, n); }

// ============================================================================
// Legacy compatibility functions (aliases)
// ============================================================================

// fl::memcopy - legacy name for memcpy
inline void* memcopy(void* dest, const void* src, size_t n) FL_NO_EXCEPT {
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
#if defined(ARDUINO) && defined(FL_IS_AVR)

// fl::strlen_P - strlen for PROGMEM strings
size_t strlen_P(detail::pgm_p s) FL_NO_EXCEPT;

// fl::strcmp_P - strcmp with PROGMEM string
int strcmp_P(const char* a, detail::pgm_p b) FL_NO_EXCEPT;

// fl::memcmp_P - memcmp with PROGMEM memory
int memcmp_P(const void* a, detail::pgm_p b, size_t n) FL_NO_EXCEPT;

// fl::memcpy_P - memcpy from PROGMEM
void* memcpy_P(void* dest, detail::pgm_p src, size_t n) FL_NO_EXCEPT;

#endif  // defined(ARDUINO) && defined(__AVR__)

}  // namespace fl
