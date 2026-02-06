/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/channels/bus_manager.h"
#include "fl/singleton.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/log.h"
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

    if (!engineName.empty()) {
        FL_DBG("ChannelBusManager: Added engine '" << engineName.c_str() << "' (priority " << priority << ", caps: " << capStr.c_str() << ")");
    } else {
        FL_DBG("ChannelBusManager: Added unnamed engine (priority " << priority << ", caps: " << capStr.c_str() << ")");
    }

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

IChannelEngine* ChannelBusManager::getEngineByName(const fl::string& name) const {
    if (name.empty()) {
        FL_ERROR("ChannelBusManager::getEngineByName() - Empty driver name provided");
        return nullptr;
    }

    for (const auto& entry : mEngines) {
        if (entry.enabled && entry.name == name) {
            return entry.engine.get();
        }
    }

    FL_ERROR("ChannelBusManager::getEngineByName() - Driver '" << name.c_str() << "' not found or not enabled");
    return nullptr;
}

IChannelEngine::Capabilities ChannelBusManager::getCapabilities() const {
    // OR together all engine capabilities
    bool supportsClockless = false;
    bool supportsSpi = false;
    for (const auto& entry : mEngines) {
        IChannelEngine::Capabilities caps = entry.engine->getCapabilities();
        supportsClockless = supportsClockless || caps.supportsClockless;
        supportsSpi = supportsSpi || caps.supportsSpi;
    }
    return IChannelEngine::Capabilities(supportsClockless, supportsSpi);
}

bool ChannelBusManager::canHandle(const ChannelDataPtr& data) const {
    (void)data;
    return true;  // Bus manager accepts all - delegates to registered engines
}

// IChannelEngine interface implementation
void ChannelBusManager::enqueue(ChannelDataPtr channelData) {
    if (!channelData) {
        FL_WARN("ChannelBusManager::enqueue() - Null channel data provided");
        return;
    }

    // Batch the channel data for later transmission
    mEnqueuedChannels.push_back(channelData);
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
    fl::string firstError;
    for (auto& entry : mEngines) {
        EngineState result = entry.engine->poll();
        if (result.state == EngineState::BUSY) {
            anyBusy = true;
        }
        // Capture first error encountered
        if (result.state == EngineState::ERROR && firstError.empty()) {
            firstError = result.error;
        }
    }

    // Clear transmitting channels when all engines are ready
    if (!anyBusy && !mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    // Return error if any engine reported error
    if (!firstError.empty()) {
        return EngineState(EngineState::ERROR, firstError);
    }

    return EngineState(anyBusy ? EngineState::BUSY : EngineState::READY);
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

    // Select engine for this transmission
    // Use first channel data for predicate filtering (all channels should be compatible)
    IChannelEngine* engine = selectEngine(channelData.size() > 0 ? channelData[0] : nullptr);
    if (!engine) {
        FL_WARN("ChannelBusManager::beginTransmission() - No compatible engines available");
        mLastError = "No compatible engines available";
        return;
    }

    // Forward channel data to selected engine by enqueueing and showing
    // Poll in a loop until engine is ready for new data
    while (engine->poll() != EngineState::READY) {
        // Yield to watchdog task to prevent watchdog timeout
        delayMicroseconds(100);
    }

    for (const auto& channel : channelData) {
        engine->enqueue(channel);
    }

    engine->show();

    mLastError.clear();  // Success - clear any previous errors
}

void ChannelBusManager::onEndFrame() {
    // Trigger transmission of all batched channel data
    show();
}

IChannelEngine* ChannelBusManager::selectEngine(const ChannelDataPtr& data) {
    if (mEngines.empty()) {
        FL_WARN("ChannelBusManager::selectEngine() - No engines registered");
        return nullptr;
    }

    // Engines are already sorted by priority descending
    // Select first enabled engine that can handle the channel data (highest priority)
    for (auto& entry : mEngines) {
        if (!entry.enabled) {
            continue;
        }

        // Check if engine can handle this channel data (predicate filtering)
        // Skip predicate check if data is nullptr (backwards compatibility)
        if (data && !entry.engine->canHandle(data)) {
            if (!entry.name.empty()) {
                FL_DBG("ChannelBusManager: Engine '" << entry.name.c_str() << "' cannot handle channel data (chipset mismatch)");
            }
            continue;
        }

        // Found compatible engine
        // Only log when engine selection actually changes (avoid per-frame spam)
        if (entry.name != mLastSelectedEngine) {
            mLastSelectedEngine = entry.name;
            if (!entry.name.empty()) {
                FL_DBG("ChannelBusManager: Selected engine '" << entry.name.c_str() << "' (priority " << entry.priority << ")");
            } else {
                FL_DBG("ChannelBusManager: Selected unnamed engine (priority " << entry.priority << ")");
            }
        }
        return entry.engine.get();
    }

    // No compatible engines found
    FL_WARN("ChannelBusManager::selectEngine() - No compatible enabled engines available");
    return nullptr;
}

ChannelBusManager& channelBusManager() {
    return ChannelBusManager::instance();
}

} // namespace fl
