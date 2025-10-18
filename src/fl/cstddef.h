#pragma once

namespace fl {

// FastLED equivalent of std::nullptr_t
typedef decltype(nullptr) nullptr_t;

// FastLED equivalent of std::size_t and std::ptrdiff_t
// These are defined here for completeness but may already exist elsewhere
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

// FastLED equivalent of std::max_align_t
// Use union approach (like standard library implementations) for fast compilation
#ifndef FL_MAX_ALIGN_T_DEFINED
#define FL_MAX_ALIGN_T_DEFINED
typedef union {
    long long ll;
    long double ld;
    void* p;
} max_align_t;
#endif

} // namespace fl

// Declare size_t in global namespace for third-party code
// (avoid max_align_t and nullptr_t which conflict with system headers)
#ifndef size_t
typedef fl::size_t size_t;
#endif
#ifndef ptrdiff_t
typedef fl::ptrdiff_t ptrdiff_t;
#endif

// FastLED equivalent of offsetof macro (stddef.h)
// Computes byte offset of a member within a struct/class at compile time
// Uses compiler builtin to avoid including <stddef.h>
#ifndef FL_OFFSETOF
#define FL_OFFSETOF(type, member) __builtin_offsetof(type, member)
#endif
