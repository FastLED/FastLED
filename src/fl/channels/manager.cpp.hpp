/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/channels/manager.h"
#include "fl/channels/detail/wait_spin_budget.h"
#include "fl/stl/singleton.h"
#include "fl/log/log.h"
#include "fl/system/engine_events.h"
#include "fl/stl/chrono.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/move.h"
#include "fl/system/trace.h"
#include "fl/task/executor.h"
#include "fl/net/network_detector.h"
#include "platforms/init_channel_driver.h"
#include "platforms/is_platform.h"
#include "fl/stl/noexcept.h"

namespace fl {

ChannelManager& ChannelManager::registry() FL_NO_EXCEPT {
    return Singleton<ChannelManager>::instance();
}

ChannelManager& ChannelManager::instance() {
    auto& out = registry();
    // Lazy initialization of platform-specific channel drivers
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false; // okay static in header
    if (!sInitialized) {
        sInitialized = true;
        platforms::initChannelDrivers();
    }
    return out;
}

ChannelManager::ChannelManager() FL_NO_EXCEPT
    : mPollNeededCallback(&ChannelManager::notifyPollNeededThunk, this),
      mPollNeededSignal() {
    FL_DBG("ChannelManager: Initializing");

    // Register as frame event listener for per-frame reset
    EngineEvents::addListener(this);
}

ChannelManager::~ChannelManager() FL_NO_EXCEPT {
    FL_DBG("ChannelManager: Destructor called");

    // Remove self from EngineEvents listener list
    EngineEvents::removeListener(this);

    for (auto& entry : mDrivers) {
        if (entry.driver) {
            entry.driver->setPollNeededCallback(IChannelDriver::PollNeededCallback());
        }
    }

    // Shared drivers automatically cleaned up by shared_ptr destructors
}

void ChannelManager::notifyPollNeeded() FL_NO_EXCEPT {
    mPollNeededSignal.notify();
}

void ChannelManager::notifyPollNeededThunk(void* context) FL_NO_EXCEPT {
    if (context == nullptr) {
        return;
    }
    static_cast<ChannelManager*>(context)->notifyPollNeeded();
}

bool ChannelManager::waitForPollNeededSignal(u32 timeoutMs) FL_NO_EXCEPT {
    return mPollNeededSignal.wait(timeoutMs);
}

u32 ChannelManager::pollNeededWaitSliceMs(u32 startTime, u32 timeoutMs) const FL_NO_EXCEPT {
    constexpr u32 kPollNeededFallbackSliceMs = 1;
    if (timeoutMs == 0) {
        return kPollNeededFallbackSliceMs;
    }
    const u32 elapsed = millis() - startTime;
    if (elapsed >= timeoutMs) {
        return 0;
    }
    const u32 remaining = timeoutMs - elapsed;
    return remaining < kPollNeededFallbackSliceMs ? remaining : kPollNeededFallbackSliceMs;
}

FL_NO_INLINE FL_COLD bool ChannelManager::addDriverSlow(
    int priority,
    const fl::shared_ptr<IChannelDriver>& driver,
    const fl::string* engineName,
    AddDriverSlowReason reason) FL_NO_EXCEPT {
    if (reason == AddDriverSlowReason::NULL_DRIVER) {
        FL_WARN("ChannelManager::addDriver() - Null driver provided");
        return false;
    }
    if (reason == AddDriverSlowReason::EMPTY_NAME) {
        FL_WARN("ChannelManager::addDriver() - Engine has empty name (driver->getName() returned empty string)");
        return false;
    }

    for (const auto& entry : mDrivers) {
        if (entry.name == *engineName) {
            // True-duplicate fast path: same shared_ptr at same priority is
            // a no-op (legacy clockless controllers may pre-bind the same
            // driver singleton from many template instantiations). Skip the
            // replace flow entirely so we don't waitForReady() or emit a
            // spurious "Replacing" warning.
            if (entry.driver == driver && entry.priority == priority) {
                FL_DBG("ChannelManager::addDriver() - '" << engineName->c_str() << "' already registered at priority " << priority << " (idempotent no-op)");
                return false;
            }
            FL_WARN("ChannelManager::addDriver() - Replacing existing driver '" << engineName->c_str() << "'");

            FL_DBG("ChannelManager: Waiting for all drivers to become READY before replacement");
            waitForReady();

            // Re-scan after waiting: task pumping in waitForReady() can run
            // callbacks, so do not retain an index or iterator across it.
            for (size_t i = 0; i < mDrivers.size(); ++i) {
                if (mDrivers[i].name != *engineName) {
                    continue;
                }
                FL_DBG("ChannelManager: Removing old driver '" << engineName->c_str() << "' (shared_ptr may delete)");
                if (mDrivers[i].driver) {
                    mDrivers[i].driver->setPollNeededCallback(IChannelDriver::PollNeededCallback());
                }
                mDrivers.erase(mDrivers.begin() + i);
                break;
            }
            return true;
        }
    }
    return true;
}

void ChannelManager::addDriver(int priority, fl::shared_ptr<IChannelDriver> driver) {
    if (!driver) {
        (void)addDriverSlow(priority, driver, nullptr, AddDriverSlowReason::NULL_DRIVER);
        return;
    }

    // Get driver name from the driver itself
    fl::string engineName = driver->getName();

    // Reject drivers with empty names
    if (engineName.empty()) {
        (void)addDriverSlow(priority, driver, &engineName, AddDriverSlowReason::EMPTY_NAME);
        return;
    }

    // A duplicate name takes the uncommon replacement/idempotency path.
    for (const auto& entry : mDrivers) {
        if (entry.name == engineName) {
            if (!addDriverSlow(priority, driver, &engineName, AddDriverSlowReason::DUPLICATE_NAME)) {
                return;
            }
            break;
        }
    }

    // Respect exclusive driver mode: auto-disable if name doesn't match exclusive driver
    bool enabled = true;  // Default: enabled
    if (!mExclusiveDriver.empty()) {
        enabled = (engineName == mExclusiveDriver);  // Only enable if matches exclusive driver
    }

#if FASTLED_HAS_DBG
    const fl::string debugEngineName = engineName;
#endif
    mDrivers.push_back({priority, driver, fl::move(engineName), enabled});
    driver->setPollNeededCallback(mPollNeededCallback);

    // Build capability string for debug output. Gate the entire block behind
    // FASTLED_HAS_DBG because the `capStr` exists ONLY to feed the FL_DBG
    // line below. On release builds (FASTLED_HAS_DBG=0 — i.e. the default
    // SKETCH_HAS_LARGE_MEMORY=0 path AND any -DFASTLED_LOG_VERBOSITY=0
    // opt-in build via the gating in fl/log/log.h) the FL_DBG itself is a
    // no-op, but without this guard the `fl::string capStr` allocation +
    // two `if` branches still emitted code. See #2773 item 2.3 follow-up.
#if FASTLED_HAS_DBG
    IChannelDriver::Capabilities caps = driver->getCapabilities();
    fl::string capStr;
    if (caps.supportsClockless) {
        capStr += "CLOCKLESS";
    }
    if (caps.supportsSpi) {
        if (!capStr.empty()) capStr += "|";
        capStr += "SPI";
    }
    if (capStr.empty()) {
        capStr = "NONE";
    }

    FL_DBG("ChannelManager: Added driver '" << debugEngineName.c_str() << "' (priority " << priority << ", caps: " << capStr.c_str() << ")");
#endif

    // Sort drivers by priority descending (higher values first) after each insertion
    // Higher priority values = higher precedence (e.g., priority 50 selected over priority 10)
    // Only 1-4 drivers expected — sort_small skips the quicksort_impl
    // instantiation entirely (see #2907 for the bloat motivation).
    fl::sort_small(mDrivers.begin(), mDrivers.end());
}

void ChannelManager::clearAllDrivers() {
    FL_DBG("ChannelManager: Waiting for all drivers to become READY before clearing");

    // Wait for all drivers to become READY before clearing
    // This prevents clearing drivers that are still transmitting
    waitForReady();

    FL_DBG("ChannelManager: Clearing " << mDrivers.size() << " drivers");

    for (auto& entry : mDrivers) {
        if (entry.driver) {
            entry.driver->setPollNeededCallback(IChannelDriver::PollNeededCallback());
        }
    }

    // Clear all drivers (shared_ptr handles cleanup automatically)
    mDrivers.clear();

    // Drop the exclusive-driver filter along with the registry it refers to.
    // Leaving it set would name a driver that no longer exists, and
    // addDriver() consults it -- so every driver registered afterwards would
    // arrive silently disabled, with no diagnostic and nothing to clear it.
    mExclusiveDriver.clear();
}

bool ChannelManager::isDriverEnabled(const char* name) const {
    if (!name) {
        FL_ERROR("ChannelManager::isDriverEnabled() - Null driver name provided");
        return false;
    }

    for (const auto& entry : mDrivers) {
        if (entry.name == name) {
            return entry.enabled;
        }
    }

    FL_ERROR("ChannelManager::isDriverEnabled() - Driver '" << name << "' not found in registry");
    return false;
}

bool ChannelManager::waitForState(bool allowDraining, u32 timeoutMs) FL_NO_EXCEPT {
    const auto condition = [this, allowDraining]() {
        const auto state = poll().state;
        return state == IChannelDriver::DriverState::READY ||
               (allowDraining && state == IChannelDriver::DriverState::DRAINING);
    };
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    // Tier 1: instant non-blocking check (avoid micros() / millis() cost on
    // the common already-ready path).
    if (condition()) {
        return true;
    }

    // Tier 2: bounded microsecond spin (#2818). Catches short DMA tails
    // (APA102 small strips, WS2812B <=8 LEDs) without paying the >=1-tick
    // floor of the cooperator yield below. Budget is runtime-tunable via
    // FastLED.setWaitSpinBudgetUs(N); set to 0 to disable.
    {
        const u32 spinBudget = fl::detail::getWaitSpinBudgetUs();
        if (spinBudget > 0) {
            const u32 spinStart = fl::micros();
            while ((fl::micros() - spinStart) < spinBudget) {
                if (condition()) {
                    return true;
                }
                if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs) {
                    FL_ERROR("ChannelManager: Timeout occurred while waiting for condition");
                    return false;
                }
            }
        }
    }

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("ChannelManager: Timeout occurred while waiting for condition");
            return false;  // Timeout occurred
        }

        const u32 sliceMs = pollNeededWaitSliceMs(startTime, timeoutMs);
        if (sliceMs == 0) {
            return false;
        }
        if (waitForPollNeededSignal(sliceMs)) {
            continue;
        }

        // Adaptive yield (refs #2815, generalizes the #2493 ESP32-P4 carve-out):
        //
        // The 1-tick (>=1 ms at CONFIG_FREERTOS_HZ=1000) floor only exists to
        // keep WiFi / lwIP / BT controller tasks alive while we are inside the
        // channel wait loop (#2254). When no radio is actually up, that floor
        // is pure timing drift -- visible as the regression reported in #2420
        // and as the per-frame cost the #2493 ESP32-P4 carve-out was avoiding.
        //
        // NetworkDetector::isAnyNetworkActive() is the runtime version of the
        // "is a radio up?" question. On non-ESP32 platforms and on ESP32-P4
        // (no radio silicon) it folds to a constant `false`, so this is
        // strictly a perf win for the common single-strip / no-WiFi case
        // without losing the WiFi-friendly behavior when a radio is active.
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

IChannelDriver::DriverState ChannelManager::poll() {
    // Poll all registered drivers and return aggregate state
    // Priority order: ERROR > BUSY > DRAINING > READY
    bool anyBusy = false;
    bool anyDraining = false;
    IChannelDriver::DriverState aggregate(IChannelDriver::DriverState::READY);

    for (auto& entry : mDrivers) {
        IChannelDriver::DriverState result = entry.driver->poll();
        if (result.state == IChannelDriver::DriverState::BUSY) {
            anyBusy = true;
        } else if (result.state == IChannelDriver::DriverState::DRAINING) {
            anyDraining = true;
        }
        // Capture first error encountered
        if (result.state == IChannelDriver::DriverState::ERROR && aggregate.error.empty()) {
            aggregate.error = fl::move(result.error);
        }
    }

    if (!aggregate.error.empty()) {
        aggregate.state = IChannelDriver::DriverState::ERROR;
    } else if (anyBusy) {
        aggregate.state = IChannelDriver::DriverState::BUSY;
    } else if (anyDraining) {
        aggregate.state = IChannelDriver::DriverState::DRAINING;
    }
    return aggregate;
}

bool ChannelManager::waitForReady(u32 timeoutMs) {
    bool ok = waitForState(false, timeoutMs);
    if (!ok) {
        FL_ERROR("ChannelManager: Timeout occurred while waiting for READY state");
    }
    return ok;
}

bool ChannelManager::waitForReadyOrDraining(u32 timeoutMs) {
    bool ok = waitForState(true, timeoutMs);
    if (!ok) {
        FL_ERROR("ChannelManager: Timeout occurred while waiting for READY or DRAINING state");
    }
    return ok;
}

void ChannelManager::onBeginFrame() {
    waitForReady();  // Wait for all drivers to become READY before clearing previous frame state.
}

void ChannelManager::onEndFrame() {
    // Call show() on all drivers to trigger transmission
    // Channels have enqueued data directly to drivers during showPixels()
    // Now we trigger transmission by calling show() on each driver
    for (auto& entry : mDrivers) {
        if (entry.enabled) {
            entry.driver->show();
        }
    }
    waitForReadyOrDraining();
}

void ChannelManager::reset() {
    // Allow all channel drivers to clean up
    waitForReady();
    FL_DBG("ChannelManager: reset() - all drivers ready");
}

ChannelManager& channelManager() {
    return ChannelManager::instance();
}

} // namespace fl
