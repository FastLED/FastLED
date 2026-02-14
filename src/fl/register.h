#pragma once

#include "fl/compiler_control.h"

/// @def FASTLED_REGISTER
/// Helper macro to replace the deprecated 'register' keyword if we're
/// using modern C++ where it's been removed entirely.
///
/// Each file that uses FASTLED_REGISTER should wrap its contents with:
///   FL_DISABLE_WARNING_PUSH
///   FL_DISABLE_WARNING_DEPRECATED_REGISTER
///   ... code using FASTLED_REGISTER ...
///   FL_DISABLE_WARNING_POP

#ifndef FASTLED_REGISTER
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 201703L
  #define FASTLED_REGISTER register
#else
  // C++17+: register keyword removed from the language
  #define FASTLED_REGISTER
#endif
FL_DISABLE_WARNING_POP
#endif
