#pragma once

#include "fl/stl/noexcept.h"

#include "fl/gfx/leds.h"
#include "fl/stl/stdint.h"


namespace fl {

template<typename T> class Grid;

// Memory safe clear function for CRGB arrays.
template <int N> inline void clear(CRGB (&arr)[N]) FL_NO_EXCEPT {
    for (int i = 0; i < N; ++i) {
        arr[i] = CRGB::Black;
    }
}

inline void clear(Leds &leds) FL_NO_EXCEPT { leds.fill(CRGB::Black); }

template<fl::size W, fl::size H>
inline void clear(LedsXY<W, H> &leds) FL_NO_EXCEPT {
    leds.fill(CRGB::Black);
}

template<typename T>
inline void clear(Grid<T> &grid) FL_NO_EXCEPT {
    grid.clear();
}

// Default, when you don't know what do then call clear.
template<typename Container>
inline void clear(Container &container) FL_NO_EXCEPT {
    container.clear();
}



} // namespace fl
