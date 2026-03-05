#pragma once

// Generic math functions dispatching between float and fixed_point types.
// Float/double: forwards to fl::expf/fl::sinf/etc. from stl/math.h.
// Fixed-point:  free functions are provided by fl/stl/fixed_point.h.

#include "fl/stl/fixed_point.h"
#include "fl/stl/math.h"
#include "fl/stl/type_traits.h"

namespace fl {

// --- exp(x) --- dispatch for fixed-point (float version lives in stl/math.h)
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
exp(T x) { return fl::expfp(x); }

// --- sincos(angle, &out_sin, &out_cos) for floating-point types ---
template <typename T>
inline typename enable_if<is_floating_point<T>::value>::type
sincos(T angle, T& out_sin, T& out_cos) {
    out_sin = static_cast<T>(fl::sinf(static_cast<float>(angle)));
    out_cos = static_cast<T>(fl::cosf(static_cast<float>(angle)));
}

} // namespace fl
