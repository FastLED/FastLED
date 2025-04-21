

#include <stdint.h>

#include "fl/force_inline.h"

namespace fl {

template <typename T>
FASTLED_FORCE_INLINE T map_range(T value, T in_min, T in_max, T out_min,
                                 T out_max) {
    // Not fully tested with all unsigned types, so watch out if you use this
    // with uint16_t and you value < in_min.
    if (in_min == in_max) {
        return out_min;
    }
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

// Promote uint8_t to int16_t for mapping.
FASTLED_FORCE_INLINE uint8_t map_range(uint8_t value, uint8_t in_min,
                                       uint8_t in_max, uint8_t out_min,
                                       uint8_t out_max) {
    int16_t v16 = value;
    int16_t in_min16 = in_min;
    int16_t in_max16 = in_max;
    int16_t out_min16 = out_min;
    int16_t out_max16 = out_max;
    int16_t out16 = map_range<int16_t>(v16, in_min16, in_max16, out_min16,
                                       out_max16);

    if (out16 < 0) {
        out16 = 0;
    } else if (out16 > 255) {
        out16 = 255;
    }
    return static_cast<uint8_t>(out16);
}

} // namespace fl