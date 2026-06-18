// IWYU pragma: private, include "fl/stl/flat_map_basic.h"
//
// flat_map_basic: implementation of the type-erased binary-search
// helpers shared across all `fl::flat_map<K, V, Less>` instantiations.
// See `flat_map_basic.h` for design rationale (FastLED #3235 Tier 2D).

#include "fl/stl/flat_map_basic.h"

namespace fl {
namespace detail {

FL_NO_INLINE
fl::size flat_map_lower_bound_idx(const void* data, fl::size n,
                                  const void* key,
                                  const flat_map_ops& ops) FL_NOEXCEPT {
    const char* base = static_cast<const char*>(data);
    fl::size first_idx = 0;
    fl::size count = n;
    while (count > 0) {
        fl::size step = count / 2;
        fl::size mid_idx = first_idx + step;
        // `.first` of `pair<Key, Value>` is at offset 0 (standard layout).
        const void* mid_key = base + mid_idx * ops.element_size;
        if (ops.less_fn(ops.less_ctx, mid_key, key)) {
            first_idx = mid_idx + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first_idx;
}

FL_NO_INLINE
fl::size flat_map_upper_bound_idx(const void* data, fl::size n,
                                  const void* key,
                                  const flat_map_ops& ops) FL_NOEXCEPT {
    const char* base = static_cast<const char*>(data);
    fl::size first_idx = 0;
    fl::size count = n;
    while (count > 0) {
        fl::size step = count / 2;
        fl::size mid_idx = first_idx + step;
        const void* mid_key = base + mid_idx * ops.element_size;
        // upper_bound: advance past elements where !(key < elem.first).
        if (!ops.less_fn(ops.less_ctx, key, mid_key)) {
            first_idx = mid_idx + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first_idx;
}

} // namespace detail
} // namespace fl
