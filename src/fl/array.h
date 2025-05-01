#pragma once


// FASTLED_STACK_ARRAY
// An array of variable length that is allocated on the stack using
// either alloca or a variable length array (VLA) support built into the
// the compiler.
// Example:
//   Instead of: int array[buff_size];
//   You'd use: FASTLED_STACK_ARRAY(int, array, buff_size);

#ifndef FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION
#if defined(__clang__) || defined(ARDUINO_GIGA_M7) || defined(ARDUINO_GIGA)
// Clang doesn't have variable length arrays. Therefore we need to emulate them using
// alloca. It's been found that Arduino Giga M7 also doesn't support variable length arrays
// for some reason so we force it to emulate them as well in this case.
#define FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION 1
#else
// Else, assume the compiler is gcc, which has variable length arrays
#define FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION 0
#endif
#endif  // FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION

#if !FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE) TYPE NAME[SIZE]
#elif __has_include(<alloca.h>)
#include <alloca.h>
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE) \
    TYPE* NAME = reinterpret_cast<TYPE*>(alloca(sizeof(TYPE) * (SIZE)))
#elif __has_include(<cstdlib>)
#include <cstdlib>
#define FASTLED_STACK_ARRAY(TYPE, NAME, SIZE) \
    TYPE* NAME = reinterpret_cast<TYPE*>(alloca(sizeof(TYPE) * (SIZE)))
#else
#error "Compiler does not allow variable type arrays."
#endif
