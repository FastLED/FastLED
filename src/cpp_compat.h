/// @file cpp_compat.h
/// Compatibility functions based on C++ version

#ifndef __INC_CPP_COMPAT_H
#define __INC_CPP_COMPAT_H

#include "FastLED.h"

#if __cplusplus <= 199711L

/// Compile-time assertion checking, introduced in C++11
/// @see https://en.cppreference.com/w/cpp/language/static_assert
#define static_assert(expression, message)

/// Declares that it is possible to evaluate a value at compile time, introduced in C++11
/// @see https://en.cppreference.com/w/cpp/language/constexpr
#define constexpr const

#else

// things that we can turn on if we're in a C++11 environment
#endif

/// @def FASTLED_REGISTER
/// Helper macro to replace the deprecated 'register' keyword if we're
/// using modern C++ where it's been removed entirely.

#if __cplusplus < 201703L
#define FASTLED_REGISTER register
#else
  #ifdef FASTLED_REGISTER
    #undef FASTLED_REGISTER
  #endif
#define FASTLED_REGISTER

#endif

#endif
