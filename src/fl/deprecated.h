#pragma once

#include "platforms/is_platform.h"

#if defined(FL_IS_CLANG)
// Clang: Do not mark classes as deprecated
  #define FASTLED_DEPRECATED_CLASS(msg)
  #ifndef FASTLED_DEPRECATED
    #define FASTLED_DEPRECATED(msg) __attribute__((deprecated(msg)))
  #endif
#elif defined(FL_IS_GCC) // GCC (but not Clang)
  #ifndef FASTLED_DEPRECATED
    #if FL_GCC_VERSION >= 405
      #define FASTLED_DEPRECATED(msg) __attribute__((deprecated(msg)))
      #define FASTLED_DEPRECATED_CLASS(msg) __attribute__((deprecated(msg)))
    #else
      #define FASTLED_DEPRECATED(msg) __attribute__((deprecated))
      #define FASTLED_DEPRECATED_CLASS(msg)
    #endif
  #endif
#else // Other compilers
  #ifndef FASTLED_DEPRECATED
    #define FASTLED_DEPRECATED(msg)
  #endif
  #define FASTLED_DEPRECATED_CLASS(msg)
#endif


#ifndef FL_DEPRECATED
#define FL_DEPRECATED(msg) FASTLED_DEPRECATED(msg)
#endif

#ifndef FL_DEPRECATED_CLASS
#define FL_DEPRECATED_CLASS(msg) FASTLED_DEPRECATED_CLASS(msg)
#endif
