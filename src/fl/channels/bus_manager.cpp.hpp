/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/channels/bus_manager.h"
#include "fl/singleton.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/engine_events.h"
#include "fl/delay.h"
#include "fl/stl/chrono.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/move.h"
#include "fl/trace.h"
#include "fl/async.h"
#include "platforms/init_channel_engine.h"

namespace fl {

ChannelBusManager& ChannelBusManager::instance() {
    auto& out = Singleton<ChannelBusManager>::instance();
    // Lazy initialization of platform-specific channel engines
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false; // okay static in header
    if (!sInitialized) {
        sInitialized = true;
        platforms::initChannelEngines();
    }
    return out;
}

ChannelBusManager::ChannelBusManager() {
    FL_DBG("ChannelBusManager: Initializing");

    // Register as frame event listener for per-frame reset
    EngineEvents::addListener(this);
}

ChannelBusManager::~ChannelBusManager() {
    FL_DBG("ChannelBusManager: Destructor called");

    // Remove self from EngineEvents listener list
    EngineEvents::removeListener(this);

    // Shared engines automatically cleaned up by shared_ptr destructors
}

void ChannelBusManager::addEngine(int priority, fl::shared_ptr<IChannelEngine> engine) {
    if (!engine) {
        FL_WARN("ChannelBusManager::addEngine() - Null engine provided");
        return;
    }

    // Get engine name from the engine itself
    fl::string engineName = engine->getName();

    // Reject engines with empty names
    if (engineName.empty()) {
        FL_WARN("ChannelBusManager::addEngine() - Engine has empty name (engine->getName() returned empty string)");
        return;
    }

    // Check if engine with this name already exists
    bool replacing = false;
    for (const auto& entry : mEngines) {
        if (entry.name == engineName) {
            replacing = true;
            FL_WARN("ChannelBusManager::addEngine() - Replacing existing engine '" << engineName.c_str() << "'");
            break;
        }
    }

    // If replacing, wait for all engines to become READY (2s timeout for stalled engines)
    if (replacing) {
        FL_DBG("ChannelBusManager: Waiting for all engines to become READY before replacement");
        waitForReady(2000);

        // Remove the old engine with matching name (shared_ptr may trigger deletion)
        for (size_t i = 0; i < mEngines.size(); ++i) {
            if (mEngines[i].name == engineName) {
                FL_DBG("ChannelBusManager: Removing old engine '" << engineName.c_str() << "' (shared_ptr may delete)");
                mEngines.erase(mEngines.begin() + i);
                break;
            }
        }
    }

    // Respect exclusive driver mode: auto-disable if name doesn't match exclusive driver
    bool enabled = true;  // Default: enabled
    if (!mExclusiveDriver.empty()) {
        enabled = (engineName == mExclusiveDriver);  // Only enable if matches exclusive driver
    }

    mEngines.push_back({priority, engine, engineName, enabled});

    // Build capability string for debug output
    IChannelEngine::Capabilities caps = engine->getCapabilities();
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

    FL_DBG("ChannelBusManager: Added engine '" << engineName.c_str() << "' (priority " << priority << ", caps: " << capStr.c_str() << ")");

    // Sort engines by priority descending (higher values first) after each insertion
    // Higher priority values = higher precedence (e.g., priority 50 selected over priority 10)
    // Only 1-4 engines expected, so sorting on insert is negligible
    fl::sort(mEngines.begin(), mEngines.end());
}

bool ChannelBusManager::removeEngine(fl::shared_ptr<IChannelEngine> engine) {
    if (!engine) {
        FL_WARN("ChannelBusManager::removeEngine() - Null engine provided");
        return false;
    }

    // Find and remove the engine from the list
    for (size_t i = 0; i < mEngines.size(); ++i) {
        if (mEngines[i].engine == engine) {
            FL_DBG("ChannelBusManager: Removing engine '" << mEngines[i].name << "'");

            // Remove using vector::erase (preserves sort order)
            mEngines.erase(mEngines.begin() + i);
            return true;  // Engine found and removed
        }
    }

    // Engine not found
    FL_WARN("ChannelBusManager::removeEngine() - Engine " << engine.get() << " not found in registry");
    return false;
}

void ChannelBusManager::clearAllEngines() {
    FL_DBG("ChannelBusManager: Waiting for all engines to become READY before clearing");

    // Wait for all engines to become READY before clearing, with 2s timeout
    // to prevent infinite hang on permanently stalled engines
    waitForReady(2000);

    FL_DBG("ChannelBusManager: Clearing " << mEngines.size() << " engines");

    // Clear all engines (shared_ptr handles cleanup automatically)
    mEngines.clear();
}

void ChannelBusManager::setDriverEnabled(const char* name, bool enabled) {
    if (!name) {
        FL_ERROR("ChannelBusManager::setDriverEnabled() - Null driver name provided");
        return;
    }

    bool found = false;
    for (auto& entry : mEngines) {
        if (entry.name == name) {
            entry.enabled = enabled;
            found = true;
            FL_DBG("ChannelBusManager: Driver '" << name << "' " << (enabled ? "enabled" : "disabled"));
        }
    }

    if (!found) {
        FL_ERROR("ChannelBusManager::setDriverEnabled() - Driver '" << name << "' not found in registry");
    }
}

bool ChannelBusManager::setExclusiveDriver(const char* name) {
    // Handle null or empty name
    if (!name || !name[0]) {
        FL_ERROR("ChannelBusManager::setExclusiveDriver() - Null or empty driver name provided");
        mExclusiveDriver.clear();
        // Disable all engines
        for (auto& entry : mEngines) {
            entry.enabled = false;
        }
        return false;
    }

    // Store exclusive driver name for forward compatibility
    // When non-empty, addEngine() will auto-disable non-matching engines
    mExclusiveDriver = name;

    // Single-pass: enable only engines matching the given name
    bool found = false;
    for (auto& entry : mEngines) {
        entry.enabled = (entry.name == name);
        found = found || entry.enabled;
    }

    if (!found) {
        FL_ERROR("ChannelBusManager::setExclusiveDriver() - Driver '" << name << "' not found in registry");
    }

    return found;
}

bool ChannelBusManager::setDriverPriority(const fl::string& name, int priority) {
    if (name.empty()) {
        FL_ERROR("ChannelBusManager::setDriverPriority() - Empty driver name provided");
        return false;
    }

    // Find engine and update priority
    bool found = false;
    for (auto& entry : mEngines) {
        if (entry.name == name) {
            entry.priority = priority;
            found = true;
            FL_DBG("ChannelBusManager: Driver '" << name << "' priority changed to " << priority);
            break;
        }
    }

    if (!found) {
        FL_ERROR("ChannelBusManager::setDriverPriority() - Driver '" << name << "' not found in registry");
        return false;
    }

    // Re-sort engines by priority (descending: higher values first)
    fl::sort(mEngines.begin(), mEngines.end());

    FL_DBG("ChannelBusManager: Engine list re-sorted after priority change");
    return true;
}

bool ChannelBusManager::isDriverEnabled(const char* name) const {
    if (!name) {
        FL_ERROR("ChannelBusManager::isDriverEnabled() - Null driver name provided");
        return false;
    }

    for (const auto& entry : mEngines) {
        if (entry.name == name) {
            return entry.enabled;
        }
    }

    FL_ERROR("ChannelBusManager::isDriverEnabled() - Driver '" << name << "' not found in registry");
    return false;
}

fl::size ChannelBusManager::getDriverCount() const {
    return mEngines.size();
}

fl::span<const DriverInfo> ChannelBusManager::getDriverInfos() const {
    // Update cache with current engine state
    mCachedDriverInfo.clear();
    mCachedDriverInfo.reserve(mEngines.size());

    for (const auto& entry : mEngines) {
        // fl::string copy is cheap (shared pointer internally, no heap allocation)
        mCachedDriverInfo.push_back({
            entry.name,
            entry.priority,
            entry.enabled
        });
    }

    return fl::span<const DriverInfo>(mCachedDriverInfo.data(), mCachedDriverInfo.size());
}

fl::shared_ptr<IChannelEngine> ChannelBusManager::getEngineByName(const fl::string& name) const {
    if (name.empty()) {
        FL_ERROR("ChannelBusManager::getEngineByName() - Empty driver name provided");
        return fl::shared_ptr<IChannelEngine>();
    }

    for (const auto& entry : mEngines) {
        if (entry.enabled && entry.name == name) {
            return entry.engine;  // Return shared_ptr directly
        }
    }

    FL_ERROR("ChannelBusManager::getEngineByName() - Driver '" << name.c_str() << "' not found or not enabled");
    return fl::shared_ptr<IChannelEngine>();
}

fl::shared_ptr<IChannelEngine> ChannelBusManager::selectEngineForChannel(const ChannelDataPtr& data, const fl::string& affinity) {
    if (!data) {
        FL_ERROR("ChannelBusManager::selectEngineForChannel() - Null channel data");
        return fl::shared_ptr<IChannelEngine>();
    }

    // If affinity is specified, look up by name    
    do {
        if (affinity.empty()) {
            break;
        }
        auto engine = getEngineByName(affinity);
        if (!engine) {
            FL_ERROR("ChannelBusManager: Affinity engine '" << affinity << "' not found");
            break;
        }
        if (!engine->canHandle(data)) {
            FL_ERROR("ChannelBusManager: Affinity engine '" << affinity << "' cannot handle channel data");
            break;
        }
        return engine;
    } while (false);
    

    // No affinity: iterate engines by priority (already sorted descending)
    for (const auto& entry : mEngines) {
        if (!entry.enabled) continue;
        if (entry.engine->canHandle(data)) {
            return entry.engine;  // Return shared_ptr
        }
    }

    FL_ERROR("ChannelBusManager: No compatible engine found for channel data");
    return fl::shared_ptr<IChannelEngine>();
}


template<typename Condition>
bool ChannelBusManager::waitForCondition(Condition condition, u32 timeoutMs) {
    const u32 POLL_INTERVAL_US = 100;  // Target 100µs between polls
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("ChannelBusManager: Timeout occurred while waiting for condition");
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

IChannelEngine::EngineState ChannelBusManager::poll() {
    // Poll all registered engines and return aggregate state
    // Return "worst" state: ERROR > BUSY > DRAINING > READY
    bool anyBusy = false;
    bool anyDraining = false;
    fl::string firstError;

    for (auto& entry : mEngines) {
        IChannelEngine::EngineState result = entry.engine->poll();
        if (result.state == IChannelEngine::EngineState::BUSY) {
            anyBusy = true;
        } else if (result.state == IChannelEngine::EngineState::DRAINING) {
            anyDraining = true;
        }
        // Capture first error encountered
        if (result.state == IChannelEngine::EngineState::ERROR && firstError.empty()) {
            firstError = result.error;
        }
    }

    // Return error if any engine reported error
    if (!firstError.empty()) {
        return IChannelEngine::EngineState(IChannelEngine::EngineState::ERROR, firstError);
    }

    return IChannelEngine::EngineState(
        anyBusy ? IChannelEngine::EngineState::BUSY
                : anyDraining ? IChannelEngine::EngineState::DRAINING
                              : IChannelEngine::EngineState::READY);
}

bool ChannelBusManager::waitForReady(u32 timeoutMs) {
    bool ok = waitForCondition([this]() {
        return poll().state == IChannelEngine::EngineState::READY;
    }, timeoutMs);
    if (!ok) {
        FL_ERROR("ChannelBusManager: Timeout occurred while waiting for READY state");
    }
    return ok;
}

bool ChannelBusManager::waitForReadyOrDraining(u32 timeoutMs) {
    bool ok = waitForCondition([this]() {
        auto state = poll();
        return state.state == IChannelEngine::EngineState::READY ||
               state.state == IChannelEngine::EngineState::DRAINING;
    }, timeoutMs);
    if (!ok) {
        FL_ERROR("ChannelBusManager: Timeout occurred while waiting for READY or DRAINING state");
    }
    return ok;
}

void ChannelBusManager::onBeginFrame() {
    // Wait for previous frame to finish before starting a new one.
    // 2s timeout: a frame should never take 2s; prevents infinite hang on stalled engine
    // (e.g. PARLIO on ESP32-C6 where DMA ISR never fires)
    if (!waitForReady(2000)) {
        FL_WARN("ChannelBusManager: onBeginFrame() timeout - engine may be stalled");
    }
}

void ChannelBusManager::onEndFrame() {
    // Call show() on all engines to trigger transmission
    // Channels have enqueued data directly to engines during showPixels()
    // Now we trigger transmission by calling show() on each engine
    for (auto& entry : mEngines) {
        if (entry.enabled) {
            entry.engine->show();
        }
    }
    // Wait for engines to reach DRAINING (DMA running) or READY.
    // We don't need to block until DMA completes here — onBeginFrame()
    // will wait for READY before the next frame starts.
    // 2s timeout: prevents infinite hang if engine never transitions from BUSY
    // (e.g. PARLIO on ESP32-C6 where DMA ISR never fires)
    if (!waitForReadyOrDraining(2000)) {
        FL_WARN("ChannelBusManager: onEndFrame() timeout - engine may be stalled");
    }
}

void ChannelBusManager::reset() {
    // Allow all channel engines to clean up
    // 2s timeout: prevents infinite hang if engine is stalled
    if (!waitForReady(2000)) {
        FL_WARN("ChannelBusManager: reset() timeout - engine may be stalled");
    }
    FL_DBG("ChannelBusManager: reset() - all engines ready");
}

ChannelBusManager& channelBusManager() {
    return ChannelBusManager::instance();
}

} // namespace fl
