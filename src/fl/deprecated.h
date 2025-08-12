#pragma once

#if defined(__clang__)
// Clang: Do not mark classes as deprecated
  #define FASTLED_DEPRECATED_CLASS(msg)
  #ifndef FASTLED_DEPRECATED
    #define FASTLED_DEPRECATED(msg) __attribute__((deprecated(msg)))
  #endif
#elif defined(__GNUC__) // GCC (but not Clang)
  #ifndef FASTLED_DEPRECATED
    #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
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


#define FL_DEPRECATED(msg) FASTLED_DEPRECATED(msg)
#define FL_DEPRECATED_CLASS(msg) FASTLED_DEPRECATED_CLASS(msg)
