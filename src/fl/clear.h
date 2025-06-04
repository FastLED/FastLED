#pragma once

#include "fl/leds.h"
#include "fl/stdint.h"

namespace fl {
// Memory safe clear function for CRGB arrays.
template <int N> inline void clear(CRGB (&arr)[N]) {
    for (int i = 0; i < N; ++i) {
        arr[i] = CRGB::Black;
    }
}

inline void clear(Leds &leds) { leds.fill(CRGB::Black); }

template<size_t W, size_t H>
inline void clear(LedsXY<W, H> &leds) {
    leds.fill(CRGB::Black);
}

} // namespace fl