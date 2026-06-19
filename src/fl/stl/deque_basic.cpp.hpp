// IWYU pragma: private, include "fl/stl/deque_basic.h"
//
// deque_basic: implementation of the shared ring-buffer + grow-policy
// helpers used by every `fl::deque<T>` instantiation. See
// `deque_basic.h` for design rationale (FastLED #3250 / #3244 Tier 3H).

#include "fl/stl/deque_basic.h"

namespace fl {
namespace detail {

FL_NO_INLINE
fl::size deque_physical_index(fl::size front,
                              fl::size logical_idx,
                              fl::size capacity) FL_NOEXCEPT {
    return (front + logical_idx) % capacity;
}

FL_NO_INLINE
fl::size deque_grow_capacity(fl::size current_capacity,
                             fl::size min_required) FL_NOEXCEPT {
    fl::size next = current_capacity == 0
                        ? static_cast<fl::size>(kDequeInitialCapacity)
                        : current_capacity * 2;
    while (next < min_required) {
        next *= 2;
    }
    return next;
}

} // namespace detail
} // namespace fl
