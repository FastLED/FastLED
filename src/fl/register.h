#pragma once

/// @def FASTLED_REGISTER
/// Helper macro to replace the deprecated 'register' keyword if we're
/// using modern C++ where it's been removed entirely.

#ifndef FASTLED_REGISTER
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 201703L
  #define FASTLED_REGISTER register
#else
  #define FASTLED_REGISTER
#endif
#endif
