/// @file fl/channels/rx/decode_ws2812.h
/// @brief Shared 4-phase WS2812 edge-pair → byte decoder.
///
/// Lifted out of the duplicated bodies in
/// `platforms/shared/rx_device_native.cpp.hpp::NativeRxDevice::decode` and
/// `platforms/arm/lpc/rx_sct_capture.cpp.hpp::LpcSctRxChannel::decode` —
/// both were verbatim copies of the same algorithm. Header-only so the
/// per-driver decode() wrappers can keep their device-specific FL_WARN
/// messages (which reference the pin number, which this function doesn't
/// know about).
///
/// **Scope.** This decoder handles the variant used by Native and LPC SCT —
/// reset-pulse breakout when LOW exceeds `reset_min_us`, gap-tolerance
/// fallback when `gap_tolerance_ns > 0`, 10 %-error gate against the
/// number of bits actually decoded. The FlexPWM (`decodeEdges` in
/// `rx_flexpwm_channel.cpp.hpp`) and FlexIO (`decodeEdgesFlexIo` in
/// `rx_flexio_channel.cpp.hpp`) drivers use a slightly different variant
/// (partial-byte left-shift, no reset detection, no gap tolerance,
/// different error counting). Folding those into this function is a
/// separate change tracked under FastLED #3035 Phase 3 — see the issue
/// for the rationale and the behaviour deltas.

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
///
/// **Algorithm.** Walks the edge stream in pairs:
///   * (HIGH=true, LOW=false) → measure both durations against the four
///     timing windows; emit a 0 or 1 bit accordingly.
///   * Polarity error (two HIGHs in a row, etc.) → bump error_count,
///     advance one edge to resync.
///   * LOW longer than `reset_min_us` → WS2812 reset pulse → end of
///     frame, return what we have so far (the partial in-progress byte
///     is dropped).
///   * LOW between `reset_min_us` and `gap_tolerance_ns` → tolerated
///     inter-frame gap (PARLIO style); attempt to decode the bit from
///     the HIGH duration alone.
///   * Otherwise → bump error_count, advance two edges.
///
/// After the walk, reject the decode with `HIGH_ERROR_RATE` if more
/// than 10 % of the decoded bits were errors. This matches the
/// behaviour of the original per-driver copies.
inline fl::result<u32, DecodeError>
decodeWs2812Edges(const ChipsetTiming4Phase& timing,
                  fl::span<const EdgeTime> edges,
                  fl::span<u8> out) FL_NOEXCEPT {
    const size_t edge_count = edges.size();
    if (edge_count == 0) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t bytes_written = 0;
    size_t bit_index = 0;
    u8 current_byte = 0;
    u32 error_count = 0;

    size_t i = 0;
    while (i + 1 < edge_count) {
        const EdgeTime high_edge = edges[i];
        const EdgeTime low_edge  = edges[i + 1];

        if (!high_edge.high || low_edge.high) {
            error_count++;
            i++;
            continue;
        }

        const u32 high_ns = high_edge.ns;
        const u32 low_ns  = low_edge.ns;

        bool is_bit1 = (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
                        low_ns  >= timing.t1l_min_ns && low_ns  <= timing.t1l_max_ns);
        bool is_bit0 = (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
                        low_ns  >= timing.t0l_min_ns && low_ns  <= timing.t0l_max_ns);

        if (!is_bit0 && !is_bit1) {
            if (low_ns >= static_cast<u32>(timing.reset_min_us) * 1000u) {
                break;  // WS2812 reset pulse → end of frame
            }
            if (timing.gap_tolerance_ns > 0 && low_ns <= timing.gap_tolerance_ns) {
                if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                    is_bit1 = true;
                } else if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                    is_bit0 = true;
                }
            }
            if (!is_bit0 && !is_bit1) {
                error_count++;
                i += 2;
                continue;
            }
        }

        const u8 bit = is_bit1 ? u8{1} : u8{0};
        current_byte = static_cast<u8>((current_byte << 1) | bit);
        bit_index++;

        if (bit_index == 8) {
            if (bytes_written >= out.size()) {
                return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out[bytes_written++] = current_byte;
            current_byte = 0;
            bit_index = 0;
        }

        i += 2;
    }

    if (bytes_written > 0 && error_count * 10 > bytes_written * 8) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::result<u32, DecodeError>::success(static_cast<u32>(bytes_written));
}

}  // namespace rx
}  // namespace channels
}  // namespace fl
