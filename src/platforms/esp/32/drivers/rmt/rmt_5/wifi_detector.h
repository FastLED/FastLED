#pragma once

// WiFi Detection API for RMT WiFi-Aware Dynamic Configuration
// Provides runtime WiFi state detection with graceful fallback for builds
// without WiFi component linked.

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"

/// @brief WiFi state detector for adaptive RMT channel management
///
/// This class provides runtime detection of WiFi activity to enable
/// adaptive RMT channel configuration. It uses weak symbol fallback
/// to gracefully handle builds where the WiFi component is not linked.
///
/// Usage:
/// @code
/// if (WiFiDetector::isWiFiActive()) {
///     // WiFi is enabled - use robust configuration
/// } else {
///     // WiFi inactive - maximize channel count
/// }
/// @endcode
///
/// Platform Support:
/// - ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2: Full support
/// - ESP32-C2: Returns false (no WiFi hardware)
/// - Other platforms: Returns false (no WiFi support)
///
/// Graceful Fallback:
/// If WiFi component not linked, all methods return false (no crashes).
class WiFiDetector {
  public:
    /// @brief Check if WiFi is currently active (any mode except NULL)
    ///
    /// This method detects if WiFi is in any active mode:
    /// - WIFI_MODE_STA (Station mode - connecting to AP)
    /// - WIFI_MODE_AP (Access Point mode)
    /// - WIFI_MODE_APSTA (Both Station and AP mode)
    ///
    /// @return true if WiFi mode is not NULL, false otherwise
    /// @return false if WiFi component not linked (weak symbol fallback)
    /// @return false if WiFi not initialized or failed to query
    ///
    /// Performance: ~1-5µs per call (ESP-IDF API overhead)
    static bool isWiFiActive();

    /// @brief Check if WiFi is connected to an access point
    ///
    /// This method specifically detects if the device is connected
    /// to a WiFi access point (in Station mode).
    ///
    /// @return true if connected to AP, false otherwise
    /// @return false if WiFi component not linked
    /// @return false if not in Station mode or connection failed
    ///
    /// Use Case: More precise detection than isWiFiActive() - only
    /// triggers adaptive behavior when WiFi is actively transmitting.
    static bool isWiFiConnected();

  private:
    // No instances - static API only
    WiFiDetector() = delete;
    ~WiFiDetector() = delete;
    WiFiDetector(const WiFiDetector &) = delete;
    WiFiDetector &operator=(const WiFiDetector &) = delete;
};

#endif // FASTLED_RMT5
