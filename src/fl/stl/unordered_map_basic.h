#pragma once

// unordered_map_basic: type-erased probing-sequence helpers shared by
// all `fl::unordered_map<K, V, Hash, Equal>` instantiations.
//
// FastLED's `unordered_map` is open-addressed with linear probing
// (cap ≤ 8) or quadratic-then-linear probing (cap > 8). The probe
// index sequence is purely a function of (hash, cap) -- it has no
// dependency on K, V, Hash, or Equal beyond the hash value the caller
// already computed. Extracting the probing math into a non-template
// helper lets every distinct unordered_map instantiation share one
// body (~30-50 B) instead of duplicating it (per the audit, the bulk
// of the per-instantiation cost is in `find_slot` / `find_index` /
// `find_unoccupied_index_using_bitset`).
//
// Design: caller passes (hash, attempt_index, cap, mask); we return the
// next probe index in the sequence. Caller handles bucket inspection,
// equality, and bitset state because those depend on K/V/Equal.
//
// The split is intentionally narrow: only the index-computation logic
// is shared. Full type erasure of `find_slot` would require also
// erasing the bucket-state checks (which touch K via `_equal(bucket.key,
// key)`) and the bitset reads; doing that pays back only on builds
// with many distinct unordered_map types. This narrow-scope helper
// captures the part that's purely arithmetic with no leverage threshold.
//
// `FL_NO_INLINE` on the implementations is load-bearing under LTO -
// without it the compiler would inline the body back into each call
// site, defeating the dedup. The single indirect call per probe step is
// dwarfed by the bucket inspection + bitset read it gates.
//
// See FastLED #3235 Tier 1C.

#include "fl/stl/int.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"  // for FL_NO_INLINE
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Probing constants -- exposed publicly so callers can match their fast-path
// detection (cap <= 8 means linear-only).
enum {
    kUnorderedMapLinearProbeOnlyThreshold = 8,
    kUnorderedMapQuadraticProbingTries = 8,
};

// Returns the i-th probe index for an open-addressed table of capacity `cap`
// (must be a power of two) given an initial hash slot `h`. `mask` is
// `cap - 1` (passed separately so the caller can cache it across probes).
//
// Probing sequence:
//   - cap <= 8: pure linear:    (h + i) & mask
//   - cap >  8, i < 8: quadratic: (h + i + i*i) & mask
//   - cap >  8, i >= 8: linear fallback
//
// Caller is expected to terminate the sequence when it has examined all
// `cap` slots or found an empty / matching slot.
FL_NO_INLINE
fl::size unordered_map_probe_idx(fl::size h, fl::size i,
                                 fl::size mask, fl::size cap) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
