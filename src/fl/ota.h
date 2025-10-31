/// @file ota.h
/// @brief Over-The-Air (OTA) update interface for FastLED
///
/// Provides a minimal, batteries-included OTA update system that works across
/// different platforms (ESP32, ESP8266, and stub for testing).
///
/// Key features:
/// - Simple one-liner initialization
/// - Support for Wi-Fi, Ethernet, or pre-configured network
/// - Built-in mDNS, ArduinoIDE OTA, and Web UI
/// - Authentication via password (MD5 for IDE, Basic Auth for Web)
/// - Mesh distribution support (placeholder for future expansion)

#ifndef FL_OTA_H
#define FL_OTA_H

#include "fl/ptr.h"
#include "fl/string.h"
#include "fl/optional.h"
#include "fl/function.h"

namespace fl {

/// @brief OTA update states
enum class OTAState : uint8_t {
    IDLE = 0,           ///< No update in progress
    STARTING = 1,       ///< Update starting
    IN_PROGRESS = 10,   ///< Update in progress
    SUCCESS = 2,        ///< Update completed successfully
    ABORTED = 254,      ///< Update aborted by user
    FAILED_START = 253, ///< Failed to start update
    FAILED_WRITE = 252, ///< Failed during write
    FAILED_END = 251,   ///< Failed to finalize update
    ERROR = 255         ///< Generic error
};

/// @brief OTA transport types
enum class OTATransport : uint8_t {
    NONE = 0,      ///< No transport configured
    WIFI = 1,      ///< Wi-Fi transport
    ETHERNET = 2,  ///< Ethernet transport
    CUSTOM = 3     ///< Custom/pre-configured transport
};

/// @brief Abstract OTA interface
///
/// This class provides the core OTA update functionality that can be
/// implemented by platform-specific backends (ESP32, ESP8266) or
/// stubbed for testing.
class OTA {
public:
    virtual ~OTA() {}

    // === Transport initialization ===
    
    /// @brief Initialize OTA with Wi-Fi transport
    /// @param hostname Device hostname for mDNS and DHCP
    /// @param ota_password Password for OTA authentication
    /// @param ssid Wi-Fi SSID to connect to
    /// @param psk Wi-Fi password
    /// @param timeout_ms Connection timeout in milliseconds
    /// @return true if initialization succeeded, false otherwise
    virtual bool beginWiFi(
        const char* hostname,
        const char* ota_password,
        const char* ssid,
        const char* psk,
        uint32_t timeout_ms = 12000
    ) = 0;

    /// @brief Initialize OTA with Ethernet transport
    /// @param hostname Device hostname for mDNS and DHCP
    /// @param ota_password Password for OTA authentication
    /// @param timeout_ms Connection timeout in milliseconds
    /// @return true if initialization succeeded, false otherwise
    virtual bool beginEthernet(
        const char* hostname,
        const char* ota_password,
        uint32_t timeout_ms = 12000
    ) = 0;

    /// @brief Initialize OTA with pre-configured network
    /// @param hostname Device hostname for mDNS
    /// @param ota_password Password for OTA authentication
    /// @return true if initialization succeeded, false otherwise
    virtual bool beginNetworkOnly(
        const char* hostname,
        const char* ota_password
    ) = 0;

    // === Mesh placeholders (no-op for now, API ready) ===
    
    /// @brief Initialize as mesh OTA distributor
    /// @param hostname Device hostname
    /// @param ota_password Password for authentication
    /// @return true if initialization succeeded, false otherwise
    virtual bool beginMeshDistributor(
        const char* hostname,
        const char* ota_password
    ) {
        // TODO: implement chunk/relay distributor
        return false;
    }

    /// @brief Initialize as mesh OTA client
    /// @param hostname Device hostname
    /// @return true if initialization succeeded, false otherwise
    virtual bool beginMeshClient(const char* hostname) {
        // TODO: implement client chunk receiver
        return false;
    }

    // === Runtime control ===
    
    /// @brief Poll OTA services (call in loop)
    virtual void poll() = 0;

    // === Feature toggles ===
    
    /// @brief Enable AP fallback mode when Wi-Fi connection fails
    /// @param ssid Access point SSID
    /// @param pass Access point password (can be nullptr for open AP)
    virtual void enableApFallback(const char* ssid, const char* pass = nullptr) = 0;

    /// @brief Disable Web UI OTA interface
    virtual void disableWeb() = 0;

    /// @brief Disable Arduino IDE OTA interface
    virtual void disableArduinoIDE() = 0;

    /// @brief Disable mDNS service
    virtual void disableMDNS() = 0;

    // === Callbacks ===
    
    /// @brief Set progress callback
    /// @param callback Function called with (written_bytes, total_bytes)
    virtual void onProgress(fl::function<void(size_t, size_t)> callback) = 0;

    /// @brief Set error callback
    /// @param callback Function called with error message
    virtual void onError(fl::function<void(const char*)> callback) = 0;

    /// @brief Set state change callback
    /// @param callback Function called with new OTAState
    virtual void onState(fl::function<void(OTAState)> callback) = 0;

    // === Status queries ===
    
    /// @brief Get current OTA state
    virtual OTAState getState() const = 0;

    /// @brief Get configured transport type
    virtual OTATransport getTransport() const = 0;

    /// @brief Get device hostname
    virtual const char* getHostname() const = 0;

    /// @brief Get device IP address (if available)
    virtual fl::optional<fl::string> getIpAddress() const = 0;

    /// @brief Check if OTA services are running
    virtual bool isRunning() const = 0;
};

}  // namespace fl

#endif // FL_OTA_H
