#pragma once

// IWYU pragma: private

#include "fl/channels/rx.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/result.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

/// Decode a completed RMT4 capture independently of its symbol-pair phase.
/// The SDK may start a symbol with LOW and end the packet after its last HIGH.
/// Callers must reject capture overflow before using the missing-LOW allowance.
fl::result<u32, DecodeError> decodeRmt4Edges(
    const ChipsetTiming4Phase &timing, fl::span<const EdgeTime> edges,
    fl::span<u8> out) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
