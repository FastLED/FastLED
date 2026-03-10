/// @file bus_manager.cpp
/// @brief Implementation of unified channel bus manager

#include "fl/channels/manager.h"
#include "fl/singleton.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/engine_events.h"
#include "fl/stl/chrono.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/move.h"
#include "fl/trace.h"
#include "fl/stl/async.h"
#include "platforms/init_channel_driver.h"

namespace fl {

ChannelManager& ChannelManager::instance() {
    auto& out = Singleton<ChannelManager>::instance();
    // Lazy initialization of platform-specific channel drivers
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false; // okay static in header
    if (!sInitialized) {
        sInitialized = true;
        platforms::initChannelDrivers();
    }
    return out;
}

ChannelManager::ChannelManager() {
    FL_DBG("ChannelManager: Initializing");

    // Register as frame event listener for per-frame reset
    EngineEvents::addListener(this);
}

ChannelManager::~ChannelManager() {
    FL_DBG("ChannelManager: Destructor called");

    // Remove self from EngineEvents listener list
    EngineEvents::removeListener(this);

    // Shared drivers automatically cleaned up by shared_ptr destructors
}

void ChannelManager::addDriver(int priority, fl::shared_ptr<IChannelDriver> driver) {
    if (!driver) {
        FL_WARN("ChannelManager::addDriver() - Null driver provided");
        return;
    }

    // Get driver name from the driver itself
    fl::string engineName = driver->getName();

    // Reject drivers with empty names
    if (engineName.empty()) {
        FL_WARN("ChannelManager::addDriver() - Engine has empty name (driver->getName() returned empty string)");
        return;
    }

    // Check if driver with this name already exists
    bool replacing = false;
    for (const auto& entry : mDrivers) {
        if (entry.name == engineName) {
            replacing = true;
            FL_WARN("ChannelManager::addDriver() - Replacing existing driver '" << engineName.c_str() << "'");
            break;
        }
    }

    // If replacing, wait for all drivers to become READY
    if (replacing) {
        FL_DBG("ChannelManager: Waiting for all drivers to become READY before replacement");
        waitForReady();

        // Remove the old driver with matching name (shared_ptr may trigger deletion)
        for (size_t i = 0; i < mDrivers.size(); ++i) {
            if (mDrivers[i].name == engineName) {
                FL_DBG("ChannelManager: Removing old driver '" << engineName.c_str() << "' (shared_ptr may delete)");
                mDrivers.erase(mDrivers.begin() + i);
                break;
            }
        }
    }

    // Respect exclusive driver mode: auto-disable if name doesn't match exclusive driver
    bool enabled = true;  // Default: enabled
    if (!mExclusiveDriver.empty()) {
        enabled = (engineName == mExclusiveDriver);  // Only enable if matches exclusive driver
    }

    mDrivers.push_back({priority, driver, engineName, enabled});

    // Build capability string for debug output
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

    FL_DBG("ChannelManager: Added driver '" << engineName.c_str() << "' (priority " << priority << ", caps: " << capStr.c_str() << ")");

    // Sort drivers by priority descending (higher values first) after each insertion
    // Higher priority values = higher precedence (e.g., priority 50 selected over priority 10)
    // Only 1-4 drivers expected, so sorting on insert is negligible
    fl::sort(mDrivers.begin(), mDrivers.end());
}

bool ChannelManager::removeDriver(fl::shared_ptr<IChannelDriver> driver) {
    if (!driver) {
        FL_WARN("ChannelManager::removeDriver() - Null driver provided");
        return false;
    }

    // Find and remove the driver from the list
    for (size_t i = 0; i < mDrivers.size(); ++i) {
        if (mDrivers[i].driver == driver) {
            FL_DBG("ChannelManager: Removing driver '" << mDrivers[i].name << "'");

            // Remove using vector::erase (preserves sort order)
            mDrivers.erase(mDrivers.begin() + i);
            return true;  // Engine found and removed
        }
    }

    // Engine not found
    FL_WARN("ChannelManager::removeDriver() - Engine " << driver.get() << " not found in registry");
    return false;
}

void ChannelManager::clearAllDrivers() {
    FL_DBG("ChannelManager: Waiting for all drivers to become READY before clearing");

    // Wait for all drivers to become READY before clearing
    // This prevents clearing drivers that are still transmitting
    waitForReady();

    FL_DBG("ChannelManager: Clearing " << mDrivers.size() << " drivers");

    // Clear all drivers (shared_ptr handles cleanup automatically)
    mDrivers.clear();
}

void ChannelManager::setDriverEnabled(const char* name, bool enabled) {
    if (!name) {
        FL_ERROR("ChannelManager::setDriverEnabled() - Null driver name provided");
        return;
    }

    bool found = false;
    for (auto& entry : mDrivers) {
        if (entry.name == name) {
            entry.enabled = enabled;
            found = true;
            FL_DBG("ChannelManager: Driver '" << name << "' " << (enabled ? "enabled" : "disabled"));
        }
    }

    if (!found) {
        FL_ERROR("ChannelManager::setDriverEnabled() - Driver '" << name << "' not found in registry");
    }
}

bool ChannelManager::setExclusiveDriver(const char* name) {
    // Handle null or empty name
    if (!name || !name[0]) {
        FL_ERROR("ChannelManager::setExclusiveDriver() - Null or empty driver name provided");
        mExclusiveDriver.clear();
        // Disable all drivers
        for (auto& entry : mDrivers) {
            entry.enabled = false;
        }
        return false;
    }

    // Store exclusive driver name for forward compatibility
    // When non-empty, addDriver() will auto-disable non-matching drivers
    mExclusiveDriver = name;

    // Single-pass: enable only drivers matching the given name
    bool found = false;
    for (auto& entry : mDrivers) {
        entry.enabled = (entry.name == name);
        found = found || entry.enabled;
    }

    if (!found) {
        FL_ERROR("ChannelManager::setExclusiveDriver() - Driver '" << name << "' not found in registry");
    }

    return found;
}

bool ChannelManager::setDriverPriority(const fl::string& name, int priority) {
    if (name.empty()) {
        FL_ERROR("ChannelManager::setDriverPriority() - Empty driver name provided");
        return false;
    }

    // Find driver and update priority
    bool found = false;
    for (auto& entry : mDrivers) {
        if (entry.name == name) {
            entry.priority = priority;
            found = true;
            FL_DBG("ChannelManager: Driver '" << name << "' priority changed to " << priority);
            break;
        }
    }

    if (!found) {
        FL_ERROR("ChannelManager::setDriverPriority() - Driver '" << name << "' not found in registry");
        return false;
    }

    // Re-sort drivers by priority (descending: higher values first)
    fl::sort(mDrivers.begin(), mDrivers.end());

    FL_DBG("ChannelManager: Engine list re-sorted after priority change");
    return true;
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

fl::size ChannelManager::getDriverCount() const {
    return mDrivers.size();
}

fl::span<const DriverInfo> ChannelManager::getDriverInfos() const {
    // Update cache with current driver state
    mCachedDriverInfo.clear();
    mCachedDriverInfo.reserve(mDrivers.size());

    for (const auto& entry : mDrivers) {
        // fl::string copy is cheap (shared pointer internally, no heap allocation)
        mCachedDriverInfo.push_back({
            entry.name,
            entry.priority,
            entry.enabled
        });
    }

    return mCachedDriverInfo;
}

fl::shared_ptr<IChannelDriver> ChannelManager::getDriverByName(const fl::string& name) const {
    if (name.empty()) {
        FL_ERROR("ChannelManager::getDriverByName() - Empty driver name provided");
        return fl::shared_ptr<IChannelDriver>();
    }

    for (const auto& entry : mDrivers) {
        if (entry.enabled && entry.name == name) {
            return entry.driver;  // Return shared_ptr directly
        }
    }

    FL_ERROR("ChannelManager::getDriverByName() - Driver '" << name.c_str() << "' not found or not enabled");
    return fl::shared_ptr<IChannelDriver>();
}

fl::shared_ptr<IChannelDriver> ChannelManager::selectDriverForChannel(const ChannelDataPtr& data, const fl::string& affinity) {
    if (!data) {
        FL_ERROR("ChannelManager::selectDriverForChannel() - Null channel data");
        return fl::shared_ptr<IChannelDriver>();
    }

    // If affinity is specified, look up by name    
    do {
        if (affinity.empty()) {
            break;
        }
        auto driver = getDriverByName(affinity);
        if (!driver) {
            FL_ERROR("ChannelManager: Affinity driver '" << affinity << "' not found");
            break;
        }
        if (!driver->canHandle(data)) {
            FL_ERROR("ChannelManager: Affinity driver '" << affinity << "' cannot handle channel data");
            break;
        }
        return driver;
    } while (false);
    

    // No affinity: iterate drivers by priority (already sorted descending)
    for (const auto& entry : mDrivers) {
        if (!entry.enabled) continue;
        if (entry.driver->canHandle(data)) {
            return entry.driver;  // Return shared_ptr
        }
    }

    FL_ERROR("ChannelManager: No compatible driver found for channel data");
    return fl::shared_ptr<IChannelDriver>();
}


template<typename Condition>
bool ChannelManager::waitForCondition(Condition condition, u32 timeoutMs) {
    const u32 startTime = timeoutMs > 0 ? millis() : 0;

    while (!condition()) {
        // Check timeout if specified
        if (timeoutMs > 0 && (millis() - startTime >= timeoutMs)) {
            FL_ERROR("ChannelManager: Timeout occurred while waiting for condition");
            return false;  // Timeout occurred
        }

        // OS yield only — keeps WiFi/lwIP alive without pumping
        // tasks or coroutines during frame transitions (re-entrancy risk).
        async_run(250, AsyncFlags::SYSTEM);
    }

    return true;  // Condition met
}

IChannelDriver::DriverState ChannelManager::poll() {
    // Poll all registered drivers and return aggregate state
    // Priority order: ERROR > BUSY > DRAINING > READY
    bool anyBusy = false;
    bool anyDraining = false;
    fl::string firstError;

    for (auto& entry : mDrivers) {
        IChannelDriver::DriverState result = entry.driver->poll();
        if (result.state == IChannelDriver::DriverState::BUSY) {
            anyBusy = true;
        } else if (result.state == IChannelDriver::DriverState::DRAINING) {
            anyDraining = true;
        }
        // Capture first error encountered
        if (result.state == IChannelDriver::DriverState::ERROR && firstError.empty()) {
            firstError = result.error;
        }
    }

    // Return error if any driver reported error
    if (!firstError.empty()) {
        return IChannelDriver::DriverState(IChannelDriver::DriverState::ERROR, firstError);
    }

    if (anyBusy) {
        return IChannelDriver::DriverState(IChannelDriver::DriverState::BUSY);
    }
    if (anyDraining) {
        return IChannelDriver::DriverState(IChannelDriver::DriverState::DRAINING);
    }
    return IChannelDriver::DriverState(IChannelDriver::DriverState::READY);
}

bool ChannelManager::waitForReady(u32 timeoutMs) {
    bool ok = waitForCondition([this]() {
        return poll().state == IChannelDriver::DriverState::READY;
    }, timeoutMs);
    if (!ok) {
        FL_ERROR("ChannelManager: Timeout occurred while waiting for READY state");
    }
    return ok;
}

bool ChannelManager::waitForReadyOrDraining(u32 timeoutMs) {
    bool ok = waitForCondition([this]() {
        auto state = poll();
        bool draining_or_done = (
            state.state == IChannelDriver::DriverState::READY ||
            state.state == IChannelDriver::DriverState::DRAINING
        );
        return draining_or_done;
    }, timeoutMs);
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
