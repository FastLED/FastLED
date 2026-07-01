// IWYU pragma: private

/// @file network_state_tracker_4.cpp.hpp
/// @brief RMT4 network state tracker — stub impl (#3469).

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt/rmt_4/network_state_tracker_4.h"

#include "fl/stl/noexcept.h"

namespace fl {

bool NetworkStateTracker4::hasChanged() FL_NO_EXCEPT {
    // Stub: today's poll always returns false, so no state change is
    // ever observed. See header docblock for the follow-up plan.
    bool current = isActive();
    if (current != mLastKnownState) {
        mLastKnownState = current;
        return true;
    }
    return false;
}

bool NetworkStateTracker4::isActive() const FL_NO_EXCEPT {
    // v1 stub. Follow-up PR wires this to the shared NetworkDetector
    // once that code is lifted out of the FASTLED_RMT5 guard.
    return false;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM &&
       // !FASTLED_RMT5
#endif // FL_IS_ESP32
