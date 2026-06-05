// IWYU pragma: private

/// @file fl/channels/detail/wait_yield.cpp.hpp
/// @brief Storage and accessors for the tiered-wait spin budget.

#include "fl/channels/detail/wait_yield.h"

#ifndef FASTLED_WAIT_SPIN_BUDGET_US
/// @brief Default microsecond spin budget for the tiered wait loop.
///
/// Sized to be cheap on a busy core (250 µs × 60 FPS = ~15 ms/s lost to spin
/// on the LED-pinned core) while still catching short DMA tails. After
/// Phase 2 of #2815 lands, the single-driver wait bypasses the spin tier
/// entirely via `IChannelDriver::waitDone()`; the spin only services the
/// multi-driver aggregate fallback path.
#define FASTLED_WAIT_SPIN_BUDGET_US 250
#endif

namespace fl {
namespace detail {

namespace {
    // Mutable storage for the runtime override. Single-writer (set via
    // CFastLED::setWaitSpinBudgetUs), readers are the wait sites — racy
    // updates are fine: stale reads just produce a slightly-wrong spin
    // budget for one wait, not a correctness issue.
    fl::u32 sWaitSpinBudgetUs = FASTLED_WAIT_SPIN_BUDGET_US;
}

fl::u32 getWaitSpinBudgetUs() FL_NOEXCEPT {
    return sWaitSpinBudgetUs;
}

void setWaitSpinBudgetUs(fl::u32 budget_us) FL_NOEXCEPT {
    sWaitSpinBudgetUs = budget_us;
}

} // namespace detail
} // namespace fl
