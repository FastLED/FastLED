
#pragma once

#include "fl/int.h"
#include "fl/clamp.h"
#include "fl/force_inline.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"
#include "fl/geometry.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)
FL_DISABLE_WARNING(double-promotion)
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION


namespace fl {

//////////////////////////////////// IMPLEMENTATION DETAILS
//////////////////////////////////////////

namespace map_range_detail {

// Primary template for map_range_math
template <typename T, typename U> struct map_range_math {
    static U map(T value, T in_min, T in_max, U out_min, U out_max) {
        if (in_min == in_max)
            return out_min;
        return out_min +
               (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }
};

// Specialization for u8 -> u8
template <> struct map_range_math<u8, u8> {
    static u8 map(u8 value, u8 in_min, u8 in_max,
                       u8 out_min, u8 out_max);  // Forward declare to avoid circular dependency
};

// Specialization for u16 -> u16
template <> struct map_range_math<u16, u16> {
    static u16 map(u16 value, u16 in_min, u16 in_max,
                        u16 out_min, u16 out_max);  // Forward declare to avoid circular dependency
};

// Partial specialization for U = vec2<V>
template <typename T, typename V> struct map_range_math<T, vec2<V>> {
    static vec2<V> map(T value, T in_min, T in_max, vec2<V> out_min,
                       vec2<V> out_max) {
        if (in_min == in_max) {
            return out_min;
        }
        // normalized [0..1]
        T scale = (value - in_min) / T(in_max - in_min);
        // deltas
        V dx = out_max.x - out_min.x;
        V dy = out_max.y - out_min.y;
        // lerp each component
        V x = out_min.x + V(dx * scale);
        V y = out_min.y + V(dy * scale);
        return {x, y};
    }
};

// Equality comparison helpers
template <typename T> bool equals(T a, T b) { return a == b; }
inline bool equals(float a, float b) { return FL_ALMOST_EQUAL_FLOAT(a, b); }
inline bool equals(double d, double d2) { return FL_ALMOST_EQUAL_DOUBLE(d, d2); }

} // namespace map_range_detail

//////////////////////////////////// PUBLIC API
//////////////////////////////////////////

template <typename T, typename U>
FASTLED_FORCE_INLINE U map_range(T value, T in_min, T in_max, U out_min,
                                 U out_max) {
    // Not fully tested with all unsigned types, so watch out if you use this
    // with u16 and you value < in_min.
    if (map_range_detail::equals(value, in_min)) {
        return out_min;
    }
    if (map_range_detail::equals(value, in_max)) {
        return out_max;
    }
    return map_range_detail::map_range_math<T, U>::map(value, in_min, in_max, out_min, out_max);
}

template <typename T, typename U>
FASTLED_FORCE_INLINE U map_range_clamped(T value, T in_min, T in_max, U out_min,
                                         U out_max) {
    // Not fully tested with all unsigned types, so watch out if you use this
    // with u16 and you value < in_min.
    value = clamp(value, in_min, in_max);
    return map_range<T, U>(value, in_min, in_max, out_min, out_max);
}

//////////////////////////////////// SPECIALIZATION IMPLEMENTATIONS
//////////////////////////////////////////

namespace map_range_detail {

// Now define the u8 specialization (forward declared above)
inline u8 map_range_math<u8, u8>::map(u8 value, u8 in_min, u8 in_max,
                   u8 out_min, u8 out_max) {
    if (value == in_min) {
        return out_min;
    }
    if (value == in_max) {
        return out_max;
    }
    // Promote u8 to i16 for mapping.
    i16 v16 = value;
    i16 in_min16 = in_min;
    i16 in_max16 = in_max;
    i16 out_min16 = out_min;
    i16 out_max16 = out_max;
    i16 out16 = fl::map_range<i16, i16>(v16, in_min16, in_max16,
                                                  out_min16, out_max16);
    if (out16 < 0) {
        out16 = 0;
    } else if (out16 > 255) {
        out16 = 255;
    }
    return static_cast<u8>(out16);
}

// Now define the u16 specialization (forward declared above)
inline u16 map_range_math<u16, u16>::map(u16 value, u16 in_min, u16 in_max,
                    u16 out_min, u16 out_max) {
    if (value == in_min) {
        return out_min;
    }
    if (value == in_max) {
        return out_max;
    }
    // Promote u16 to u32 for mapping to avoid overflow in multiplication.
    u32 v32 = value;
    u32 in_min32 = in_min;
    u32 in_max32 = in_max;
    u32 out_min32 = out_min;
    u32 out_max32 = out_max;
    u32 out32 = fl::map_range<u32, u32>(v32, in_min32, in_max32,
                                                  out_min32, out_max32);
    if (out32 > 65535) {
        out32 = 65535;
    }
    return static_cast<u16>(out32);
}

} // namespace map_range_detail

} // namespace fl

FL_DISABLE_WARNING_POP
