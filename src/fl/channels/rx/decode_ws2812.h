/// @file fl/channels/rx/decode_ws2812.h
/// @brief Shared 4-phase WS2812 edge-pair -> byte decoder (declaration).
///
/// Lifted out of the duplicated bodies in
/// `platforms/shared/rx_device_native.cpp.hpp::NativeRxDevice::decode` and
/// `platforms/arm/lpc/rx_sct_capture.cpp.hpp::LpcSctRxChannel::decode` --
/// both were verbatim copies of the same algorithm. The per-driver decode()
/// wrappers keep their device-specific FL_WARN messages (which reference
/// the pin number; this function does not know about it).
///
/// **Scope.** This decoder handles the variant used by Native and LPC SCT --
/// reset-pulse breakout when LOW exceeds `reset_min_us`, gap-tolerance
/// fallback when `gap_tolerance_ns > 0`, 10 %-error gate against the
/// number of bits actually decoded. The FlexPWM (`decodeEdges` in
/// `rx_flexpwm_channel.cpp.hpp`) and FlexIO (`decodeEdgesFlexIo` in
/// `rx_flexio_channel.cpp.hpp`) drivers use a slightly different variant
/// (partial-byte left-shift, no reset detection, no gap tolerance,
/// different error counting). Folding those into this function is a
/// separate change tracked under FastLED #3035 Phase 3 -- see the issue
/// for the rationale and the behaviour deltas.
///
/// **Preserved-from-original quirks** (intentionally NOT fixed in the
/// consolidation, see #3035 follow-ups):
///   1. Reset-pulse detection fires BEFORE gap-tolerance evaluation, so
///      a LOW pulse in the documented "reset_min_us .. gap_tolerance_ns"
///      window currently terminates the frame instead of being tolerated.
///      Matches the prior behaviour of both Native and LPC decoders.
///   2. Error-rate denominator is `bytes_written * 8` rather than total
///      decoded bits; the partial trailing `bit_index` residue is not
///      counted. Can over-reject frames that end mid-byte. Same
///      preserved behaviour.

#pragma once

#include "fl/channels/rx.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/result.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace channels {
namespace rx {

/// @brief Decode a WS2812 edge-pair stream into bytes.
/// @param timing  4-phase timing thresholds (T0H/T0L/T1H/T1L windows + reset).
/// @param edges   Captured edge stream. Each WS2812 bit is one HIGH edge
///                followed by one LOW edge, both labelled with their
///                duration in ns.
/// @param out     Destination byte buffer. MSB-first per WS2812 wire order.
/// @return Number of decoded bytes on success, or a `DecodeError` on
///         buffer overflow / >10 % error rate. An empty `edges` span
///         returns `INVALID_ARGUMENT` so callers can distinguish "no data
///         captured" from "captured but garbage".
fl::result<u32, DecodeError>
decodeWs2812Edges(const ChipsetTiming4Phase& timing,
                  fl::span<const EdgeTime> edges,
                  fl::span<u8> out) FL_NOEXCEPT;

}  // namespace rx
}  // namespace channels
}  // namespace fl
