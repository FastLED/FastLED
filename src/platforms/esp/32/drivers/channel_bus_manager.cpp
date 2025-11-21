/// @file channel_bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/compiler_control.h"
#ifdef ESP32

#include "channel_bus_manager.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/engine_events.h"
#include "ftl/algorithm.h"
#include "ftl/move.h"
#include "ftl/unique_ptr.h"

// Include concrete engine implementations
#if FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "spi/channel_engine_spi.h"
#endif
#if FASTLED_ESP32_HAS_PARLIO
#include "parlio/channel_engine_parlio.h"
#endif

namespace fl {

ChannelBusManager::ChannelBusManager() {
    FL_DBG("ChannelBusManager: Initializing");

    // Register as frame event listener for per-frame reset
    EngineEvents::addListener(this);

    // Construct owned engines and add them with priorities
    // Add in priority order (highest first) for clarity
    #if FASTLED_ESP32_HAS_PARLIO
    mEngines.push_back({PRIORITY_PARLIO, fl::unique_ptr<ChannelEngine>(createParlioEngine())});
    FL_DBG("ChannelBusManager: Added PARLIO engine (priority " << PRIORITY_PARLIO << ")");
    #endif

    #if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    mEngines.push_back({PRIORITY_SPI, fl::make_unique<ChannelEngineSpi>()});
    FL_DBG("ChannelBusManager: Added SPI engine (priority " << PRIORITY_SPI << ")");
    #endif

    #if FASTLED_RMT5
    mEngines.push_back({PRIORITY_RMT, fl::make_unique<ChannelEngineRMT>()});
    FL_DBG("ChannelBusManager: Added RMT engine (priority " << PRIORITY_RMT << ")");
    #endif

    // Sort by priority descending (highest first)
    fl::sort(mEngines.begin(), mEngines.end());
    FL_DBG("ChannelBusManager: Sorted " << mEngines.size() << " engines by priority");
}

ChannelBusManager::~ChannelBusManager() {
    FL_DBG("ChannelBusManager: Destructor called");
    // Owned engines automatically cleaned up by unique_ptr destructors
}

ChannelBusManager& ChannelBusManager::instance() {
    static ChannelBusManager instance;
    return instance;
}

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
    // We can't call beginTransmission() directly as it's protected
    FL_DBG("ChannelBusManager: Transmitting with priority " << mActiveEnginePriority);
    for (const auto& channel : channelData) {
        mActiveEngine->enqueue(channel);
    }
    mActiveEngine->show();
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

#endif // ESP32
