#pragma once

// Stringify helper for pragma arguments
#define FL_STRINGIFY2(x) #x
#define FL_STRINGIFY(x) FL_STRINGIFY2(x)

// BEGIN BASE MACROS
#if defined(__clang__)
  #define FL_DISABLE_WARNING_PUSH         _Pragma("clang diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("clang diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(clang diagnostic ignored "-W" #warning))

#elif defined(__GNUC__) && (__GNUC__*100 + __GNUC_MINOR__) >= 406
  #define FL_DISABLE_WARNING_PUSH         _Pragma("GCC diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("GCC diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(GCC diagnostic ignored "-W" #warning))
#else
  #define FL_DISABLE_WARNING_PUSH
  #define FL_DISABLE_WARNING_POP
  #define FL_DISABLE_WARNING(warning)
#endif
// END BASE MACROS

// WARNING SPECIFIC MACROS THAT MAY NOT BE UNIVERSAL.
#if defined(__clang__)
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS \
    FL_DISABLE_WARNING(global-constructors)
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED \
    FL_DISABLE_WARNING(self-assign-overloaded)
  // Clang doesn't have format-truncation warning, use no-op
  #define FL_DISABLE_FORMAT_TRUNCATION
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE FL_DISABLE_WARNING(null-dereference)
  #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
#elif defined(__GNUC__) && (__GNUC__*100 + __GNUC_MINOR__) >= 406
  // GCC doesn't have global-constructors warning, use no-op
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
  // GCC doesn't have self-assign-overloaded warning, use no-op
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
  // GCC has format-truncation warning
  #define FL_DISABLE_FORMAT_TRUNCATION \
    FL_DISABLE_WARNING(format-truncation)
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE
  // implicit-fallthrough warning requires GCC >= 7.0
  #if (__GNUC__*100 + __GNUC_MINOR__) >= 700
    #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH FL_DISABLE_WARNING(implicit-fallthrough)
  #else
    #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
  #endif
#else
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
  #define FL_DISABLE_FORMAT_TRUNCATION
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE
#endif
// END WARNING SPECIFIC MACROS THAT MAY NOT BE UNIVERSAL.

// Fast math optimization controls with additional aggressive flags
#if defined(__clang__)
  #define FL_FAST_MATH_BEGIN \
    _Pragma("clang diagnostic push") \
    _Pragma("STDC FP_CONTRACT ON")

  #define FL_FAST_MATH_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_FAST_MATH_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"fast-math\")") \
    _Pragma("GCC optimize (\"tree-vectorize\")") \
    _Pragma("GCC optimize (\"unroll-loops\")")

  #define FL_FAST_MATH_END   _Pragma("GCC pop_options")

#elif defined(_MSC_VER)
  #define FL_FAST_MATH_BEGIN __pragma(float_control(precise, off))
  #define FL_FAST_MATH_END   __pragma(float_control(precise, on))
#else
  #define FL_FAST_MATH_BEGIN /* nothing */
  #define FL_FAST_MATH_END   /* nothing */
#endif

// Optimization Level O3
#if defined(__clang__)
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN \
    _Pragma("clang diagnostic push")

  #define FL_OPTIMIZATION_LEVEL_O3_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"O3\")")

  #define FL_OPTIMIZATION_LEVEL_O3_END   _Pragma("GCC pop_options")
#else
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN /* nothing */
  #define FL_OPTIMIZATION_LEVEL_O3_END   /* nothing */
#endif

// Optimization Level O0 (Debug/No optimization)
#if defined(__clang__)
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN \
    _Pragma("clang diagnostic push")

  #define FL_OPTIMIZATION_LEVEL_O0_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"O0\")")

  #define FL_OPTIMIZATION_LEVEL_O0_END   _Pragma("GCC pop_options")
#else
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN /* nothing */
  #define FL_OPTIMIZATION_LEVEL_O0_END   /* nothing */
#endif

// 6/29/2025 - Differences in all src vs individual compilation was 35 to 39 seconds.
// #define FASTLED_ALL_SRC 0

// All Source Build Control
// When FASTLED_ALL_SRC is enabled, all source is compiled into a single translation unit
// Compiler-specific defaults: Clang=faster with unified, GCC=slower with unified
#ifndef FASTLED_ALL_SRC
  #define FASTLED_ALL_SRC 0
#endif

// #if FASTLED_ALL_SRC == 1
// #error "FASTLED_ALL_SRC is defined"
// #endif
