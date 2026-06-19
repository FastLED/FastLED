#pragma once

// deque_basic: non-template chunk-map arithmetic helper shared by every
// `fl::deque<T>` instantiation.
//
// FastLED's `deque<T>` is a chunked container -- a map of fixed-size chunk
// pointers with the underlying elements stored in separately-allocated
// chunks. Element pointers stay valid across push at either end because
// only the chunk-pointer map ever reallocates, never the chunks. See
// FastLED #3270 for the design.
//
// The map-grow policy is pure arithmetic (current capacity -> next
// capacity) and doesn't depend on T, so it's emitted once per build via
// `FL_NO_INLINE` regardless of how many `deque<T>` instantiations exist.
// Mirrors the dedup pattern from `unordered_map_basic` (#3235 Tier 1C) and
// `flat_map_basic` (#3239).

#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"  // for FL_NO_INLINE
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Initial chunk-pointer-map capacity used when the deque first allocates a
// map. Small enough to amortize cheaply on first push; the map grows by
// doubling thereafter.
enum {
    kDequeInitialMapCapacity = 4,
};

// Compute the next chunk-map capacity when the deque's chunk map needs to
// grow. Doubles from the current capacity (or starts at
// `kDequeInitialMapCapacity` if `current == 0`) until the result is at
// least `min_required_chunks`. Saturates to `min_required_chunks` if
// further doubling would overflow `fl::size` (preventing a non-terminating
// loop).
//
// Caller is responsible for actually allocating the new map and copying
// chunk pointers across; this helper only computes the target capacity so
// the doubling loop isn't duplicated per `deque<T>` instantiation.
FL_NO_INLINE
fl::size deque_grow_map_capacity(fl::size current_map_capacity,
                                 fl::size min_required_chunks) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
