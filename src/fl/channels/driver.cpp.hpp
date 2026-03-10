
#include "fl/channels/driver.h"
#include "fl/log.h"
#include "fl/stl/chrono.h"
#include "fl/stl/async.h"

namespace fl {

template<typename Condition>
bool IChannelDriver::waitForCondition(Condition condition, u32 timeoutMs) {
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("Timeout occurred while waiting for condition");
            return false;  // Timeout occurred
        }

        // OS yield only — keeps WiFi/lwIP alive without pumping
        // tasks or coroutines in the driver polling loop.
        async_run(250, AsyncFlags::SYSTEM);
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
