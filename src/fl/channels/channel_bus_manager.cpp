/// @file channel_bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "channel_bus_manager.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/engine_events.h"
#include "ftl/algorithm.h"
#include "ftl/move.h"

namespace fl {

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

void ChannelBusManager::addEngine(int priority, fl::shared_ptr<ChannelEngine> engine) {
    if (!engine) {
        FL_WARN("ChannelBusManager::addEngine() - Null engine provided");
        return;
    }

    mEngines.push_back({priority, engine});
    FL_DBG("ChannelBusManager: Added engine (priority " << priority << ")");

    // Sort engines by priority descending (highest first) after each insertion
    // Only 1-4 engines expected, so sorting on insert is negligible
    fl::sort(mEngines.begin(), mEngines.end());
}

// ChannelEngine interface implementation
void ChannelBusManager::enqueue(ChannelDataPtr channelData) {
    // Select engine on first call if not already selected
    if (!mActiveEngine) {
        mActiveEngine = selectEngine();
        if (!mActiveEngine) {
            FL_WARN("ChannelBusManager::enqueue() - No engines available");
            setLastError("No engines available for channel data");
            return;
        }
    }

    // Forward to base class to batch the channel data
    // The base class will call beginTransmission() later when show() is called
    ChannelEngine::enqueue(channelData);
}

ChannelEngine::EngineState ChannelBusManager::pollDerived() {
    if (mActiveEngine) {
        // Forward to active engine's poll() (not pollDerived - we want the wrapper logic)
        return mActiveEngine->poll();
    }

    // No active engine = idle state
    return EngineState::READY;
}

void ChannelBusManager::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    if (channelData.size() == 0) {
        return;
    }

    if (!mActiveEngine) {
        FL_WARN("ChannelBusManager::beginTransmission() - No active engine selected");
        setLastError("No active engine selected");
        return;
    }

    // Forward channel data to active engine by enqueueing and showing
    // Poll first to ensure active engine is in READY state
    FL_DBG("ChannelBusManager: Transmitting with priority " << mActiveEnginePriority);
    FL_DBG("ChannelBusManager: Polling active engine before transmission");
    mActiveEngine->poll();  // Clear any previous transmission state

    FL_DBG("ChannelBusManager: Enqueueing " << channelData.size() << " channels");
    for (const auto& channel : channelData) {
        mActiveEngine->enqueue(channel);
    }
    FL_DBG("ChannelBusManager: Calling show() on active engine");
    mActiveEngine->show();
    FL_DBG("ChannelBusManager: Transmission complete");
    clearError();  // Success - clear any previous errors
}

void ChannelBusManager::onEndFrame() {
    // Reset to highest priority engine for next frame
    // This allows us to re-evaluate engine selection each frame
    FL_DBG("ChannelBusManager: Resetting to highest priority engine for next frame");
    mActiveEngine = nullptr;
    mActiveEnginePriority = -1;
}

ChannelEngine* ChannelBusManager::selectEngine() {
    if (mEngines.empty()) {
        FL_WARN("ChannelBusManager::selectEngine() - No engines registered");
        return nullptr;
    }

    // Engines are already sorted by priority descending, so first is highest priority
    EngineEntry& entry = mEngines[0];
    mActiveEngine = entry.engine.get();
    mActiveEnginePriority = entry.priority;

    FL_DBG("ChannelBusManager: Selected engine with priority " << mActiveEnginePriority);
    return mActiveEngine;
}

ChannelEngine* ChannelBusManager::getNextLowerPriorityEngine() {
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

} // namespace fl
