#pragma once

/// @def FASTLED_REGISTER
/// Helper macro to replace the deprecated 'register' keyword if we're
/// using modern C++ where it's been removed entirely.

#ifndef FASTLED_REGISTER
// Always use empty FASTLED_REGISTER for modern C++ builds
// The 'register' keyword is deprecated in C++11 and removed in C++17
// Since FastLED now targets C++17+, we always use empty definition
#define FASTLED_REGISTER
#endif
