#pragma once

#include "fl/span.h"
#include "fl/map.h"
#include "fl/atomic.h"
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
        static fl::SortedHeapMap<uintptr_t, int> sTrackerMap;
        static fl::atomic_int sNextId(0);

        uintptr_t tracker_addr = reinterpret_cast<uintptr_t>(this);
        int existing_id;

        if (sTrackerMap.get(tracker_addr, &existing_id)) {
            mId = existing_id;
        } else {
            mId = sNextId.fetch_add(1);
            sTrackerMap.update(tracker_addr, mId);
        }
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
        const uint8_t* data = reinterpret_cast<const uint8_t*>(pixels.data());
        size_t size = pixels.size() * 3; // 3 bytes per CRGB
        update(fl::span<const uint8_t>(data, size));
    }

    /// @brief Get the strip ID for this tracker
    /// @return The strip ID assigned to this controller
    int getId() const { return mId; }

private:
    int mId;
};

}  // namespace fl
