// IWYU pragma: private, include "fl/stl/deque_basic.h"
//
// deque_basic: implementation of the shared chunk-map grow-policy helper
// used by every `fl::deque<T>` instantiation. See `deque_basic.h` for
// design rationale (FastLED #3270).

#include "fl/stl/deque_basic.h"

namespace fl {
namespace detail {

FL_NO_INLINE
fl::size deque_grow_map_capacity(fl::size current_map_capacity,
                                 fl::size min_required_chunks) FL_NOEXCEPT {
    fl::size next = current_map_capacity == 0
                        ? static_cast<fl::size>(kDequeInitialMapCapacity)
                        : current_map_capacity * 2;
    while (next < min_required_chunks) {
        if (next > static_cast<fl::size>(-1) / 2) {
            // Saturate: doubling further would overflow fl::size.
            return min_required_chunks;
        }
        next *= 2;
    }
    return next;
}

} // namespace detail
} // namespace fl
