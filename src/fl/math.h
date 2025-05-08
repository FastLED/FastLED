
#pragma once

#include <math.h>
#include "fl/math_macros.h"
#include "fl/map_range.h"
#include "fl/clamp.h"

// Don't let the math macros interfere with our code.
#pragma push_macro("abs")
#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("sqrt")

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
    return ::sqrt(value);
}


}  // namespace fl

// Restore the math macros
#pragma pop_macro("abs")
#pragma pop_macro("min")
#pragma pop_macro("max")
#pragma pop_macro("sqrt")
