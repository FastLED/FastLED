/// @file fl/channels/rx/decode_ws2812.cpp.hpp
/// @brief Shared 4-phase WS2812 edge-pair -> byte decoder (definition).
///
/// See `decode_ws2812.h` for the full contract and the explicit list of
/// behaviours preserved verbatim from the original Native + LPC SCT
/// decoders that this consolidation replaces.

// IWYU pragma: private

#include "fl/channels/rx/decode_ws2812.h"

namespace fl {
namespace channels {
namespace rx {

namespace {

/// Bit from a HIGH alone, for the frame's last bit: its LOW runs into the
/// reset, so it is either a reset-length LOW or was never closed by an edge.
/// -1 when the HIGH fits neither window.
int ws2812BitFromHigh(u32 high_ns, const ChipsetTiming4Phase& timing) FL_NO_EXCEPT {
    if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
        return 1;
    }
    if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
        return 0;
    }
    return -1;
}

}  // namespace

fl::result<u32, DecodeError>
decodeWs2812Edges(const ChipsetTiming4Phase& timing,
                  fl::span<const EdgeTime> edges,
                  fl::span<u8> out) FL_NO_EXCEPT {
    const size_t edge_count = edges.size();
    if (edge_count == 0) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t bytes_written = 0;
    size_t bit_index = 0;
    u8 current_byte = 0;
    u32 error_count = 0;

    // Shifts one bit in; false when a completed byte does not fit `out`.
    auto push_bit = [&](bool one) FL_NO_EXCEPT -> bool {
        current_byte = static_cast<u8>((current_byte << 1) | (one ? 1u : 0u));
        if (++bit_index < 8) {
            return true;
        }
        if (bytes_written >= out.size()) {
            return false;
        }
        out[bytes_written++] = current_byte;
        current_byte = 0;
        bit_index = 0;
        return true;
    };
    const u32 reset_ns = static_cast<u32>(timing.reset_min_us) * 1000u;
    bool frame_ended = false;

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
            if (low_ns >= reset_ns) {
                // WS2812 reset pulse -> end of frame. The LOW is the last
                // bit's LOW run into the reset, so the bit is in the HIGH.
                const int bit = ws2812BitFromHigh(high_ns, timing);
                if (bit >= 0 && !push_bit(bit == 1)) {
                    return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
                }
                frame_ended = true;
                break;
            }
            // #3416 RX-HIGH-5: also break on a long HIGH between frames.
            // The FlexIO RX path observed sustained HIGH-state between
            // frames (#3066, FlexIO TX shifter latch retention) where
            // the line stayed high for many milliseconds. Without this
            // branch a single stray falling edge after the long HIGH
            // would misclassify a phantom bit cell of arbitrary
            // duration as a real WS2812 bit.
            if (high_ns >= reset_ns) {
                frame_ended = true;
                break;
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

        if (!push_bit(is_bit1)) {
            return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
        i += 2;
    }

    // A capture that stops after a HIGH has no edge to close the last LOW.
    if (!frame_ended && i + 1 == edge_count && edges[i].high) {
        const int bit = ws2812BitFromHigh(edges[i].ns, timing);
        if (bit >= 0 && !push_bit(bit == 1)) {
            return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
    }

    if (bytes_written > 0 && error_count * 10 > bytes_written * 8) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::result<u32, DecodeError>::success(static_cast<u32>(bytes_written));
}

}  // namespace rx
}  // namespace channels
}  // namespace fl
