#pragma once

// IWYU pragma: private

/// @file rp_pio_edge_capture.h
/// @brief Host-testable run-length conversion for the RP PIO RX sampler.

#include "fl/channels/rx.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/vector.h"

namespace fl {

/// Converts duration-counter results from the RP PIO RX program into edges.
///
/// Each PIO word is the complement of a decrementing counter. The program
/// spends two PIO cycles per count, so this class owns calibration, glitch
/// filtering, skipped phases, capacity handling, and saturation without a
/// Pico SDK dependency.
class RpPioEdgeCapture {
  public:
    void reset(u32 min_signal_ns, u32 skip_signals) FL_NO_EXCEPT {
        mMinSignalNs = min_signal_ns;
        mSkipSignals = skip_signals;
        mSaturated = false;
    }

    static u32 durationNs(u64 counter_ticks, u32 pio_clock_hz) FL_NO_EXCEPT {
        if (pio_clock_hz == 0) return 0;
        constexpr u32 kMaxEdgeNs = 0x7fffffffu;
        const u64 ns = (counter_ticks * 2ull * 1000000000ull) / pio_clock_hz;
        return ns > kMaxEdgeNs ? kMaxEdgeNs : static_cast<u32>(ns);
    }

    bool appendCounter(bool high, u32 remaining_counter, u32 pio_clock_hz,
                       u32 setup_cycles, fl::vector<EdgeTime>* edges,
                       size_t capacity) FL_NO_EXCEPT {
        if (pio_clock_hz == 0) return false;
        const u64 counter_ticks = ~remaining_counter;
        const u64 total_cycles = counter_ticks * 2ull + setup_cycles;
        if ((total_cycles * 1000000000ull) / pio_clock_hz > 0x7fffffffu) {
            mSaturated = true;
        }
        const u64 ns = (total_cycles * 1000000000ull) / pio_clock_hz;
        return appendDuration(high, ns > 0x7fffffffu ? 0x7fffffffu : static_cast<u32>(ns),
                              edges, capacity);
    }

    bool appendDuration(bool high, u32 duration_ns, fl::vector<EdgeTime>* edges,
                        size_t capacity) FL_NO_EXCEPT {
        if (edges == nullptr) return false;
        if (duration_ns < mMinSignalNs) return true;
        if (mSkipSignals > 0) {
            --mSkipSignals;
            return true;
        }
        if (!edges->empty() && edges->back().high == (high ? 1u : 0u)) {
            const u64 merged = static_cast<u64>(edges->back().ns) + duration_ns;
            constexpr u32 kMaxEdgeNs = 0x7fffffffu;
            edges->back().ns = merged > kMaxEdgeNs ? kMaxEdgeNs : static_cast<u32>(merged);
            if (merged > kMaxEdgeNs) mSaturated = true;
            return true;
        }
        if (edges->size() >= capacity) return false;
        edges->push_back(EdgeTime(high, duration_ns));
        return true;
    }

    bool saturated() const FL_NO_EXCEPT { return mSaturated; }

  private:
    u32 mMinSignalNs = 0;
    u32 mSkipSignals = 0;
    bool mSaturated = false;
};

}  // namespace fl
