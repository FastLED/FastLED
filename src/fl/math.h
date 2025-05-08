
#pragma once

#include <math.h>
#include "fl/math_macros.h"
#include "fl/map_range.h"
#include "fl/clamp.h"



#ifndef FASTLED_NO_UNDEF_MATH_MACROS
#define FASTLED_NO_UNDEF_MATH_MACROS 0
#endif

#if !FASTLED_NO_UNDEF_MATH_MACROS
// The c math micros should be classified as a war crime.
#ifdef abs
  #undef abs
#endif

#ifdef min
  #undef min
#endif

#ifdef max
  #undef max
#endif

#ifdef sqrt
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


#ifndef FASTLED_FL_USING_MATH_MACROS
#define FASTLED_FL_USING_MATH_MACROS 0
#endif



#if FASTLED_FL_USING_MATH_MACROS

using fl::floor;
using fl::ceil;
using fl::abs;
using fl::min;
using fl::max;

#endif  // FASTLED_FL_USING_MATH_MACROS
