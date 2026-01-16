#pragma once

#include "fl/stl/span.h"
#include "fl/stl/map.h"
#include "fl/stl/atomic.h"
#include "fl/stl/bit_cast.h"
#include "crgb.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"

namespace fl {

/// @brief Helper class to track and update strip data for LED controllers
///
/// This class encapsulates the strip ID management and data capture logic,
/// making it easier for LED controllers to report their state to the
/// ActiveStripData system.
///
/// The tracker uses its own address to obtain a unique sequential ID,
/// eliminating the need to pass the controller pointer.
///
/// Usage:
/// @code
/// class MyController {
///     ActiveStripTracker mTracker;  // No constructor argument needed!
///
///     void showPixels() {
///         // ... render pixels ...
///         mTracker.update(fl::span<const uint8_t>(rgb_data, size));
///     }
/// };
/// @endcode
class ActiveStripTracker {
public:
    /// @brief Constructor - registers this tracker and gets a unique sequential ID
    /// Uses the tracker's own address to obtain a stable, unique identifier
    ActiveStripTracker() {
        uintptr_t tracker_addr = fl::ptr_to_int(this);
        int existing_id;

        if (getTrackerMap().get(tracker_addr, &existing_id)) {
            mId = existing_id;
        } else {
            mId = getNextId().fetch_add(1);
            getTrackerMap().update(tracker_addr, mId);
        }
    }

    /// @brief Copy constructor - creates new tracker with fresh ID
    /// Each copy gets its own unique ID in the registry
    ActiveStripTracker(const ActiveStripTracker& other) {
        (void)other; // Don't copy ID - get fresh one
        uintptr_t tracker_addr = fl::ptr_to_int(this);
        mId = getNextId().fetch_add(1);
        getTrackerMap().update(tracker_addr, mId);
    }

    /// @brief Copy assignment - keeps same ID, updates address
    ActiveStripTracker& operator=(const ActiveStripTracker& other) {
        if (this != &other) {
            uintptr_t addr = fl::ptr_to_int(this);
            getTrackerMap().erase(addr);
            mId = other.mId;
            getTrackerMap().update(addr, mId);
        }
        return *this;
    }

    /// @brief Move constructor - transfers ID to new address
    ActiveStripTracker(ActiveStripTracker&& other) noexcept {
        mId = other.mId;
        uintptr_t old_addr = fl::ptr_to_int(&other);
        uintptr_t new_addr = fl::ptr_to_int(this);
        getTrackerMap().erase(old_addr);
        getTrackerMap().update(new_addr, mId);
    }

    /// @brief Move assignment - transfers ID to new address
    ActiveStripTracker& operator=(ActiveStripTracker&& other) noexcept {
        if (this != &other) {
            uintptr_t old_this = fl::ptr_to_int(this);
            uintptr_t old_other = fl::ptr_to_int(&other);
            getTrackerMap().erase(old_this);
            getTrackerMap().erase(old_other);
            mId = other.mId;
            uintptr_t new_this = fl::ptr_to_int(this);
            getTrackerMap().update(new_this, mId);
        }
        return *this;
    }

    /// @brief Destructor - unregisters this tracker
    /// Removes the tracker's address from the map to clean up orphaned IDs
    ~ActiveStripTracker() {
        uintptr_t tracker_addr = fl::ptr_to_int(this);
        getTrackerMap().erase(tracker_addr);
    }

    /// @brief Update strip data with RGB pixel data
    /// @param pixel_data Span of RGB pixel data (3 bytes per pixel: R, G, B)
    void update(fl::span<const uint8_t> pixel_data) {
        fl::ActiveStripData::Instance().update(mId, millis(), pixel_data);
    }

    /// @brief Update strip data with CRGB pixel data
    /// @param pixels Span of CRGB pixels
    void update(fl::span<const CRGB> pixels) {
        // Convert CRGB span to uint8_t span (safe because CRGB is packed RGB)
        const uint8_t* data = fl::bit_cast<const uint8_t*>(pixels.data());
        size_t size = pixels.size() * 3; // 3 bytes per CRGB
        update(fl::span<const uint8_t>(data, size));
    }

    /// @brief Get the strip ID for this tracker
    /// @return The strip ID assigned to this controller
    int getId() const { return mId; }

    /// @brief Reset all tracker state (for testing)
    /// Clears the tracker map and resets the ID counter
    /// WARNING: Only use this in test environments!
    static void resetForTesting() {
        getTrackerMap().clear();
        getNextId().store(0);
    }

private:
    /// @brief Get the static tracker map (accessor for resetForTesting)
    static fl::SortedHeapMap<uintptr_t, int>& getTrackerMap() {
        static fl::SortedHeapMap<uintptr_t, int> sTrackerMap;
        return sTrackerMap;
    }

    /// @brief Get the static ID counter (accessor for resetForTesting)
    static fl::atomic_int& getNextId() {
        static fl::atomic_int sNextId(0);  // okay static in header
        return sNextId;
    }

    int mId;
};

}  // namespace fl
