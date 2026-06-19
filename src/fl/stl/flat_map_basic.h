#pragma once

// flat_map_basic: type-erased binary-search core shared by all
// `fl::flat_map<K, V, Less>` instantiations. The per-instantiation
// binary-search code (`lower_bound`, `upper_bound`) is pure byte-arithmetic
// + one comparator call per iteration; it does not depend on the K or V
// types beyond their layout (`sizeof(pair<K,V>)`) and the comparator's
// behavior. Sharing the loop across instantiations saves ~150 B per
// distinct flat_map type at the cost of ~30-50 B per call site.
//
// Design pattern mirrors `fl::vector_basic`: the templated public
// `flat_map<K, V, Less>` calls into the non-template helpers below
// through a `flat_map_ops` parameter block that carries (a) a thunk
// pointer that knows how to compare two `Key` values given their
// addresses and (b) the element stride (`sizeof(pair<K,V>)`).
//
// Comparator state is passed via a `const void*` context pointer so
// that stateful comparators (e.g. ones that capture string-intern
// tables) are supported uniformly with stateless ones (`fl::less<T>`).
// The thunk casts the context back to its concrete `Less*` and invokes
// `(*less)(key_a, key_b)`. For `fl::pair<K, V>`, `.first` is at offset
// 0 (standard layout) so the helpers can treat the storage as an array
// of K-prefixed records and compare on the prefix directly.
//
// FL_NO_INLINE on the implementations is load-bearing: with LTO enabled
// (default on every embedded target) the compiler would otherwise fold
// the body back into each call site, defeating the whole dedup goal.
//
// See FastLED #3235 Tier 2D. Note: at small instantiation counts the
// per-call thunk overhead can exceed the savings (sketches with 1-2
// flat_map types see neutral-or-slightly-negative deltas). The win
// scales with the number of distinct `flat_map<K,V,Less>` types in the
// link, so the value materializes on kitchen-sink builds (WASM compiler
// runtime, full RPC + audio + UI matrices) more than on focused
// sketches.

#include "fl/stl/int.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"  // for FL_NO_INLINE
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Function-pointer-with-context comparator: returns true iff *a < *b.
using flat_map_less_thunk_t = bool (*)(const void* ctx,
                                       const void* a, const void* b) FL_NO_EXCEPT;

struct flat_map_ops {
    flat_map_less_thunk_t less_fn;
    const void* less_ctx;   // address of the owning flat_map's `mLess`
    fl::size element_size;  // sizeof(pair<K, V>)
};

// lower_bound returns the index of the first element in [0, n) whose
// `.first` (a `Key` prefix at the start of each element) is NOT less
// than `*key`. If no such element exists, returns `n`. Caller turns the
// index back into an iterator via `begin() + idx`.
FL_NO_INLINE
fl::size flat_map_lower_bound_idx(const void* data, fl::size n,
                                  const void* key,
                                  const flat_map_ops& ops) FL_NO_EXCEPT;

// upper_bound returns the index of the first element in [0, n) whose
// `.first` is strictly greater than `*key` (equivalently: `less(*key,
// elem.first)` is true).
FL_NO_INLINE
fl::size flat_map_upper_bound_idx(const void* data, fl::size n,
                                  const void* key,
                                  const flat_map_ops& ops) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
