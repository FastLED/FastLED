
#pragma once

#include <math.h>
#include "fl/math_macros.h"
#include "fl/map_range.h"
#include "fl/clamp.h"

#ifndef FASTLED_NO_UNDEF_MATH_MACROS
#define FASTLED_NO_UNDEF_MATH_MACROS 0
#endif

#if !FASTLED_NO_UNDEF_MATH_MACROS
// Save the original macro definitions if they exist
#ifdef abs
  #define FASTLED_MACRO_SAVE_ABS abs
  #undef abs
#endif

#ifdef min
  #define FASTLED_MACRO_SAVE_MIN min
  #undef min
#endif

#ifdef max
  #define FASTLED_MACRO_SAVE_MAX max
  #undef max
#endif

#ifdef sqrt
  #define FASTLED_MACRO_SAVE_SQRT sqrt
  #undef sqrt
#endif
#endif


namespace fl {

template <typename T>
inline T floor(T value) {
    if (value >= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::floor(static_cast<float>(value)));
}

template <typename T>
inline T ceil(T value) {
    if (value <= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::ceil(static_cast<float>(value)));
}

template <typename T>
inline T abs(T value) {
    return (value < 0) ? -value : value;
}

template <typename T>
inline T min(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
inline T max(T a, T b) {
    return (a > b) ? a : b;
}

inline float sqrt(float value) {
    return ::sqrtf(value);
}


}  // namespace fl




// Restore the original macro definitions if we saved them
#if !FASTLED_NO_UNDEF_MATH_MACROS
#ifdef FASTLED_MACRO_SAVE_ABS
  #define abs FASTLED_MACRO_SAVE_ABS
  #undef FASTLED_MACRO_SAVE_ABS
#endif

#ifdef FASTLED_MACRO_SAVE_MIN
  #define min FASTLED_MACRO_SAVE_MIN
  #undef FASTLED_MACRO_SAVE_MIN
#endif

#ifdef FASTLED_MACRO_SAVE_MAX
  #define max FASTLED_MACRO_SAVE_MAX
  #undef FASTLED_MACRO_SAVE_MAX
#endif

#ifdef FASTLED_MACRO_SAVE_SQRT
  #define sqrt FASTLED_MACRO_SAVE_SQRT
  #undef FASTLED_MACRO_SAVE_SQRT
#endif
#endif
