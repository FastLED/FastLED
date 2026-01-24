// Network Detection API Implementation
// Uses weak symbol fallback pattern to gracefully handle builds without
// network components (WiFi, Ethernet, Bluetooth)

#include "network_detector.h"

#if FASTLED_RMT5

#include "fl/compiler_control.h"
#include "fl/stl/cstring.h"

// Use ESP-IDF SoC capability macros to detect hardware features
// These are defined in soc/soc_caps.h for each chip variant
FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
FL_EXTERN_C_END

// Platform capability detection using SoC macros
#ifndef SOC_WIFI_SUPPORTED
#define SOC_WIFI_SUPPORTED 0
#endif

#ifndef SOC_EMAC_SUPPORTED
#define SOC_EMAC_SUPPORTED 0
#endif

#ifndef SOC_BT_SUPPORTED
#define SOC_BT_SUPPORTED 0
#endif

// Map SoC capabilities to FastLED feature flags
#define FASTLED_RMT_WIFI_CAPABLE_PLATFORM SOC_WIFI_SUPPORTED
#define FASTLED_RMT_ETHERNET_CAPABLE_PLATFORM SOC_EMAC_SUPPORTED
#define FASTLED_RMT_BLUETOOTH_CAPABLE_PLATFORM SOC_BT_SUPPORTED

// Bluetooth enabled requires both hardware support AND build config enabled
#ifndef FL_ESP_32_BLUETOOTH_ENABLED
#if FASTLED_RMT_BLUETOOTH_CAPABLE_PLATFORM && defined(CONFIG_BT_ENABLED)
#define FL_ESP_32_BLUETOOTH_ENABLED 1
#else
#define FL_ESP_32_BLUETOOTH_ENABLED 0
#endif
#endif

//=============================================================================
// ESP-IDF Includes (Platform-Specific)
//=============================================================================

#if FASTLED_RMT_WIFI_CAPABLE_PLATFORM
FL_EXTERN_C_BEGIN
#include "esp_err.h"
#include "esp_wifi.h"
FL_EXTERN_C_END
#endif

#if FASTLED_RMT_ETHERNET_CAPABLE_PLATFORM
FL_EXTERN_C_BEGIN
#include "esp_netif.h"
FL_EXTERN_C_END
#endif

#if FL_ESP_32_BLUETOOTH_ENABLED
FL_EXTERN_C_BEGIN
#include "esp_bt.h"
FL_EXTERN_C_END
#endif
namespace fl {


//=============================================================================
// WiFi Detection (WiFi-capable platforms only)
//=============================================================================

#if FASTLED_RMT_WIFI_CAPABLE_PLATFORM

// Weak symbol declarations for WiFi functions
FL_EXTERN_C_BEGIN

FL_LINK_WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);
FL_LINK_WEAK esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);

FL_EXTERN_C_END

bool NetworkDetector::isWiFiActive() {
    // Check if WiFi function is linked (weak symbol resolution)
    if (esp_wifi_get_mode == nullptr) {
        return false;  // WiFi component not linked
    }

    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);

    if (err != ESP_OK) {
        return false;  // WiFi not initialized or query failed
    }

    // WiFi is active if mode is not NULL
    return (mode != WIFI_MODE_NULL);
}

bool NetworkDetector::isWiFiConnected() {
    // Check if WiFi function is linked (weak symbol resolution)
    if (esp_wifi_sta_get_ap_info == nullptr) {
        return false;  // WiFi component not linked
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

    // ESP_OK means connected to AP
    return (err == ESP_OK);
}

#else  // !FASTLED_RMT_WIFI_CAPABLE_PLATFORM

// Non-WiFi platforms (ESP32-C2, etc.) - stub implementations
bool NetworkDetector::isWiFiActive() {
    return false;  // No WiFi hardware
}

bool NetworkDetector::isWiFiConnected() {
    return false;  // No WiFi hardware
}

#endif  // FASTLED_RMT_WIFI_CAPABLE_PLATFORM

//=============================================================================
// Ethernet Detection (Ethernet-capable platforms only)
//=============================================================================

#if FASTLED_RMT_ETHERNET_CAPABLE_PLATFORM

// Weak symbol declarations for esp_netif functions
FL_EXTERN_C_BEGIN

FL_LINK_WEAK esp_netif_t* esp_netif_next(esp_netif_t* netif);
FL_LINK_WEAK const char* esp_netif_get_desc(esp_netif_t* esp_netif);
FL_LINK_WEAK bool esp_netif_is_netif_up(esp_netif_t* esp_netif);
FL_LINK_WEAK esp_err_t esp_netif_get_ip_info(esp_netif_t* esp_netif, esp_netif_ip_info_t* ip_info);

FL_EXTERN_C_END

namespace {
    /// @brief Helper function to find Ethernet interface
    /// @return Pointer to Ethernet netif, or nullptr if not found
    esp_netif_t* findEthernetInterface() {
        if (esp_netif_next == nullptr || esp_netif_get_desc == nullptr) {
            return nullptr;  // esp_netif component not linked
        }

        // Enumerate all network interfaces
        esp_netif_t* netif = esp_netif_next(nullptr);  // Get first interface
        while (netif != nullptr) {
            const char* desc = esp_netif_get_desc(netif);
            if (desc != nullptr) {
                // Check if this is an Ethernet interface
                // Typical descriptions: "eth", "ethernet", "ETH_DEF"
                if (fl::strstr(desc, "eth") != nullptr ||
                    fl::strstr(desc, "ETH") != nullptr) {
                    return netif;
                }
            }
            netif = esp_netif_next(netif);  // Next interface
        }

        return nullptr;  // No Ethernet interface found
    }
}

bool NetworkDetector::isEthernetActive() {
    esp_netif_t* eth_netif = findEthernetInterface();
    if (eth_netif == nullptr) {
        return false;  // No Ethernet interface
    }

    // Check if interface is up (weak symbol check)
    if (esp_netif_is_netif_up == nullptr) {
        return false;  // Function not linked
    }

    return esp_netif_is_netif_up(eth_netif);
}

bool NetworkDetector::isEthernetConnected() {
    esp_netif_t* eth_netif = findEthernetInterface();
    if (eth_netif == nullptr) {
        return false;  // No Ethernet interface
    }

    // Check if esp_netif_get_ip_info is linked
    if (esp_netif_get_ip_info == nullptr) {
        return false;  // Function not linked
    }

    // Get IP info to check if we have a valid IP address
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(eth_netif, &ip_info);

    if (err != ESP_OK) {
        return false;  // Failed to get IP info
    }

    // Check if we have a non-zero IP address (not 0.0.0.0)
    return (ip_info.ip.addr != 0);
}

#else  // !FASTLED_RMT_ETHERNET_CAPABLE_PLATFORM

// Non-Ethernet platforms - stub implementations
bool NetworkDetector::isEthernetActive() {
    return false;  // No Ethernet hardware
}

bool NetworkDetector::isEthernetConnected() {
    return false;  // No Ethernet hardware
}

#endif  // FASTLED_RMT_ETHERNET_CAPABLE_PLATFORM

//=============================================================================
// Bluetooth Detection (Bluetooth-capable platforms only)
//=============================================================================

#if FL_ESP_32_BLUETOOTH_ENABLED

// Weak symbol declaration for Bluetooth controller function
FL_EXTERN_C_BEGIN

FL_LINK_WEAK esp_bt_controller_status_t esp_bt_controller_get_status(void);

FL_EXTERN_C_END

bool NetworkDetector::isBluetoothActive() {
    // Check if Bluetooth function is linked (weak symbol resolution)
    if (esp_bt_controller_get_status == nullptr) {
        return false;  // Bluetooth component not linked
    }

    esp_bt_controller_status_t status = esp_bt_controller_get_status();

    // Controller is active only when enabled (not just initialized)
    return (status == ESP_BT_CONTROLLER_STATUS_ENABLED);
}

#else  // !FL_ESP_32_BLUETOOTH_ENABLED

// Non-Bluetooth platforms (ESP32-S2) or Bluetooth disabled in build config - stub implementation
bool NetworkDetector::isBluetoothActive() {
    return false;  // No Bluetooth hardware or Bluetooth disabled in project configuration
}

#endif  // FL_ESP_32_BLUETOOTH_ENABLED

//=============================================================================
// Convenience Methods (All Platforms)
//=============================================================================

bool NetworkDetector::isAnyNetworkActive() {
    return isWiFiActive() || isEthernetActive() || isBluetoothActive();
}

bool NetworkDetector::isAnyNetworkConnected() {
    return isWiFiConnected() || isEthernetConnected();
}

} // namespace fl

#endif  // FASTLED_RMT5
