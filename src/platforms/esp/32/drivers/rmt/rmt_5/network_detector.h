#pragma once

// Network Detection API for RMT Network-Aware Dynamic Configuration
// Provides runtime network activity detection (WiFi, Ethernet, Bluetooth)
// with graceful fallback for builds without network components linked.

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/stdint.h"

namespace fl {

/// @brief Network activity detector for adaptive RMT channel management
///
/// This class provides runtime detection of network activity to enable
/// adaptive RMT channel configuration. It uses weak symbol fallback
/// to gracefully handle builds where network components are not linked.
///
/// **Supported Network Types:**
/// - **WiFi**: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
/// - **Ethernet**: ESP32 (with external PHY), ESP32-C6
/// - **Bluetooth**: ESP32, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
///
/// **Usage:**
/// @code
/// // Check specific network type
/// if (NetworkDetector::isWiFiActive()) {
///     // WiFi is enabled - use robust configuration
/// }
///
/// // Check if any network is active
/// if (NetworkDetector::isAnyNetworkActive()) {
///     // Some network is active - adjust RMT configuration
/// }
/// @endcode
///
/// **Graceful Fallback:**
/// If network components are not linked, all methods return false (no crashes).
///
/// **Performance:** ~1-10µs per call (ESP-IDF API overhead)
class NetworkDetector {
  public:
    //=========================================================================
    // WiFi Detection
    //=========================================================================

    /// @brief Check if WiFi is currently active (any mode except NULL)
    ///
    /// Detects if WiFi is in any active mode:
    /// - WIFI_MODE_STA (Station mode - connecting to AP)
    /// - WIFI_MODE_AP (Access Point mode)
    /// - WIFI_MODE_APSTA (Both Station and AP mode)
    ///
    /// @return true if WiFi mode is not NULL, false otherwise
    /// @return false if WiFi component not linked (weak symbol fallback)
    /// @return false if WiFi not initialized or failed to query
    ///
    /// **Platform Support:** ESP32, S2, S3, C3, C6, H2 (not C2)
    static bool isWiFiActive();

    /// @brief Check if WiFi is connected to an access point
    ///
    /// Specifically detects if the device is connected to a WiFi access point
    /// (in Station mode).
    ///
    /// @return true if connected to AP, false otherwise
    /// @return false if WiFi component not linked
    /// @return false if not in Station mode or connection failed
    ///
    /// **Use Case:** More precise detection than isWiFiActive() - only
    /// triggers adaptive behavior when WiFi is actively transmitting.
    static bool isWiFiConnected();

    //=========================================================================
    // Ethernet Detection
    //=========================================================================

    /// @brief Check if Ethernet interface is active (link up)
    ///
    /// Detects if an Ethernet network interface exists and is operational.
    /// Uses ESP-IDF's esp_netif API to enumerate interfaces.
    ///
    /// @return true if Ethernet interface is up, false otherwise
    /// @return false if esp_netif component not linked
    /// @return false if no Ethernet interface found
    ///
    /// **Platform Support:** ESP32 (with external PHY like LAN8720), ESP32-C6
    static bool isEthernetActive();

    /// @brief Check if Ethernet has a valid IP address
    ///
    /// Detects if Ethernet interface is connected and has obtained an IP address.
    ///
    /// @return true if Ethernet has valid IP, false otherwise
    /// @return false if esp_netif component not linked
    /// @return false if no IP address assigned
    ///
    /// **Use Case:** More precise than isEthernetActive() - confirms actual
    /// network connectivity, not just link status.
    static bool isEthernetConnected();

    //=========================================================================
    // Bluetooth Detection
    //=========================================================================

    /// @brief Check if Bluetooth controller is active (enabled)
    ///
    /// Detects if Bluetooth Classic or BLE controller is enabled.
    /// Both Classic BT and BLE use the same controller on ESP32.
    ///
    /// @return true if Bluetooth controller is enabled, false otherwise
    /// @return false if Bluetooth component not linked
    /// @return false if controller is idle or only initialized
    ///
    /// **Platform Support:** ESP32, S3, C3, C6, H2 (not S2)
    ///
    /// **Note:** This detects both Classic Bluetooth and BLE (they share
    /// the same controller). If you need to distinguish between them, use
    /// additional ESP-IDF APIs.
    static bool isBluetoothActive();

    //=========================================================================
    // Convenience Methods (Shorthand)
    //=========================================================================

    /// @brief Check if any network type is active
    ///
    /// Convenience method that returns true if any of WiFi, Ethernet, or
    /// Bluetooth is currently active.
    ///
    /// @return true if any network is active, false if all are inactive
    ///
    /// **Equivalent to:**
    /// @code
    /// isWiFiActive() || isEthernetActive() || isBluetoothActive()
    /// @endcode
    static bool isAnyNetworkActive();

    /// @brief Check if any network type is connected
    ///
    /// Convenience method that returns true if any of WiFi or Ethernet
    /// has an active connection (has IP address).
    ///
    /// @return true if any network is connected, false otherwise
    ///
    /// **Equivalent to:**
    /// @code
    /// isWiFiConnected() || isEthernetConnected()
    /// @endcode
    ///
    /// **Note:** Bluetooth is not included because it doesn't have a
    /// "connected" state in the same sense (no IP address).
    static bool isAnyNetworkConnected();

  private:
    // No instances - static API only
    NetworkDetector() = delete;
    ~NetworkDetector() = delete;
    NetworkDetector(const NetworkDetector &) = delete;
    NetworkDetector &operator=(const NetworkDetector &) = delete;
};

} // namespace fl

#endif // FASTLED_RMT5
