/// @file platforms/ota.h
/// Platform interface for OTA (Over-The-Air) firmware updates
///
/// This header defines the IOTA interface that platform-specific implementations
/// must conform to. Platform implementations override the weak platform_create_ota()
/// function to provide their concrete implementation.

#pragma once

#include "fl/stdint.h"
#include "fl/function.h"
#include "fl/shared_ptr.h"
#include "fl/compiler_control.h"

namespace fl {
namespace platforms {

/// @brief Platform interface for OTA (Over-The-Air) update functionality
///
/// Platform-specific implementations must inherit from this interface and
/// provide concrete implementations of all virtual methods.
class IOTA {
public:
    virtual ~IOTA() = default;

    /// @brief Factory method to create platform-specific OTA instance
    /// @return Shared pointer to IOTA implementation (may be null/no-op on unsupported platforms)
    static fl::shared_ptr<IOTA> create();

    // ========== Network Setup Methods ==========

    /// @brief Start OTA with full Wi-Fi setup (station mode)
    /// @param hostname Device hostname (used for mDNS and DHCP)
    /// @param password Password for OTA authentication
    /// @param ssid Wi-Fi network SSID
    /// @param wifi_pass Wi-Fi network password
    /// @return true if setup successful, false otherwise
    virtual bool beginWiFi(const char* hostname, const char* password,
                          const char* ssid, const char* wifi_pass) = 0;

    /// @brief Start OTA with Ethernet setup
    /// @param hostname Device hostname (used for mDNS and DHCP)
    /// @param password Password for OTA authentication
    /// @return true if setup successful, false otherwise
    virtual bool beginEthernet(const char* hostname, const char* password) = 0;

    /// @brief Start OTA services only (network already configured)
    /// @param hostname Device hostname (used for mDNS)
    /// @param password Password for OTA authentication
    /// @return true if setup successful, false otherwise
    virtual bool begin(const char* hostname, const char* password) = 0;

    // ========== Optional Configuration ==========

    /// @brief Enable AP (Access Point) fallback mode if Wi-Fi STA connection fails
    /// @param ap_ssid Access Point SSID
    /// @param ap_pass Access Point password (nullptr for open AP)
    virtual void enableApFallback(const char* ap_ssid, const char* ap_pass = nullptr) = 0;

    // ========== Callback Registration ==========

    /// @brief Set progress callback (called during firmware upload)
    /// @param callback Callback function (written, total)
    virtual void onProgress(fl::function<void(size_t, size_t)> callback) = 0;

    /// @brief Set error callback (called on OTA errors)
    /// @param callback Callback function (error message)
    virtual void onError(fl::function<void(const char*)> callback) = 0;

    /// @brief Set state callback (called on state transitions)
    /// @param callback Callback function (state code)
    virtual void onState(fl::function<void(uint8_t)> callback) = 0;

    // ========== Runtime Methods ==========

    /// @brief Poll OTA handlers (must be called regularly in loop())
    virtual void poll() = 0;
};

// Platform-specific factory function - can be overridden via strong linkage
fl::shared_ptr<IOTA> platform_create_ota() FL_LINK_WEAK;

}  // namespace platforms
}  // namespace fl
