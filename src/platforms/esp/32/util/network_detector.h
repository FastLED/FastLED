#pragma once

// IWYU pragma: private

/// @file platforms/esp/32/util/network_detector.h
/// @brief Runtime detection of WiFi / Ethernet / Bluetooth activity on ESP32.
///
/// Used by adaptive driver paths and the channel-manager tiered wait
/// (see #2815 / #2818) to gate the deep `vTaskDelay(>=1 tick)` cooperator
/// yield on whether a radio/network stack is actually running. When no
/// network is up, the wait collapses to a non-yielding fast path; when a
/// radio is active, we keep the deep yield so WiFi/lwIP/BT controller
/// tasks don't starve (preserves #2254 fix).
///
/// Promoted from `platforms/esp/32/drivers/rmt/rmt_5/network_detector.h`
/// so non-RMT5 ESP32 builds (PARLIO-only ESP32-P4, etc.) can use it.
///
/// Weak-symbol fallback: if the WiFi / esp_netif / Bluetooth components
/// are not linked into the build, each query returns false. Cost is
/// ~1-10 µs per call (ESP-IDF API overhead).

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

class NetworkDetector {
  public:
    static bool isWiFiActive() FL_NOEXCEPT;
    static bool isWiFiConnected() FL_NOEXCEPT;

    static bool isEthernetActive() FL_NOEXCEPT;
    static bool isEthernetConnected() FL_NOEXCEPT;

    static bool isBluetoothActive() FL_NOEXCEPT;

    static bool isAnyNetworkActive() FL_NOEXCEPT;
    static bool isAnyNetworkConnected() FL_NOEXCEPT;

  private:
    NetworkDetector() = delete;
    ~NetworkDetector() = delete;
    NetworkDetector(const NetworkDetector &) = delete;
    NetworkDetector &operator=(const NetworkDetector &) = delete;
};

} // namespace fl

#endif // FL_IS_ESP32
