#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_refill_timing.h"

namespace fl {
namespace detail {

void Rmt4RefillTiming::reset() FL_NO_EXCEPT {
    mLastFill.store(0, fl::memory_order_release);
    mFillPhase.store(0, fl::memory_order_release);
}

FL_IRAM bool Rmt4RefillTiming::allowFill(
    u32 now, u32 maxCyclesPerFill) FL_NO_EXCEPT {
    const u32 phase = mFillPhase.load(fl::memory_order_acquire);
    if (phase == 0) {
        // SDK task-context fill precedes rmt_tx_start(). A scheduling delay
        // before hardware starts is unrelated to an ISR refill deadline.
        mFillPhase.store(1, fl::memory_order_release);
        return true;
    }
    if (phase == 1) {
        // Establish the clock from the first hardware-driven refill.
        mLastFill.store(now, fl::memory_order_release);
        mFillPhase.store(2, fl::memory_order_release);
        return true;
    }
    const u32 lastFill = mLastFill.load(fl::memory_order_acquire);
    if (now - lastFill > maxCyclesPerFill) {
        return false;
    }
    mLastFill.store(now, fl::memory_order_release);
    return true;
}

} // namespace detail
} // namespace fl
