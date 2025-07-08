#pragma once

#include "fl/leds.h"
#include "fl/stdint.h"


namespace fl {

template<typename T>
class Grid;

// Memory safe clear function for CRGB arrays.
template <int N> inline void clear(CRGB (&arr)[N]) {
    for (int i = 0; i < N; ++i) {
        arr[i] = CRGB::Black;
    }
}

inline void clear(Leds &leds) { leds.fill(CRGB::Black); }

template<fl::size W, fl::size H>
inline void clear(LedsXY<W, H> &leds) {
    leds.fill(CRGB::Black);
}

template<typename T>
inline void clear(Grid<T> &grid) {
    grid.clear();
}

// Default, when you don't know what do then call clear.
template<typename Container>
inline void clear(Container &container) {
    container.clear();
}



} // namespace fl
