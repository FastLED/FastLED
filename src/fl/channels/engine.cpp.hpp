
#include "fl/channels/engine.h"
#include "fl/log.h"
#include "fl/stl/chrono.h"
#include "fl/delay.h"
#include "fl/async.h"

namespace fl {

template<typename Condition>
bool IChannelEngine::waitForCondition(Condition condition, u32 timeoutMs) {
    const u32 POLL_INTERVAL_US = 100;  // Target 100µs between polls
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("Timeout occurred while waiting for condition");
            return false;  // Timeout occurred
        }

        u32 loopStart = micros();

        // Run async tasks first (allows HTTP requests, timers, etc. to process)
        async_run();

        // Calculate elapsed time
        u32 elapsed = micros() - loopStart;

        // If we have time remaining in the 100µs interval, delay for the remainder
        if (elapsed < POLL_INTERVAL_US) {
            delayMicroseconds(POLL_INTERVAL_US - elapsed);
        }
        // Otherwise skip delay and go straight to next poll iteration
    }

    return true;  // Condition met
}

bool IChannelEngine::waitForReady(u32 timeoutMs) {
    // wait until the engine is in a READY state.
    bool ok = waitForCondition([this]() {
        auto state = poll();
        return state.state == IChannelEngine::EngineState::READY;
    }, timeoutMs);
    return ok;
}

bool IChannelEngine::waitForReadyOrDraining(u32 timeoutMs) {
    bool ok = waitForCondition([this]() {
        auto state = poll();
        return state.state == IChannelEngine::EngineState::READY ||
               state.state == IChannelEngine::EngineState::DRAINING;
    }, timeoutMs);
    return ok;
}

}  // namespace fl
