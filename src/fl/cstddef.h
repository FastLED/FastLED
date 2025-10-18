#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Standard Definition Types (stddef.h equivalents)
//
// Provides size_t, ptrdiff_t, max_align_t, and offsetof macro for both C and C++.
// C code should generally use fl/cstdint.h which includes the C versions of
// these types.
///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
namespace fl {

// FastLED equivalent of std::nullptr_t
typedef decltype(nullptr) nullptr_t;

// FastLED equivalent of std::size_t and std::ptrdiff_t
typedef __SIZE_TYPE__ size_t;

#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
// Fallback for compilers that don't define __PTRDIFF_TYPE__ (like older AVR-GCC)
typedef long ptrdiff_t;
#endif

// FastLED equivalent of std::max_align_t
// Use union approach (like standard library implementations) for fast compilation
typedef union {
    long long ll;
    long double ld;
    void* p;
} max_align_t;

} // namespace fl

// Declare size_t and ptrdiff_t in global namespace for third-party C++ code
// (avoid max_align_t and nullptr_t which conflict with system headers)
typedef fl::size_t size_t;
typedef fl::ptrdiff_t ptrdiff_t;

#else
// C language support: define types directly in global namespace
#ifndef FL_SIZE_T_DEFINED
#define FL_SIZE_T_DEFINED
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef FL_PTRDIFF_T_DEFINED
#define FL_PTRDIFF_T_DEFINED
#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
// Fallback for compilers that don't define __PTRDIFF_TYPE__ (like older AVR-GCC)
typedef long ptrdiff_t;
#endif
#endif
#endif  // __cplusplus

// FastLED equivalent of offsetof macro (stddef.h)
// Computes byte offset of a member within a struct/class at compile time
// Uses compiler builtin to avoid including <stddef.h>
// Note: This is C++ only and depends on __cplusplus guard above
#define FL_OFFSETOF(type, member) __builtin_offsetof(type, member)
