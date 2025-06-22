
#pragma once

#include "fl/stdint.h"

#include "fl/force_inline.h"

namespace fl {

template <typename T> FASTLED_FORCE_INLINE T clamp(T value, T min, T max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

} // namespace fl
