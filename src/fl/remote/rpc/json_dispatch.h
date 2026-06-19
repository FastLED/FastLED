#pragma once

// json_dispatch: type-erased core helpers for JSON-to-primitive conversion.
//
// Legacy `fl::detail::JsonToIntegerVisitor<T>` / `JsonToFloatVisitor<T>` /
// `JsonToBoolVisitor` / `JsonToStringVisitor` (formerly in
// `fl/remote/rpc/json_visitors.h`) each contained ~10 `operator()`
// overloads (one per `json_value::variant_t` alternative). When invoked
// via `data.visit(visitor)`, the variant emitted a `visit_fn<T, Visitor>`
// thunk per (alternative × visitor) pair, plus the per-T body fan-out for
// the templated visitors -- the audit in #3235 catalogued ~2-3 KB of
// visitor codegen on LPC845.
//
// This dispatch layer collapses that per-(visitor × alternative)
// explosion to **one** shared non-template body per output type. The
// thin templated `JsonToType<T>::convert` (in `json_to_type.h`) calls
// the appropriate core, gets back the canonical type
// (`i64` / `bool` / `float` / `string`) plus a `TypeConversionResult`,
// and applies per-T narrowing + range checking. The per-(T × alternative)
// thunk explosion is gone, which is why the visitor classes themselves
// were deleted in #3251 (no remaining callers).
//
// `FL_NO_INLINE` on the implementations is load-bearing under LTO --
// without it the compiler inlines the body back at each call site,
// defeating dedup.
//
// See FastLED #3247 / #3244 Tier 2E Phase 2, plus #3251 / Tier 3I dead-code
// removal of the formerly-templated visitor classes.

#include "fl/remote/rpc/type_conversion_result.h"  // IWYU pragma: keep
#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"  // FL_NO_INLINE
#include "fl/stl/noexcept.h"

namespace fl {

struct json_value;

namespace detail {

// Each core returns the converted result in `out_value` and emits any
// warnings / errors via the returned `TypeConversionResult`. Caller is
// responsible for applying per-T narrowing on the result (e.g. casting
// the returned `i64` to `int8_t` with overflow detection).
//
// All four cores switch over `val.data.tag()` and read the active
// alternative via `data.template ptr<T>()` -- no `data.visit(visitor)`
// call, no `visit_fn<T, Visitor>` thunk instantiation.

FL_NO_INLINE
TypeConversionResult json_convert_to_i64_core(const json_value& val,
                                              fl::i64* out_value) FL_NOEXCEPT;

FL_NO_INLINE
TypeConversionResult json_convert_to_bool_core(const json_value& val,
                                               bool* out_value) FL_NOEXCEPT;

FL_NO_INLINE
TypeConversionResult json_convert_to_float_core(const json_value& val,
                                                float* out_value) FL_NOEXCEPT;

FL_NO_INLINE
TypeConversionResult json_convert_to_string_core(const json_value& val,
                                                 fl::string* out_value) FL_NOEXCEPT;

} // namespace detail
} // namespace fl
