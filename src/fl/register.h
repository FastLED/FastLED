#pragma once

#include "platforms/is_platform.h"

/// @def FASTLED_REGISTER
/// Helper macro to replace the deprecated 'register' keyword if we're
/// using modern C++ where it's been removed entirely.

#ifndef FASTLED_REGISTER
// Suppress deprecation warnings for 'register' keyword in older C++ standards
#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-register"
#elif defined(FL_IS_WIN_MSVC)
  #pragma warning(push)
  #pragma warning(disable: 4996)
#endif

#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 201703L
  #define FASTLED_REGISTER register
#else
  #define FASTLED_REGISTER
#endif

#if defined(FL_IS_GCC) || defined(FL_IS_CLANG)
  #pragma GCC diagnostic pop
#elif defined(FL_IS_WIN_MSVC)
  #pragma warning(pop)
#endif
#endif
