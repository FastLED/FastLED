
#pragma once

#include <math.h>
#include "fl/math_macros.h"
#include "fl/map_range.h"
#include "fl/clamp.h"


#pragma push_macro("abs")

#ifdef abs
#undef abs
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

}

#pragma pop_macro("abs")