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

void ChannelBusManager::addEngine(int priority, fl::shared_ptr<IChannelEngine> engine) {
    if (!engine) {
        FL_WARN("ChannelBusManager::addEngine() - Null engine provided");
        return;
    }

    mEngines.push_back({priority, engine});
    FL_DBG("ChannelBusManager: Added engine (priority " << priority << ")");

    // Sort engines by priority descending (higher values first) after each insertion
    // Higher priority values = higher precedence (e.g., priority 50 selected over priority 10)
    // Only 1-4 engines expected, so sorting on insert is negligible
    fl::sort(mEngines.begin(), mEngines.end());
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
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission
        beginTransmission(fl::span<const ChannelDataPtr>(mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelBusManager::poll() {
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

void ChannelBusManager::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
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
        // Keep polling until previous transmission completes
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

    // Engines are already sorted by priority descending (higher values first)
    // First element has highest priority value
    EngineEntry& entry = mEngines[0];
    mActiveEngine = entry.engine.get();
    mActiveEnginePriority = entry.priority;

    return mActiveEngine;
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

} // namespace fl
