#pragma once

/// @file fl/channels/detail/wait_yield.h
/// @brief Tiered wait helper used by ChannelManager and IChannelDriver.
///
/// Implements the Phase 1 (#2818) strategy from #2815:
///   Tier 1 — instant non-blocking condition check
///   Tier 2 — bounded microsecond spin (default FASTLED_WAIT_SPIN_BUDGET_US = 250)
///   Tier 3 — cooperator-friendly deep yield, gated on whether a radio is
///            actually active (fl::NetworkDetector on ESP32). When no radio
///            is up we skip the `vTaskDelay(>=1 tick)` floor.
///
/// The tiered structure replaces the previous unconditional 1-tick yield
/// and removes the `FL_IS_ESP_32P4` carve-out — NetworkDetector's
/// weak-symbol fallback returns "no radio" on P4 automatically.

#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/task/executor.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
// IWYU pragma: begin_keep
#include "platforms/esp/32/util/network_detector.h"  // ok platform headers
// IWYU pragma: end_keep
#endif

namespace fl {
namespace detail {

/// @brief Get the current tiered-wait spin budget (microseconds).
fl::u32 getWaitSpinBudgetUs() FL_NOEXCEPT;

/// @brief Set the tiered-wait spin budget (microseconds).
/// @note Public-facing entry point is `CFastLED::setWaitSpinBudgetUs(N)`.
void setWaitSpinBudgetUs(fl::u32 budget_us) FL_NOEXCEPT;

/// @brief Block until `condition()` returns true, or `timeoutMs` elapses.
/// @return true if condition became true; false on timeout.
///
/// Tier 1 catches the already-ready case (free fast path).
/// Tier 2 spins for up to `getWaitSpinBudgetUs()` microseconds — catches
///         DMA tails without paying the FreeRTOS tick floor.
/// Tier 3 yields to the OS each iteration. On ESP32 we check
///         `NetworkDetector::isAnyNetworkActive()` once before entering
///         the loop: when no radio is up, we use a non-tick-flooring
///         fast yield; when a radio IS up, we keep the deep yield to
///         preserve the #2254 WiFi/lwIP cooperator invariant.
template<typename Condition>
inline bool tieredWait(Condition condition, fl::u32 timeoutMs) {
    const fl::u32 start_ms = fl::millis();

    // Tier 1: instant non-blocking check.
    if (condition()) {
        return true;
    }

    // Tier 2: bounded microsecond spin.
    const fl::u32 spin_budget = getWaitSpinBudgetUs();
    if (spin_budget > 0) {
        const fl::u32 spin_start = fl::micros();
        while ((fl::micros() - spin_start) < spin_budget) {
            if (condition()) {
                return true;
            }
            if (timeoutMs > 0 && (fl::millis() - start_ms) >= timeoutMs) {
                return false;
            }
        }
    }

    // Tier 3: cooperator-friendly deep yield.
#if defined(FL_IS_ESP32)
    const bool radio_up = fl::NetworkDetector::isAnyNetworkActive();
#else
    constexpr bool radio_up = false;
#endif

    while (!condition()) {
        if (timeoutMs > 0 && (fl::millis() - start_ms) >= timeoutMs) {
            return false;
        }
        if (radio_up) {
            // Deep yield — keeps WiFi/lwIP/BT cooperator tasks alive (#2254).
            fl::task::run(250, fl::task::ExecFlags::SYSTEM);
        } else {
            // No radio active — skip the >=1-tick floor and use the
            // cheap fl::yield() path (same as the previous P4 fast path).
            fl::task::run(0, fl::task::ExecFlags::SYSTEM);
        }
    }
    return true;
}

} // namespace detail
} // namespace fl
