
#pragma once

#include "fl/clamp.h"
#include "fl/map_range.h"
#include "fl/math_macros.h"
#include <math.h>

namespace fl {

template <typename T> inline T floor(T value) {
    if (value >= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::floor(static_cast<float>(value)));
}

template <typename T> inline T ceil(T value) {
    if (value <= 0) {
        return static_cast<T>(static_cast<int>(value));
    }
    return static_cast<T>(::ceil(static_cast<float>(value)));
}

// Arduino will define this in the global namespace as macros, so we can't
// define them ourselves.
// template <typename T>
// inline T abs(T value) {
//     return (value < 0) ? -value : value;
// }

// template <typename T>
// inline T min(T a, T b) {
//     return (a < b) ? a : b;
// }

// template <typename T>
// inline T max(T a, T b) {
//     return (a > b) ? a : b;
// }

} // namespace fl
