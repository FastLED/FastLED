#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED C Standard Library Compatibility Header
//
// This header provides a compatibility layer for third-party code that
// expects standard C library functions like malloc, free, exit, etc.
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {
// C-style comparison function type for qsort
typedef int (*qsort_compare_fn)(const void*, const void*);

// Implementations. The public fl::name functions below are templates with an
// unused defaulted parameter, so wherever the C library version is also
// visible (ADL on an fl type, or a using-directive for fl) the non-template
// ::name wins the tie instead of the call being ambiguous (FastLED#4827).
namespace libc_impl {
void *aligned_alloc(fl::size_t alignment, fl::size_t size) FL_NO_EXCEPT;
long strtol(const char* str, char** endptr, int base) FL_NO_EXCEPT;
unsigned long strtoul(const char* str, char** endptr, int base) FL_NO_EXCEPT;
int atoi(const char* str) FL_NO_EXCEPT;
long atol(const char* str) FL_NO_EXCEPT;
double strtod(const char* str, char** endptr) FL_NO_EXCEPT;
void qsort(void* base, size_t nmemb, size_t size, qsort_compare_fn compar) FL_NO_EXCEPT;
u32 rand() FL_NO_EXCEPT;
const char* getenv(const char* name) FL_NO_EXCEPT;
}  // namespace libc_impl


// ============================================================================
// fl::aligned_alloc / fl::aligned_free — Portable over-aligned allocation
// ============================================================================
// Mirrors std::aligned_alloc from <cstdlib> (C++17 / C11).
// Falls back to plain malloc on platforms where over-alignment is
// unsupported or unnecessary (AVR, ESP8266).
// Defined in cstdlib.cpp.hpp.

template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void *aligned_alloc(fl::size_t alignment, fl::size_t size) FL_NO_EXCEPT { return libc_impl::aligned_alloc(alignment, size); }

namespace detail {
/// Portable over-allocate-and-align used by aligned_alloc on bare-metal
/// targets; free only with aligned_free_via_malloc (#4817).
void *aligned_alloc_via_malloc(fl::size_t alignment, fl::size_t size) FL_NO_EXCEPT;
void aligned_free_via_malloc(void *ptr) FL_NO_EXCEPT;
}  // namespace detail

// Portable free for memory obtained from fl::aligned_alloc.
// On POSIX, std::free suffices; on Windows, _aligned_free is required.
void aligned_free(void *ptr) FL_NO_EXCEPT;

// Convert string to long integer
// Similar to standard strtol but without locale support
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE long strtol(const char* str, char** endptr, int base) FL_NO_EXCEPT { return libc_impl::strtol(str, endptr, base); }

// Convert string to unsigned long integer
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE unsigned long strtoul(const char* str, char** endptr, int base) FL_NO_EXCEPT { return libc_impl::strtoul(str, endptr, base); }

// Convert string to integer
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE int atoi(const char* str) FL_NO_EXCEPT { return libc_impl::atoi(str); }

// Convert string to long
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE long atol(const char* str) FL_NO_EXCEPT { return libc_impl::atol(str); }

// Convert string to double
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE double strtod(const char* str, char** endptr) FL_NO_EXCEPT { return libc_impl::strtod(str, endptr); }


// qsort - Quick sort function compatible with C stdlib qsort
// Sorts an array of elements using the provided comparison function
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void qsort(void* base, size_t nmemb, size_t size, qsort_compare_fn compar) FL_NO_EXCEPT { return libc_impl::qsort(base, nmemb, size, compar); }

// Pseudo-random number generator (mirrors ::rand()).
// Returns u32 — values are always non-negative [0, RAND_MAX].
// Fixed-width avoids AVR's 16-bit int truncation.
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE u32 rand() FL_NO_EXCEPT { return libc_impl::rand(); }

// Get the value of an environment variable
// Only functional on FASTLED_TESTING (stub platform), returns nullptr otherwise
// This avoids std:: namespace dependencies on embedded platforms
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE const char* getenv(const char* name) FL_NO_EXCEPT { return libc_impl::getenv(name); }

} // namespace fl
