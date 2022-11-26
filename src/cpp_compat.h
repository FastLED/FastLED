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

#if __cplusplus < 201703L
#define REGISTER register
#else
#define REGISTER
#endif

#endif
