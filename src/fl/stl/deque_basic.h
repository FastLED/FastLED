#pragma once

// deque_basic: type-erased ring-buffer helpers shared by all
// `fl::deque<T>` instantiations.
//
// FastLED's `deque<T>` is a ring buffer with logical-to-physical index
// translation `(front + logical_idx) % capacity` and a power-of-two-ish
// grow policy (`8 -> 16 -> 32 -> ...`). Both pieces are purely
// arithmetic and don't depend on T beyond the templated wrapper using
// them. Extracting them into non-template helpers lets every distinct
// `deque<T>` instantiation share one body each, mirroring the
// `unordered_map_basic` (#3235 Tier 1C) and `flat_map_basic` (#3239)
// patterns.
//
// The split is intentionally narrow: only the index-computation and
// grow-policy math is shared. Element placement-new / destruction /
// the per-T move loop in `ensure_capacity` stays in the templated body
// because it depends on T's lifetime. (Full type erasure -- moving the
// move loop into the helper via an `element_ops` table -- is the
// `flat_map_basic` direction and only pays back on builds with many
// distinct `deque<T>` types; today the link has just 2-3, so the
// narrow-arithmetic split matches the leverage profile.)
//
// `FL_NO_INLINE` on the implementations is load-bearing under LTO --
// without it the compiler would inline the body back into each call
// site, defeating the dedup. The single indirect call per growth step
// is dwarfed by the allocation + per-element move it gates.
//
// See FastLED #3250 / #3244 Tier 3H. Note: at small instantiation counts
// (the current state of the link), per-call thunk overhead can equal or
// exceed savings. The value materializes on builds with many distinct
// `deque<T>` types, consistent with the meta lesson-learned from #3235.

#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"  // for FL_NO_INLINE
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Initial allocation size used by `deque<T>` when no element has been
// pushed yet. Exposed publicly so callers can match the constant in
// their own fast-path checks.
enum {
    kDequeInitialCapacity = 8,
};

// Compute the physical buffer index for a given logical position.
//
//   physical = (front + logical_idx) % capacity
//
// Caller is responsible for validating `capacity > 0` and `logical_idx
// < size`. The body lives in `deque_basic.cpp.hpp` so it's emitted once
// per build regardless of how many `deque<T>` instantiations exist.
FL_NO_INLINE
fl::size deque_physical_index(fl::size front,
                              fl::size logical_idx,
                              fl::size capacity) FL_NOEXCEPT;

// Compute the next capacity value when `deque<T>::ensure_capacity` needs
// to grow. Doubles from the current capacity (or starts at
// `kDequeInitialCapacity` if `current == 0`) and keeps doubling until
// it meets or exceeds `min_required`. Returns the chosen capacity.
//
// Caller is responsible for actually performing the allocation + element
// move; this helper only computes the target size so the doubling loop
// isn't duplicated per `deque<T>` instantiation.
FL_NO_INLINE
fl::size deque_grow_capacity(fl::size current_capacity,
                             fl::size min_required) FL_NOEXCEPT;

} // namespace detail
} // namespace fl
