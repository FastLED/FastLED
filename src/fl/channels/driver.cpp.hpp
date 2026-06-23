
#include "fl/channels/driver.h"
#include "fl/channels/detail/wait_spin_budget.h"
#include "fl/log/log.h"
#include "fl/net/network_detector.h"
#include "fl/stl/chrono.h"
#include "fl/task/executor.h"
#include "platforms/is_platform.h"

namespace fl {

template<typename Condition>
bool IChannelDriver::waitForCondition(Condition condition, u32 timeoutMs) {
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    // Tier 1: instant non-blocking check.
    if (condition()) {
        return true;
    }

    // Tier 2: bounded microsecond spin (#2818). Catches short DMA tails
    // without paying the >=1-tick floor below. Budget runtime-tunable via
    // FastLED.setWaitSpinBudgetUs(N); 0 disables.
    {
        const u32 spinBudget = fl::detail::getWaitSpinBudgetUs();
        if (spinBudget > 0) {
            const u32 spinStart = fl::micros();
            while ((fl::micros() - spinStart) < spinBudget) {
                if (condition()) {
                    return true;
                }
                if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs) {
                    FL_ERROR("Timeout occurred while waiting for condition");
                    return false;
                }
            }
        }
    }

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("Timeout occurred while waiting for condition");
            return false;  // Timeout occurred
        }

        // Adaptive yield (refs #2815, generalizes the #2493 ESP32-P4 carve-out):
        // see the matching comment in ChannelManager::waitForCondition for the
        // full rationale. The deep yield is only needed when a radio is
        // actually up; otherwise the FreeRTOS tick floor is pure timing drift.
        if (fl::NetworkDetector::isAnyNetworkActive()) {
            // Radio active: keep WiFi/lwIP/BT alive with the deep yield.
            task::run(250, task::ExecFlags::SYSTEM);
        } else {
            // No radio: fast yield, no FreeRTOS tick floor.
            task::run(0, task::ExecFlags::SYSTEM);
        }
    }

    return true;  // Condition met
}

bool IChannelDriver::waitForReady(u32 timeoutMs) {
    // wait until the driver is in a READY state.
    bool ok = waitForCondition([this]() {
        auto state = poll();
        return state.state == IChannelDriver::DriverState::READY;
    }, timeoutMs);
    return ok;
}

bool IChannelDriver::waitForReadyOrDraining(u32 timeoutMs) {
    // wait until the driver is in a READY or DRAINING state.
    bool ok = waitForCondition([this]() {
        auto state = poll();
        return state.state == IChannelDriver::DriverState::READY || state.state == IChannelDriver::DriverState::DRAINING;
    }, timeoutMs);
    return ok;
}

}  // namespace fl
