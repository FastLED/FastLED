#if defined(__GNUC__)  // GCC or Clang
#  ifndef FASTLED_DEPRECATED
#    if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#      define FASTLED_DEPRECATED(msg) __attribute__((deprecated(msg)))
#    else
#      define FASTLED_DEPRECATED(msg) __attribute__((deprecated))
#    endif
#  endif
#else  // Other compilers
#  ifndef FASTLED_DEPRECATED
#    define FASTLED_DEPRECATED(msg)
#  endif
#endif