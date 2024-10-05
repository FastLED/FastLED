#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

namespace fl_math {
    template <typename T>
    T min(T a, T b) {
        return a < b ? a : b;
    }
    
    template <typename T>
    T max(T a, T b) {
        return a > b ? a : b;
    }

    template <typename T>
    T abs(T a) {
        return a < 0 ? -a : a;
    }
}

FASTLED_NAMESPACE_END