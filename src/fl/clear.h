#pragma once

namespace fl {
// Memory safe clear function for CRGB arrays.
template <int N> inline void clear(CRGB (&arr)[N]) {
    for (int i = 0; i < N; ++i) {
        arr[i] = CRGB::Black;
    }
}
} // namespace fl