// ok no header - public declarations remain in fl/channels/manager.h

#include "fl/channels/manager.h"
#include "fl/log/log.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"

namespace fl {

bool ChannelManager::removeDriver(fl::shared_ptr<IChannelDriver> driver) {
    if (!driver) {
        FL_WARN("ChannelManager::removeDriver() - Null driver provided");
        return false;
    }

    // Find and remove the driver from the list
    for (size_t i = 0; i < mDrivers.size(); ++i) {
        if (mDrivers[i].driver == driver) {
            FL_DBG("ChannelManager: Removing driver '" << mDrivers[i].name << "'");

            mDrivers[i].driver->setPollNeededCallback(IChannelDriver::PollNeededCallback());

            // Remove using vector::erase (preserves sort order)
            mDrivers.erase(mDrivers.begin() + i);
            return true;  // Engine found and removed
        }
    }

    // Engine not found
    FL_WARN("ChannelManager::removeDriver() - Engine " << driver.get() << " not found in registry");
    return false;
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

bool ChannelManager::setExclusiveDriver(Bus bus, fl::u8 which) FL_NO_EXCEPT {
    return setExclusiveDriverByName(busDriverName(bus, which));
}

bool ChannelManager::setExclusiveDriverByName(const char* name) {
    // Handle null or empty name: disable everything.
    if (!name || !name[0]) {
        FL_ERROR("ChannelManager::setExclusiveDriverByName() - Null or empty driver name provided");
        mExclusiveDriver.clear();
        for (auto& entry : mDrivers) {
            entry.enabled = false;
        }
        return false;
    }

    // Store exclusive driver name for forward compatibility.
    // When non-empty, addDriver() will auto-disable non-matching drivers.
    mExclusiveDriver = name;

    // Single-pass: enable only drivers matching the given name.
    bool found = false;
    for (auto& entry : mDrivers) {
        entry.enabled = (entry.name == name);
        found = found || entry.enabled;
    }

    if (!found) {
        FL_ERROR("ChannelManager::setExclusiveDriverByName() - Driver '" << name << "' not found in registry");
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

    // Re-sort drivers by priority (descending: higher values first).
    // 1-4 drivers expected here too — sort_small avoids the quicksort body.
    fl::sort_small(mDrivers.begin(), mDrivers.end());

    FL_DBG("ChannelManager: Engine list re-sorted after priority change");
    return true;
}

ChannelManager::DriverStatus ChannelManager::driverStatus(const fl::string& name) const {
    if (name.empty()) {
        return DriverStatus::NOT_REGISTERED;
    }
    for (const auto& entry : mDrivers) {
        if (entry.name == name) {
            return entry.enabled ? DriverStatus::STATUS_ENABLED
                                  : DriverStatus::STATUS_DISABLED;
        }
    }
    return DriverStatus::NOT_REGISTERED;
}

fl::size ChannelManager::getDriverCount() const {
    return mDrivers.size();
}

fl::span<const DriverInfo> ChannelManager::getDriverInfos() const {
    if (!mCachedDriverInfo) {
        mCachedDriverInfo = fl::make_shared<fl::vector<DriverInfo>>();
    }
    auto& cache = *mCachedDriverInfo;
    // Update cache with current driver state.
    cache.clear();
    cache.reserve(mDrivers.size());

    for (const auto& entry : mDrivers) {
        // fl::string copy is cheap (shared pointer internally, no heap allocation)
        cache.push_back({
            entry.name,
            entry.priority,
            entry.enabled
        });
    }

    return cache;
}

fl::shared_ptr<IChannelDriver> ChannelManager::findDriverByName(const fl::string& name) const {
    if (name.empty()) {
        return fl::shared_ptr<IChannelDriver>();
    }
    for (const auto& entry : mDrivers) {
        if (entry.enabled && entry.name == name) {
            return entry.driver;
        }
    }
    return fl::shared_ptr<IChannelDriver>();
}

fl::shared_ptr<IChannelDriver> ChannelManager::getDriverByName(const fl::string& name) const {
    if (name.empty()) {
        FL_ERROR("ChannelManager::getDriverByName() - Empty driver name provided");
        return fl::shared_ptr<IChannelDriver>();
    }
    auto driver = findDriverByName(name);
    if (!driver) {
        FL_ERROR("ChannelManager::getDriverByName() - Driver '" << name.c_str() << "' not found or not enabled");
    }
    return driver;
}

fl::shared_ptr<IChannelDriver> ChannelManager::selectDriverForChannel(const ChannelDataPtr& data, const fl::string& affinity) {
    if (!data) {
        FL_ERROR("ChannelManager::selectDriverForChannel() - Null channel data");
        return fl::shared_ptr<IChannelDriver>();
    }

    // If affinity is specified, look up by name. Misses fall through to
    // priority dispatch below — per-frame logging is intentionally silent
    // here because `Channel::showPixels` now emits a one-shot, actionable
    // FL_ERROR with the enableDrivers<...>() / enableAllDrivers() hint
    // (#2455). Use `findDriverByName` (silent) rather than `getDriverByName`
    // (logs on miss) so the silent fall-through actually IS silent.
    do {
        if (affinity.empty()) {
            break;
        }
        auto driver = findDriverByName(affinity);
        if (!driver) {
            break;  // diagnostic emitted at the channel layer
        }
        if (!driver->canHandle(data)) {
            FL_WARN_ONCE("ChannelManager: Affinity driver '" << affinity << "' cannot handle channel data (chipset/bus mismatch). Falling back to AUTO/priority dispatch.");
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

} // namespace fl
