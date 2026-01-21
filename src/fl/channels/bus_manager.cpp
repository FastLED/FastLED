/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/channels/bus_manager.h"
#include "fl/singleton.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/engine_events.h"
#include "fl/delay.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/move.h"
#include "fl/trace.h"
#include "platforms/init_channel_engine.h"

namespace fl {

ChannelBusManager& ChannelBusManager::instance() {
    auto& out = Singleton<ChannelBusManager>::instance();
    // Lazy initialization of platform-specific channel engines
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (!sInitialized) {
        sInitialized = true;
        platform::initChannelEngines();
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

void ChannelBusManager::addEngine(int priority, fl::shared_ptr<IChannelEngine> engine, const char* name) {
    if (!engine) {
        FL_WARN("ChannelBusManager::addEngine() - Null engine provided");
        return;
    }

    fl::string engineName = name ? name : "";

    // Respect exclusive driver mode: auto-disable if name doesn't match exclusive driver
    bool enabled = true;  // Default: enabled
    if (!mExclusiveDriver.empty()) {
        enabled = (engineName == mExclusiveDriver);  // Only enable if matches exclusive driver
    }

    mEngines.push_back({priority, engine, engineName, enabled});

    if (!engineName.empty()) {
        FL_DBG("ChannelBusManager: Added engine '" << engineName.c_str() << "' (priority " << priority << ")");
    } else {
        FL_DBG("ChannelBusManager: Added unnamed engine (priority " << priority << ")");
    }

    // Sort engines by priority descending (higher values first) after each insertion
    // Higher priority values = higher precedence (e.g., priority 50 selected over priority 10)
    // Only 1-4 engines expected, so sorting on insert is negligible
    fl::sort(mEngines.begin(), mEngines.end());
}

void ChannelBusManager::setDriverEnabled(const char* name, bool enabled) {
    if (!name) {
        return;  // Null name = no-op
    }

    bool found = false;
    for (auto& entry : mEngines) {
        if (entry.name == name) {
            entry.enabled = enabled;
            found = true;
            FL_DBG("ChannelBusManager: Driver '" << name << "' " << (enabled ? "enabled" : "disabled"));
        }
    }

    if (found) {
        // Reset active engine to force re-selection on next enqueue
        // This allows the change to take effect immediately
        mActiveEngine = nullptr;
        mActiveEnginePriority = -1;
    }
}

bool ChannelBusManager::setExclusiveDriver(const char* name) {
    // Store exclusive driver name for forward compatibility
    // When non-empty, addEngine() will auto-disable non-matching engines
    mExclusiveDriver = (name && name[0]) ? name : "";

    // Single-pass: enable only engines matching the given name
    // Handles nullptr, empty string, not found, and found cases naturally
    bool found = false;
    for (auto& entry : mEngines) {
        entry.enabled = (name && name[0] && entry.name == name);
        found = found || entry.enabled;
    }

    // Reset active engine to force re-selection on next enqueue
    mActiveEngine = nullptr;
    mActiveEnginePriority = -1;

    return found;
}

bool ChannelBusManager::isDriverEnabled(const char* name) const {
    if (!name) {
        return false;  // Null name = not enabled
    }

    for (const auto& entry : mEngines) {
        if (entry.name == name) {
            return entry.enabled;
        }
    }
    return false;  // Not found = not enabled
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

IChannelEngine* ChannelBusManager::getEngineByName(const char* name) const {
    if (!name || !name[0]) {
        return nullptr;  // Null or empty name = not found
    }

    for (const auto& entry : mEngines) {
        if (entry.enabled && entry.name == name) {
            return entry.engine.get();
        }
    }
    return nullptr;  // Not found or not enabled
}

// IChannelEngine interface implementation
void ChannelBusManager::enqueue(ChannelDataPtr channelData) {
    // Select engine on first call if not already selected
    if (!mActiveEngine) {
        mActiveEngine = selectEngine();
        if (!mActiveEngine) {
            FL_WARN("ChannelBusManager::enqueue() - No engines available");
            mLastError = "No engines available for channel data";
            return;
        }
    }

    // Batch the channel data for later transmission
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelBusManager::show() {
    FL_SCOPED_TRACE;
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission
        beginTransmission(fl::span<const ChannelDataPtr>(mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelBusManager::poll() {
    FL_SCOPED_TRACE;
    // Poll all registered engines to allow buffer cleanup
    // even when mActiveEngine is nullptr (after onEndFrame reset)
    bool anyBusy = false;
    for (auto& entry : mEngines) {
        if (entry.engine->poll() == EngineState::BUSY) {
            anyBusy = true;
        }
    }

    // Clear transmitting channels when all engines are ready
    if (!anyBusy && !mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    return anyBusy ? EngineState::BUSY : EngineState::READY;
}

void ChannelBusManager::onBeginFrame() {
    // CRITICAL: Poll engines before the frame starts to release buffers from previous frame
    // This ensures that ChannelData::mInUse flags are cleared before Channel::showPixels() is called
    // Sequence:
    //   1. Frame N transmission completes (ISR sets transmissionComplete)
    //   2. onBeginFrame() called (this method)
    //   3. poll() clears mInUse flags via ChannelEngineRMT::poll()
    //   4. Channel::showPixels() called for Frame N+1 (can now encode safely)
    poll();
}

void ChannelBusManager::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    FL_SCOPED_TRACE;
    if (channelData.size() == 0) {
        return;
    }

    if (!mActiveEngine) {
        FL_WARN("ChannelBusManager::beginTransmission() - No active engine selected");
        mLastError = "No active engine selected";
        return;
    }

    // Forward channel data to active engine by enqueueing and showing
    // Poll in a loop until engine is ready for new data
    while (mActiveEngine->poll() != EngineState::READY) {
        // Yield to watchdog task to prevent watchdog timeout
        delayMicroseconds(100);
    }

    for (const auto& channel : channelData) {
        mActiveEngine->enqueue(channel);
    }

    mActiveEngine->show();

    mLastError.clear();  // Success - clear any previous errors
}

void ChannelBusManager::onEndFrame() {
    // Trigger transmission of all batched channel data
    show();

    // Reset to highest priority engine for next frame
    // This allows us to re-evaluate engine selection each frame
    mActiveEngine = nullptr;
    mActiveEnginePriority = -1;
}

IChannelEngine* ChannelBusManager::selectEngine() {
    if (mEngines.empty()) {
        FL_WARN("ChannelBusManager::selectEngine() - No engines registered");
        return nullptr;
    }

    // Engines are already sorted by priority descending
    // Select first enabled engine (highest priority)
    for (auto& entry : mEngines) {
        if (entry.enabled) {
            mActiveEngine = entry.engine.get();
            mActiveEnginePriority = entry.priority;

            // Only log when engine selection actually changes (avoid per-frame spam)
            static fl::string lastSelectedEngine;
            if (entry.name != lastSelectedEngine) {
                lastSelectedEngine = entry.name;
                if (!entry.name.empty()) {
                    FL_DBG("ChannelBusManager: Selected engine '" << entry.name.c_str() << "' (priority " << entry.priority << ")");
                } else {
                    FL_DBG("ChannelBusManager: Selected unnamed engine (priority " << entry.priority << ")");
                }
            }
            return mActiveEngine;
        }
    }

    // No enabled engines found
    FL_WARN("ChannelBusManager::selectEngine() - No enabled engines available");
    return nullptr;
}

IChannelEngine* ChannelBusManager::getNextLowerPriorityEngine() {
    // NOTE: Currently unused - fallback logic to be implemented in future PR
    // This method will be used to switch to lower priority engine when
    // the current engine runs out of channels
    if (mEngines.empty()) {
        return nullptr;
    }

    // Find next engine with lower priority than current
    for (fl::size i = 0; i < mEngines.size(); ++i) {
        if (mEngines[i].priority < mActiveEnginePriority) {
            // Found next lower priority engine
            mActiveEnginePriority = mEngines[i].priority;
            return mEngines[i].engine.get();
        }
    }

    // No lower priority engine found
    return nullptr;
}

ChannelBusManager& channelBusManager() {
    return ChannelBusManager::instance();
}

} // namespace fl
