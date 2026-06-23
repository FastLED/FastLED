#pragma once

// IWYU pragma: private

#include "fl/stl/span.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/atomic.h"
#include "fl/stl/bit_cast.h"
#include "crgb.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "fl/stl/noexcept.h"

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
    ActiveStripTracker() FL_NOEXCEPT {
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
    ActiveStripTracker(const ActiveStripTracker& other) FL_NOEXCEPT {
        (void)other; // Don't copy ID - get fresh one
        uintptr_t tracker_addr = fl::ptr_to_int(this);
        mId = getNextId().fetch_add(1);
        getTrackerMap().update(tracker_addr, mId);
    }

    /// @brief Copy assignment - keeps same ID, updates address
    ActiveStripTracker& operator=(const ActiveStripTracker& other) FL_NOEXCEPT {
        if (this != &other) {
            uintptr_t addr = fl::ptr_to_int(this);
            getTrackerMap().erase(addr);
            mId = other.mId;
            getTrackerMap().update(addr, mId);
        }
        return *this;
    }

    /// @brief Move constructor - transfers ID to new address
    ActiveStripTracker(ActiveStripTracker&& other) FL_NOEXCEPT {
        mId = other.mId;
        uintptr_t old_addr = fl::ptr_to_int(&other);
        uintptr_t new_addr = fl::ptr_to_int(this);
        getTrackerMap().erase(old_addr);
        getTrackerMap().update(new_addr, mId);
    }

    /// @brief Move assignment - transfers ID to new address
    ActiveStripTracker& operator=(ActiveStripTracker&& other) FL_NOEXCEPT {
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
    void update(fl::span<const u8> pixel_data) FL_NOEXCEPT {
        fl::ActiveStripData::Instance() FL_NOEXCEPT .update(mId, fl::millis(), pixel_data);
    }

    /// @brief Update strip data with CRGB pixel data
    /// @param pixels Span of CRGB pixels
    void update(fl::span<const CRGB> pixels) FL_NOEXCEPT {
        // Convert CRGB span to uint8_t span (safe because CRGB is packed RGB)
        const u8* data = fl::bit_cast<const u8*>(pixels.data());
        size_t size = pixels.size() * 3; // 3 bytes per CRGB
        update(fl::span<const u8>(data, size));
    }

    /// @brief Update from a fl::vector<u8> (resolves ambiguity with CRGB overload)
    void update(const fl::vector<u8>& container) FL_NOEXCEPT {
        update(fl::span<const u8>(container.data(), container.size())); // ok span from pointer
    }

    /// @brief Get the strip ID for this tracker
    /// @return The strip ID assigned to this controller
    int getId() const FL_NOEXCEPT { return mId; }

    /// @brief Reset all tracker state (for testing)
    /// Clears the tracker map and resets the ID counter
    /// WARNING: Only use this in test environments!
    static void resetForTesting() FL_NOEXCEPT {
        getTrackerMap().clear();
        getNextId().store(0);
    }

private:
    /// @brief Get the static tracker map (accessor for resetForTesting)
    static fl::flat_map<uintptr_t, int>& getTrackerMap() FL_NOEXCEPT {
        static fl::flat_map<uintptr_t, int> sTrackerMap;
        return sTrackerMap;
    }

    /// @brief Get the static ID counter (accessor for resetForTesting)
    static fl::atomic_int& getNextId() FL_NOEXCEPT {
        static fl::atomic_int sNextId(0) FL_NOEXCEPT;  // okay static in header
        return sNextId;
    }

    int mId;
};

}  // namespace fl
