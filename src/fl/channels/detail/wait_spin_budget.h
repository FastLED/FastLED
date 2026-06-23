#pragma once

/// @file fl/channels/detail/wait_spin_budget.h
/// @brief Runtime-tunable microsecond spin budget for the channel-manager
///        and driver wait loops (Phase 1 of #2815 / #2818).
///
/// The wait sites in `fl/channels/manager.cpp.hpp` and
/// `fl/channels/driver.cpp.hpp` use a tiered structure:
///   Tier 1 - instant non-blocking condition check
///   Tier 2 - bounded microsecond spin (this budget)
///   Tier 3 - cooperator yield, NetworkDetector-gated (added by #2817)
///
/// Tier 2 catches short DMA tails (APA102 small strips, WS2812B <=8 LEDs)
/// without paying the FreeRTOS >=1 ms tick floor. Default 250 us.
/// Set to 0 to disable the spin tier entirely.
///
/// Public-facing entry points live on CFastLED:
///   FastLED.setWaitSpinBudgetUs(N)
///   FastLED.getWaitSpinBudgetUs()

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

/// @brief Get the current tiered-wait spin budget (microseconds).
fl::u32 getWaitSpinBudgetUs() FL_NOEXCEPT;

/// @brief Set the tiered-wait spin budget (microseconds).
void setWaitSpinBudgetUs(fl::u32 budget_us) FL_NOEXCEPT;

} // namespace detail
} // namespace fl
