// IWYU pragma: private, include "fl/stl/unordered_map_basic.h"
//
// unordered_map_basic: implementation of the type-erased probing-
// sequence helper shared across all `fl::unordered_map<K, V, ...>`
// instantiations. See `unordered_map_basic.h` for design rationale
// (FastLED #3235 Tier 1C).

#include "fl/stl/unordered_map_basic.h"

namespace fl {
namespace detail {

FL_NO_INLINE
fl::size unordered_map_probe_idx(fl::size h, fl::size i,
                                 fl::size mask, fl::size cap) FL_NO_EXCEPT {
    if (cap <= kUnorderedMapLinearProbeOnlyThreshold) {
        return (h + i) & mask;
    }
    if (i < kUnorderedMapQuadraticProbingTries) {
        return (h + i + i * i) & mask;
    }
    return (h + i) & mask;
}

} // namespace detail
} // namespace fl
