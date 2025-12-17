#pragma once

#include "fl/has_include.h"
#include "fl/stl/cstring.h"
#include "fl/stl/bit_cast.h"

// Conditional stack array support includes - must be before namespace
#ifndef FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION
#if defined(__clang__) || defined(ARDUINO_GIGA_M7) || defined(ARDUINO_GIGA)
// Clang doesn't have variable length arrays. Therefore we need to emulate them
// using alloca. It's been found that Arduino Giga M7 also doesn't support
// variable length arrays for some reason so we force it to emulate them as well
// in this case.
#define FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION 1
#else
// Else, assume the compiler is gcc, which has variable length arrays
#define FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION 0
#endif
#endif // FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION

/// @brief Stack-allocated array with automatic zero-initialization
///
/// Creates either a VLA (variable length array) or emulates one using alloca()
/// depending on compiler capabilities. The allocated array is automatically
/// zero-initialized.
///
/// @param TYPE The element type
/// @param NAME The variable name
/// @param SIZE The number of elements
///
/// @note This macro must be used at statement level. Use it like:
///       FASTLED_STACK_ARRAY(uint8_t, buffer, size);
#if !FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE)                                  \
    TYPE NAME[SIZE];                                                           \
    fl::memset(NAME, 0, sizeof(TYPE) * (SIZE))
#elif FL_HAS_INCLUDE(<alloca.h>)
#include <alloca.h>
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE)                                  \
    TYPE *NAME = fl::bit_cast_ptr<TYPE>(alloca(sizeof(TYPE) * (SIZE)));      \
    fl::memset(NAME, 0, sizeof(TYPE) * (SIZE))
#elif FL_HAS_INCLUDE(<malloc.h>)
#include <malloc.h>
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE)                                  \
    TYPE *NAME = fl::bit_cast_ptr<TYPE>(alloca(sizeof(TYPE) * (SIZE)));      \
    fl::memset(NAME, 0, sizeof(TYPE) * (SIZE))
#else
#error "Compiler does not allow variable type arrays."
#endif
