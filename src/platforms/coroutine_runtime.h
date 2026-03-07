#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/atomic.h"
#include "fl/engine_events.h"

namespace fl {
namespace platforms {

/// @brief Platform-specific coroutine runtime interface
///
/// Provides platform-specific event pumping, timing primitives, and shutdown
/// coordination for the coroutine system.
/// Registers as an EngineEvents::Listener to receive onExit() shutdown signal.
///
/// Platform implementations:
/// - ESP32: Uses vTaskDelay() to yield to FreeRTOS scheduler
/// - Arduino: Uses Arduino delayMicroseconds()
/// - Stub/Host: Uses coroutine runner + thread sleep
/// - WASM: Uses emscripten_thread_sleep()
class ICoroutineRuntime : public EngineEvents::Listener {
public:
    static ICoroutineRuntime& instance();

    ICoroutineRuntime() {
        EngineEvents::addListener(this);
    }

    ~ICoroutineRuntime() override {
        EngineEvents::removeListener(this);
    }

    /// Pump platform event queue with sleep
    /// @param us Microseconds to pump/sleep
    virtual void pumpEventQueueWithSleep(fl::u32 us) = 0;

    /// Check if platform has background events that need yielding time
    /// @return True if platform has background events (e.g., FreeRTOS tasks)
    virtual bool hasPlatformBackgroundEvents() = 0;

    /// Platform-specific millisecond delay
    /// @param ms Milliseconds to delay
    virtual void platformDelay(fl::u32 ms) = 0;

    /// Platform-specific microsecond delay
    /// @param us Microseconds to delay
    virtual void platformDelayMicroseconds(fl::u32 us) = 0;

    /// Platform-specific millisecond timer
    /// @return Milliseconds since system startup
    virtual fl::u32 platformMillis() = 0;

    /// Platform-specific microsecond timer
    /// @return Microseconds since system startup
    virtual fl::u32 platformMicros() = 0;

    /// OS/RTOS yield — yield CPU to other tasks/threads
    virtual void osYield() = 0;

    /// Check if shutdown has been requested
    /// Background threads should poll this and exit early when true.
    bool isShutdownRequested() { return mShutdownRequested.load(); }

    /// Request shutdown — background threads will see this via isShutdownRequested()
    void requestShutdown() { mShutdownRequested.store(true); }

    /// Reset shutdown flag — call after cleanup to allow new threads
    void resetShutdown() { mShutdownRequested.store(false); }

    /// EngineEvents::Listener callback — sets shutdown flag on engine exit
    void onExit() override { requestShutdown(); }

private:
    fl::atomic<bool> mShutdownRequested{false};
};

}  // namespace platforms
}  // namespace fl
