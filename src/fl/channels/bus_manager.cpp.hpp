/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager



#include "fl/channels/bus_manager.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"
#include "fl/fastled.h"
#include "fl/singleton.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/engine_events.h"
#include "fl/delay.h"
#include "fl/stl/time.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/move.h"
#include "fl/trace.h"
#include "fl/async.h"
#include "platforms/init_channel_engine.h"

// Forward declarations for FastLED accessor functions (defined in FastLED.cpp.hpp)
fl::u8 get_brightness();
fl::u16 get_fps();

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
        FL_ERROR("ChannelBusManager::addEngine() - Null engine provided (programmer error)");
        return;
    }

    // Get engine name from the engine itself
    fl::string engineName = engine->getName();

    // Reject engines with empty names
    if (engineName.empty()) {
        FL_ERROR("ChannelBusManager::addEngine() - Engine has empty name (invalid engine implementation)");
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

    // If replacing, wait for all engines to become READY
    if (replacing) {
        FL_DBG("ChannelBusManager: Waiting for all engines to become READY before replacement");
        wait();

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

    // Wait for all engines to become READY before clearing
    // This prevents clearing engines that are still transmitting
    wait();

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
        FL_ERROR("ChannelBusManager::enqueue() - Null channel data provided (programmer error)");
        return;
    }

    // Batch the channel data for later transmission
    // Note: Channel pointer is nullptr when called externally (backwards compatibility)
    mEnqueuedChannels.push_back({nullptr, channelData});
}

// Internal enqueue with channel association (for event firing)
void ChannelBusManager::enqueueWithChannel(Channel* channel, ChannelDataPtr channelData) {
    if (!channelData) {
        FL_ERROR("ChannelBusManager::enqueueWithChannel() - Null channel data provided (programmer error)");
        return;
    }

    // Batch the channel data with channel association
    mEnqueuedChannels.push_back({channel, channelData});
}

void ChannelBusManager::show() {
    FL_SCOPED_TRACE;
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission
        beginTransmission(fl::span<const ChannelDataEntry>(mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelBusManager::poll() {
    FL_SCOPED_TRACE;
    // Poll all registered engines to allow buffer cleanup
    // even when mActiveEngine is nullptr (after onEndFrame reset)
    bool anyBusy = false;
    fl::string firstError;
    for (auto& entry : mEngines) {
        IChannelEngine::EngineState result = entry.engine->poll();
        if (result.state == IChannelEngine::EngineState::BUSY) {
            anyBusy = true;
        }
        // Capture first error encountered
        if (result.state == IChannelEngine::EngineState::ERROR && firstError.empty()) {
            firstError = result.error;
        }
    }

    // Clear transmitting channels when all engines are ready
    if (!anyBusy && !mTransmittingChannels.empty()) {
        mTransmittingChannels.clear();
    }

    // Return error if any engine reported error
    if (!firstError.empty()) {
        return IChannelEngine::EngineState(IChannelEngine::EngineState::ERROR, firstError);
    }

    return IChannelEngine::EngineState(anyBusy ? IChannelEngine::EngineState::BUSY : IChannelEngine::EngineState::READY);
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

void ChannelBusManager::wait() {
    waitForCondition([this]() { return poll() == IChannelEngine::EngineState::READY; });
}


void ChannelBusManager::onBeginFrame() {
    // CRITICAL: Poll engines before the frame starts to release buffers from previous frame
    // This ensures that ChannelData::mInUse flags are cleared before encoding
    // Sequence:
    //   1. Frame N transmission completes (ISR sets transmissionComplete)
    //   2. onBeginFrame() called (this method)
    //   3. poll() clears mInUse flags via engine poll()
    poll();

    // Note: encodeTrackedChannels() moved to onEndFrame() to synchronize dither state
    // with legacy controllers (both encode in the same frame, using consecutive R values)
}

void ChannelBusManager::encodeTrackedChannels() {
    // Top-down encoding: ChannelBusManager drives encoding instead of CLEDController draw loop
    // This ensures proper attribute application (dither, brightness, etc.) before encoding
    if (mChannels.empty()) {
        return;
    }

    // Fetch brightness and FPS from FastLED singleton (reduces AVR binary size)
    u8 brightness = get_brightness();
    u16 fps = get_fps();

    // Apply dither logic from legacy system: if FPS < 100, disable dither
    // Note: Only disable if FPS is being actively tracked (fps > 0)
    // FPS = 0 means FPS tracking hasn't started, so respect user's dither setting
    bool disableDither = (fps > 0 && fps < 100);

    for (const fl::ChannelPtr& channel : mChannels) {
        if (!channel) {
            continue;
        }

        // All channels in mChannels are enabled for rendering
        // The draw list is now managed by ChannelBusManager

        // Save and apply dither setting (matches legacy FastLED.show() logic)
        u8 savedDither = channel->getDither();
        if (disableDither) {
            channel->setDither(0);
        }

        // Encode pixels and get the channel data
        ChannelDataPtr data = channel->encodePixels(brightness);

        // Restore dither setting
        channel->setDither(savedDither);

        // Enqueue encoded data if encoding succeeded
        if (data) {
            enqueueWithChannel(channel.get(), data);
        }
    }
}

void ChannelBusManager::beginTransmission(fl::span<const ChannelDataEntry> channelData) {
    FL_SCOPED_TRACE;
    if (channelData.size() == 0) {
        return;
    }

    // Select engine for this transmission
    // Use first channel data for predicate filtering (all channels should be compatible)
    IChannelEngine* engine = selectEngine(channelData.size() > 0 ? channelData[0].data : nullptr);
    if (!engine) {
        FL_ERROR("ChannelBusManager::beginTransmission() - No compatible engines available (system misconfiguration)");
        mLastError = "No compatible engines available";
        return;
    }

    // Forward channel data to selected engine by enqueueing and showing
    // Wait until engine is ready for new data (with check-pump-delay logic)
    if (engine) {
        waitForCondition([engine]() { return engine->poll() == IChannelEngine::EngineState::READY; });
    }

    // Fire onChannelEnqueued events with actual selected engine name
    auto& events = ChannelEvents::instance();
    fl::string engineName = engine->getName();

    for (const auto& entry : channelData) {
        engine->enqueue(entry.data);

        // Fire event with accurate engine name (after engine selection)
        if (entry.channel) {
            events.onChannelEnqueued(*entry.channel, engineName);
        }
    }

    engine->show();

    mLastError.clear();  // Success - clear any previous errors
}

void ChannelBusManager::onEndFrame() {
    // Encode tracked channels (after legacy controllers have encoded)
    // Dither sync now handled in Channel::encodePixels() between the two PixelController creations
    encodeTrackedChannels();

    // Trigger transmission of all batched channel data
    show();
}

void ChannelBusManager::reset() {
    // Clear all pending channels
    mEnqueuedChannels.clear();
    mTransmittingChannels.clear();
    // Allow all channel engines to clean up
    wait();
    FL_DBG("ChannelBusManager: reset() - cleared all channels");
}

void ChannelBusManager::registerChannel(fl::ChannelPtr channel) {
    if (!channel) {
        return;
    }

    // Check if already registered (uses shared_ptr equality)
    for (const auto& ch : mChannels) {
        if (ch == channel) {
            FL_WARN("ChannelBusManager::registerChannel() - Channel already registered");
            return;
        }
    }

    mChannels.push_back(channel);
    FL_DBG("ChannelBusManager: Registered channel (id=" << channel->id() << ", name='" << channel->name().c_str() << "')");

    // Fire event after adding to tracked list
    auto& events = ChannelEvents::instance();
    events.onChannelAdded(*channel);
}

void ChannelBusManager::unregisterChannel(fl::ChannelPtr channel) {
    if (!channel) {
        return;
    }

    for (auto it = mChannels.begin(); it != mChannels.end(); ++it) {
        if (*it == channel) {
            // Fire event before removing from tracked list
            auto& events = ChannelEvents::instance();
            events.onChannelRemoved(*channel);

            mChannels.erase(it);
            FL_DBG("ChannelBusManager: Unregistered channel (id=" << channel->id() << ")");
            return;
        }
    }

    FL_WARN("ChannelBusManager::unregisterChannel() - Channel not found in tracked list");
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
