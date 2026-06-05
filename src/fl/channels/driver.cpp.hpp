
#include "fl/channels/driver.h"
#include "fl/channels/detail/wait_yield.h"
#include "fl/log/log.h"
#include "fl/stl/chrono.h"
#include "fl/task/executor.h"
#include "platforms/is_platform.h"

namespace fl {

template<typename Condition>
bool IChannelDriver::waitForCondition(Condition condition, u32 timeoutMs) {
    // Tiered wait — shared helper with ChannelManager. See #2815/#2818.
    const bool ok = fl::detail::tieredWait(condition, timeoutMs);
    if (!ok) {
        FL_ERROR("Timeout occurred while waiting for condition");
    }
    return ok;
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
