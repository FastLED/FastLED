/// @file network_state_tracker_4.h
/// @brief Singleton network-state hook for the RMT4 driver (#3469).
///
/// The RMT5 sibling
/// (`../rmt_5/network_state_tracker.h`) polls `NetworkDetector` for
/// live WiFi / Ethernet / BLE status; that infrastructure is currently
/// guarded on `FASTLED_RMT5` and would need to be lifted to a shared
/// location before the RMT4 side can call into it. This v1 provides
/// the same singleton API — `isActive()` + `hasChanged()` + `reset()`
/// — with a **stub** always-false implementation so the RMT4 driver
/// can be wired against the manager today. A follow-up PR flips the
/// stub to a real detector.
///
/// The manager (`RmtMemoryManager4::calculateMemoryBlocks(active)`)
/// then produces `3` when this returns `true`, giving TX one extra
/// 64-word block of buffer headroom during WiFi bursts. Until the
/// detector is real the recommendation stays at the historic RMT4
/// default of `2`, which matches today's behaviour bit-for-bit.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"

namespace fl {

/// @brief Singleton network-state tracker used by the RMT4 memory
///        manager. Same public shape as the RMT5 sibling for future
///        code sharing.
class NetworkStateTracker4 {
  public:
    static NetworkStateTracker4 &instance() FL_NO_EXCEPT {
        return Singleton<NetworkStateTracker4>::instance();
    }

    /// @brief Update the cached state and return true iff it changed.
    ///        Currently a no-op (stub always false).
    bool hasChanged() FL_NO_EXCEPT;

    /// @brief Read the current network state without touching the cache.
    ///        Currently stubbed to always return false.
    ///
    /// A follow-up PR will change this to poll the (lifted) network
    /// detector.
    bool isActive() const FL_NO_EXCEPT;

    /// @brief Last cached value (from the most recent `hasChanged()`).
    bool lastKnownState() const FL_NO_EXCEPT { return mLastKnownState; }

    /// @brief Test-only: reset the cache to false.
    void reset() FL_NO_EXCEPT { mLastKnownState = false; }

  private:
    friend class Singleton<NetworkStateTracker4>;

    NetworkStateTracker4() FL_NO_EXCEPT : mLastKnownState(false) {}
    ~NetworkStateTracker4() = default;

    NetworkStateTracker4(const NetworkStateTracker4 &) = delete;
    NetworkStateTracker4 &operator=(const NetworkStateTracker4 &) = delete;

    bool mLastKnownState;
};

} // namespace fl

#endif // FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM &&
       // !FASTLED_RMT5
#endif // FL_IS_ESP32
