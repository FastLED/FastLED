/// @file network_state_tracker.h
/// @brief Singleton tracker for network state changes
///
/// Centralizes network state tracking to avoid redundant checks and ensure
/// consistent state across all RMT channel drivers.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Singleton tracker for network state (WiFi, Ethernet, Bluetooth)
///
/// This singleton provides centralized network state tracking with change detection.
/// All RMT channel drivers share this single instance to avoid redundant network
/// state queries and ensure consistent behavior.
///
/// Example Usage:
/// @code
/// auto& tracker = NetworkStateTracker::instance();
///
/// if (tracker.hasChanged()) {
///     FL_DBG("Network state changed: " << (tracker.isActive() ? "ACTIVE" : "INACTIVE"));
///     // Reconfigure channels...
/// }
/// @endcode
class NetworkStateTracker {
public:
    /// @brief Get singleton instance (uses fl::Singleton for no-destructor semantics)
    /// @return Reference to the global network state tracker
    static NetworkStateTracker& instance() FL_NOEXCEPT {
        return Singleton<NetworkStateTracker>::instance();
    }

    /// @brief Check if network state has changed since last call to hasChanged()
    /// @return true if state changed, false if unchanged
    ///
    /// **Side Effect:** Updates internal last-known state to current state.
    /// Subsequent calls will return false until state changes again.
    bool hasChanged() FL_NOEXCEPT;

    /// @brief Get current network state (without affecting change tracking)
    /// @return true if any network (WiFi, Ethernet, or Bluetooth) is active
    bool isActive() const FL_NOEXCEPT;

    /// @brief Get last known network state (cached value)
    /// @return Last network state returned by hasChanged()
    bool lastKnownState() const FL_NOEXCEPT { return mLastKnownState; }

    /// @brief Reset tracker state (for testing)
    void reset() FL_NOEXCEPT { mLastKnownState = false; }

private:
    friend class Singleton<NetworkStateTracker>;

    NetworkStateTracker() : mLastKnownState(false) {}
    ~NetworkStateTracker() = default;

    // Non-copyable, non-movable
    NetworkStateTracker(const NetworkStateTracker&) = delete;
    NetworkStateTracker& operator=(const NetworkStateTracker&) = delete;
    NetworkStateTracker(NetworkStateTracker&&) = delete;
    NetworkStateTracker& operator=(NetworkStateTracker&&) = delete;

    bool mLastKnownState;  ///< Last known network state (cached)
};

} // namespace fl

#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
