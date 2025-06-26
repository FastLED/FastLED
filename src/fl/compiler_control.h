#pragma once

// Stringify helper for pragma arguments
#define FL_STRINGIFY2(x) #x
#define FL_STRINGIFY(x) FL_STRINGIFY2(x)

#if defined(__clang__)
  #define FL_DISABLE_WARNING_PUSH         _Pragma("clang diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("clang diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(clang diagnostic ignored "-W" #warning))
#elif defined(__GNUC__)
  #define FL_DISABLE_WARNING_PUSH         _Pragma("GCC diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("GCC diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(GCC diagnostic ignored "-W" #warning))
#else
  #define FL_DISABLE_WARNING_PUSH
  #define FL_DISABLE_WARNING_POP
  #define FL_DISABLE_WARNING(warning)
#endif

#if defined(__clang__)
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS \
    FL_DISABLE_WARNING(global-constructors)
#else
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS /* nothing */
#endif

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
