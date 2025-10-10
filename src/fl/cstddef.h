#pragma once


#ifdef FASTLED_TESTING
#include <cstddef>  // ok include
#endif

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
#ifndef FL_MAX_ALIGN_T_DEFINED
#define FL_MAX_ALIGN_T_DEFINED
#ifdef __BIGGEST_ALIGNMENT__
typedef struct {
    alignas(__BIGGEST_ALIGNMENT__) char __max_align_placeholder;
} max_align_t;
#else
// Fallback for compilers that don't support __BIGGEST_ALIGNMENT__
typedef long double max_align_t;
#endif
#endif

} // namespace fl 
