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