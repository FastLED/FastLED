#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

namespace fl_math {
    
    template <typename T>
    T Min(T a, T b) {
        return a < b ? a : b;
    }
    
    template <typename T>
    T Max(T a, T b) {
        return a > b ? a : b;
    }

    template <typename T>
    T Abs(T a) {
        return a < 0 ? -a : a;
    }
}


FASTLED_NAMESPACE_END