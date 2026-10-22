#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_4/rmt_rx_decode_4.h"

namespace fl {
namespace detail {

fl::result<u32, DecodeError> decodeRmt4Edges(
    const ChipsetTiming4Phase &timing, fl::span<const EdgeTime> edges,
    fl::span<u8> out) FL_NO_EXCEPT {
    if (edges.empty() || out.empty()) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    const auto inRange = [](u32 value, u32 minimum, u32 maximum) FL_NO_EXCEPT {
        return value >= minimum && value <= maximum;
    };
    const auto highBit = [&](u32 high) FL_NO_EXCEPT {
        if (inRange(high, timing.t1h_min_ns, timing.t1h_max_ns)) {
            return 1;
        }
        if (inRange(high, timing.t0h_min_ns, timing.t0h_max_ns)) {
            return 0;
        }
        return -1;
    };
    u32 written = 0;
    u8 byte = 0;
    u8 bits = 0;
    const auto appendBit = [&](int bit) FL_NO_EXCEPT {
        if (written >= out.size()) {
            return false;
        }
        byte = static_cast<u8>((byte << 1) | bit);
        if (++bits == 8) {
            out[written++] = byte;
            byte = 0;
            bits = 0;
        }
        return true;
    };
    bool started = false;
    bool pending = false;
    u32 high = 0;
    const u64 resetNs = static_cast<u64>(timing.reset_min_us) * 1000;
    for (const auto &edge : edges) {
        // A zero duration terminates an SDK packet. It is not a physical edge.
        if (edge.ns == 0) {
            break;
        }
        if (edge.high) {
            if (pending) {
                return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
            }
            high = edge.ns;
            pending = true;
            started = true;
            continue;
        }
        if (!pending) {
            if (!started) {
                continue; // RX can begin anywhere in the leading LOW idle.
            }
            if (edge.ns >= resetNs) {
                break;
            }
            return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
        }
        int bit = -1;
        const bool zeroHigh = inRange(high, timing.t0h_min_ns, timing.t0h_max_ns);
        const bool oneHigh = inRange(high, timing.t1h_min_ns, timing.t1h_max_ns);
        if (zeroHigh && inRange(edge.ns, timing.t0l_min_ns, timing.t0l_max_ns)) {
            bit = 0;
        } else if (oneHigh && inRange(edge.ns, timing.t1l_min_ns, timing.t1l_max_ns)) {
            bit = 1;
        }
        const bool gap = timing.gap_tolerance_ns != 0 &&
                         edge.ns > timing.t0l_max_ns &&
                         edge.ns <= timing.gap_tolerance_ns;
        const bool reset = edge.ns >= resetNs;
        if (bit < 0 && (gap || reset)) {
            bit = highBit(high);
        }
        if (bit < 0) {
            return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
        }
        if (!appendBit(bit)) {
            return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
        pending = false;
        if (reset && !gap) {
            break;
        }
    }
    if (pending) {
        const int bit = highBit(high);
        if (bit < 0) {
            return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
        }
        if (!appendBit(bit)) {
            return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
    }
    return fl::result<u32, DecodeError>::success(written);
}


} // namespace detail
} // namespace fl
