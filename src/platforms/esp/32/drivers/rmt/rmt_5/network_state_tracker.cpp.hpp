/// @file network_state_tracker.cpp
/// @brief Singleton tracker for network state changes implementation

#include "network_state_tracker.h"

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32) && FASTLED_RMT5

#include "network_detector.h"

namespace fl {

bool NetworkStateTracker::hasChanged() {
    bool currentState = NetworkDetector::isAnyNetworkActive();

    if (currentState != mLastKnownState) {
        // State changed - update and return true
        mLastKnownState = currentState;
        return true;
    }

    // No change
    return false;
}

bool NetworkStateTracker::isActive() const {
    return NetworkDetector::isAnyNetworkActive();
}

} // namespace fl

#endif // FL_IS_ESP32 && FASTLED_RMT5
