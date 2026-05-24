
#include "fl/channels/driver.h"
#include "fl/log/log.h"
#include "fl/stl/chrono.h"
#include "fl/task/executor.h"
#include "platforms/is_platform.h"

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

#if defined(FL_IS_ESP_32P4)
        // ESP32-P4 has no network stack (no WiFi, no Bluetooth, no lwIP),
        // so the deep-yield rationale from #2254 doesn't apply. taskYIELD()
        // avoids the 1-tick (≥1 ms at CONFIG_FREERTOS_HZ=1000) floor that
        // adds ~8 ms per frame in PARLIO multi-strip workloads (#2493).
        task::run(0, task::ExecFlags::SYSTEM);
#else
        // OS yield only — keeps WiFi/lwIP alive without pumping
        // tasks or coroutines in the driver polling loop.
        task::run(250, task::ExecFlags::SYSTEM);
#endif
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
