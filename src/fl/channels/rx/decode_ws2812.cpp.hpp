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

fl::result<u32, DecodeError>
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
                break;  // WS2812 reset pulse -> end of frame
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
