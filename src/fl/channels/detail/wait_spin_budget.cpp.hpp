// IWYU pragma: private

/// @file fl/channels/detail/wait_spin_budget.cpp.hpp
/// @brief Storage and accessors for the tiered-wait spin budget (#2818).

#include "fl/channels/detail/wait_spin_budget.h"
#include "fl/stl/atomic.h"

#ifndef FASTLED_WAIT_SPIN_BUDGET_US
/// @brief Default microsecond spin budget for the channel wait loops.
///
/// Sized to catch short DMA tails (APA102 small strips, WS2812B <=8 LEDs)
/// while keeping spin overhead small: 250 us x 60 FPS = ~15 ms/s on the
/// LED-pinned core.
#define FASTLED_WAIT_SPIN_BUDGET_US 250
#endif

namespace fl {
namespace detail {

namespace {
    // Atomic so readers in waitForCondition do not race against a writer
    // in CFastLED::setWaitSpinBudgetUs. On bare-metal (no real atomics)
    // fl::atomic collapses to a plain integer, which is fine because
    // non-FreeRTOS targets only access from one thread context.
    fl::atomic<fl::u32> sWaitSpinBudgetUs{FASTLED_WAIT_SPIN_BUDGET_US};
}

fl::u32 getWaitSpinBudgetUs() FL_NOEXCEPT {
    return sWaitSpinBudgetUs.load();
}

void setWaitSpinBudgetUs(fl::u32 budget_us) FL_NOEXCEPT {
    sWaitSpinBudgetUs.store(budget_us);
}

} // namespace detail
} // namespace fl
