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

// Include concrete engine implementations
#if FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "spi/channel_engine_spi.h"
#endif

namespace fl {

ChannelBusManager::ChannelBusManager() {
    FL_DBG("ChannelBusManager: Initializing");

    // Register as frame event listener for per-frame reset
    EngineEvents::addListener(this);

    // Construct owned engines and register them with priorities
    #if FASTLED_RMT5
    mRmtEngine = new ChannelEngineRMT();
    registerEngine(PRIORITY_RMT, mRmtEngine);
    FL_DBG("ChannelBusManager: Registered RMT engine (priority " << PRIORITY_RMT << ")");
    #endif

    #if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    mSpiEngine = new ChannelEngineSpi();
    registerEngine(PRIORITY_SPI, mSpiEngine);
    FL_DBG("ChannelBusManager: Registered SPI engine (priority " << PRIORITY_SPI << ")");
    #endif
}

ChannelBusManager::~ChannelBusManager() {
    FL_DBG("ChannelBusManager: Destructor called");

    // Cleanup owned engines
    #if FASTLED_RMT5
    if (mRmtEngine) {
        delete mRmtEngine;
        mRmtEngine = nullptr;
    }
    #endif

    #if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    if (mSpiEngine) {
        delete mSpiEngine;
        mSpiEngine = nullptr;
    }
    #endif
}

ChannelBusManager& ChannelBusManager::instance() {
    static ChannelBusManager instance;
    return instance;
}

void ChannelBusManager::registerEngine(int priority, ChannelEngine* engine) {
    if (!engine) {
        FL_WARN("ChannelBusManager::registerEngine() - null engine pointer ignored");
        return;
    }

    FL_DBG("ChannelBusManager: Registering engine with priority " << priority);

    // Add engine to registry
    EngineEntry entry{priority, engine};
    mEngines.push_back(entry);

    // Sort by priority descending (highest first)
    fl::sort(mEngines.begin(), mEngines.end());

    FL_DBG("ChannelBusManager: Now have " << mEngines.size() << " registered engines");
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
    mActiveEngine = entry.engine;
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
            return mEngines[i].engine;
        }
    }

    // No lower priority engine found
    return nullptr;
}

} // namespace fl

#endif // ESP32
