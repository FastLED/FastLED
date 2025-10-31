/// @file fl/ota.h
/// Minimal, batteries-included OTA (Over-The-Air) update system for ESP32
///
/// This module provides a simple, one-liner API for enabling OTA updates on ESP32
/// devices via Wi-Fi or Ethernet. It supports both Arduino IDE OTA (port 3232) and
/// a web-based update interface at the root path "/".
///
/// Key features:
/// - One-liner setup: `beginWiFi()`, `beginEthernet()`, or `begin()`
/// - Arduino IDE OTA support with MD5 password authentication
/// - Web-based OTA UI with Basic Auth (username: "admin")
/// - Automatic mDNS hostname registration for discovery
/// - Optional AP fallback mode for Wi-Fi connection failures
/// - Progress/error/state callbacks for monitoring
/// - Low polling overhead: ~10-73µs when idle (<0.5% at 60 FPS)
///
/// Performance characteristics:
/// - Web OTA has ZERO polling overhead (runs in separate FreeRTOS task)
/// - Arduino IDE OTA: ~10-50µs (native) or ~73µs (ArduinoOTA fallback)
/// - Safe to call `poll()` every loop iteration for LED animations
///
/// Architecture:
/// - Built on ESP-IDF native APIs (minimal Arduino dependency)
/// - Uses esp_http_server.h for async Web OTA (separate task)
/// - Uses esp_wifi.h / esp_eth.h for network transport
/// - Uses mdns.h for service discovery
/// - Future-proof: designed for ESP-IDF component migration
///
/// Platform support:
/// - ESP32 (all variants): Full feature set
/// - ESP8266: Reduced feature set (no Ethernet support)
/// - Other platforms: Compile-time stubs (no-op)
///
/// Security notes:
/// - Arduino IDE OTA uses MD5 hash of password
/// - Web UI uses HTTP Basic Auth (plaintext over HTTP)
/// - Recommended: Use only on trusted networks or with HTTPS
///
/// Hardware considerations:
/// - OTA flash writes consume ~100-200mA on ESP32 (brief spikes to 300mA)
/// - LED arrays can draw significant current during animations
/// - Power supply recommendation: 5V 2A+ for ESP32 + moderate LED count
/// - During OTA update: Reduce LED brightness or count to prevent brownouts
/// - Use quality USB cable and power source (cheap cables may cause voltage drop)
/// - Optional: Add 100-1000µF capacitor between VIN/3.3V and GND for stability
/// - ESP32 variants with external antenna: Verify antenna is properly connected
/// - Wi-Fi performance: ESP32 only supports 2.4GHz band (not 5GHz)
///
/// Example usage:
/// @code
/// #include <FastLED.h>
/// #include "fl/ota.h"
///
/// fl::OTA ota;
///
/// void setup() {
///   // Option 1: Full Wi-Fi setup + OTA
///   ota.beginWiFi("my-device", "password", "MySSID", "wifi-pass");
///
///   // Option 2: Ethernet setup + OTA (ESP32 internal EMAC)
///   // ota.beginEthernet("my-device", "password");
///
///   // Option 3: OTA only (network already configured)
///   // ota.begin("my-device", "password");
///
///   // Optional: Set callbacks
///   ota.onProgress([](size_t written, size_t total) {
///     Serial.printf("Progress: %u/%u\n", written, total);
///   });
/// }
///
/// void loop() {
///   ota.poll();  // Low overhead: ~10-73µs when idle
///   // ... your LED animation code ...
/// }
/// @endcode
///
/// References:
/// - ESP-IDF HTTP Server: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
/// - ESP-IDF OTA: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html

#pragma once

#include "fl/function.h"
#include "fl/shared_ptr.h"

// Note: OTA implementation uses Arduino/ESP-IDF standard types
// No FastLED type headers needed here to avoid conflicts

namespace fl {

// Forward declaration
namespace platforms {
    class IOTA;
}

/// @brief OTA (Over-The-Air) update manager for ESP32 platforms
///
/// Provides a unified interface for enabling OTA firmware updates via
/// Arduino IDE (port 3232) and web browser (HTTP POST at "/").
/// Handles network setup (Wi-Fi/Ethernet), mDNS registration, and
/// authentication automatically.
class OTA {
public:
    /// @brief Default constructor (lazy initialization)
    OTA();

    /// @brief Destructor - cleans up resources
    ~OTA();

    // ========== Network Setup Methods ==========

    /// @brief Start OTA with full Wi-Fi setup (station mode)
    /// @param hostname Device hostname (used for mDNS and DHCP)
    /// @param password Password for OTA authentication (MD5 hashed for Arduino IDE OTA, plaintext for Web UI Basic Auth)
    /// @param ssid Wi-Fi network SSID
    /// @param wifi_pass Wi-Fi network password
    /// @return true if setup successful, false otherwise
    bool beginWiFi(const char* hostname, const char* password,
                   const char* ssid, const char* wifi_pass);

    /// @brief Start OTA with Ethernet setup (ESP32 internal EMAC)
    /// @param hostname Device hostname (used for mDNS and DHCP)
    /// @param password Password for OTA authentication
    /// @return true if setup successful, false otherwise
    /// @note For external Ethernet chips (W5500/ENC28J60), call your Ethernet.begin() first, then use begin()
    bool beginEthernet(const char* hostname, const char* password);

    /// @brief Start OTA services only (network already configured)
    /// @param hostname Device hostname (used for mDNS)
    /// @param password Password for OTA authentication
    /// @return true if setup successful, false otherwise
    /// @note Use this when you've already configured Wi-Fi or Ethernet yourself
    bool begin(const char* hostname, const char* password);

    // ========== Optional Configuration ==========

    /// @brief Enable AP (Access Point) fallback mode if Wi-Fi STA connection fails
    /// @param ap_ssid Access Point SSID
    /// @param ap_pass Access Point password (minimum 8 characters, use nullptr for open AP)
    /// @note Must be called before beginWiFi(). Only applies to Wi-Fi mode.
    void enableApFallback(const char* ap_ssid, const char* ap_pass = nullptr);

    // ========== Callback Registration ==========

    /// @brief Callback function type for OTA progress updates
    /// @param written Bytes written so far
    /// @param total Total bytes to write
    using ProgressCallback = fl::function<void(size_t written, size_t total)>;

    /// @brief Callback function type for OTA errors
    /// @param message Error message string
    using ErrorCallback = fl::function<void(const char* message)>;

    /// @brief Callback function type for OTA state changes
    /// @param state New state code (platform-specific)
    using StateCallback = fl::function<void(uint8_t state)>;

    /// @brief Set progress callback (called during firmware upload)
    /// @param callback Callback function (supports lambdas, function pointers, functors)
    void onProgress(ProgressCallback callback);

    /// @brief Set error callback (called on OTA errors)
    /// @param callback Callback function (supports lambdas, function pointers, functors)
    void onError(ErrorCallback callback);

    /// @brief Set state callback (called on state transitions)
    /// @param callback Callback function (supports lambdas, function pointers, functors)
    void onState(StateCallback callback);

    // ========== Runtime Methods ==========

    /// @brief Poll OTA handlers (must be called regularly in loop())
    /// @note Low overhead: ~10-73µs when idle. Web OTA runs in separate task (zero overhead).
    void poll();



private:
    // Platform-specific implementation (lazy initialized via shared_ptr)
    // Using shared_ptr for proper lifetime management and lazy initialization
    fl::shared_ptr<fl::platforms::IOTA> impl_;
};

}  // namespace fl
