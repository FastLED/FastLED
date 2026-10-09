// IWYU pragma: private

// Network Detection API Implementation
// Uses weak symbol fallback pattern to gracefully handle builds without
// network components (WiFi, Ethernet, Bluetooth)

#include "platforms/esp/32/drivers/rmt/rmt_5/network_detector.h"

#if FASTLED_RMT5

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

// Use ESP-IDF SoC capability macros to detect hardware features
// These are defined in soc/soc_caps.h for each chip variant
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
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
#include "esp_netif.h"
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

// Weak symbol declarations for the esp_netif functions used to detect Wi-Fi.
//
// Wi-Fi is detected through esp_netif, never through esp_wifi_* (#4791). On
// classic ESP32 Arduino the core's Bluetooth/PHY init already extracts
// libnet80211's ieee80211_api.o, so a weak esp_wifi_get_mode reference always
// resolves; merely referencing it keeps ~1.3 KB of Wi-Fi BSS (s_wifi_nvs) and
// its call chain alive in every sketch. libesp_netif's objects are only
// extracted when the application brings up a network stack (Arduino WiFi,
// fl::wifi, IDF esp_netif_create_default_wifi_*), so these stay null and cost
// nothing in sketches without networking.
FL_EXTERN_C_BEGIN

FL_LINK_WEAK esp_netif_t* esp_netif_get_handle_from_ifkey(const char* if_key);
FL_LINK_WEAK esp_netif_t* esp_netif_next(esp_netif_t* netif);
FL_LINK_WEAK const char* esp_netif_get_desc(esp_netif_t* esp_netif);
FL_LINK_WEAK bool esp_netif_is_netif_up(esp_netif_t* esp_netif);
FL_LINK_WEAK esp_err_t esp_netif_get_ip_info(esp_netif_t* esp_netif, esp_netif_ip_info_t* ip_info);

FL_EXTERN_C_END

namespace {
esp_netif_t* wifiNetif(const char* key) FL_NO_EXCEPT {
    if (esp_netif_get_handle_from_ifkey == nullptr) {
        return nullptr;  // esp_netif component not linked
    }
    return esp_netif_get_handle_from_ifkey(key);
}

/// Any interface that is not Ethernet: covers Wi-Fi on a custom (non-default)
/// netif. May also count PPP/Thread interfaces, which is harmless because
/// callers only use this to pick the network-safe configuration.
bool anyNonEthernetNetif() FL_NO_EXCEPT {
    if (esp_netif_next == nullptr || esp_netif_get_desc == nullptr) {
        return false;
    }
    for (esp_netif_t* n = esp_netif_next(nullptr); n != nullptr;
         n = esp_netif_next(n)) {
        const char* desc = esp_netif_get_desc(n);
        if (desc == nullptr || (fl::strstr(desc, "eth") == nullptr &&
                                fl::strstr(desc, "ETH") == nullptr)) {
            return true;
        }
    }
    return false;
}
}  // namespace

bool NetworkDetector::isWiFiActive() FL_NO_EXCEPT {
    // A default Wi-Fi STA or AP interface exists once Wi-Fi has been
    // initialized (Arduino WiFi, fl::wifi and IDF default handlers all create
    // one); custom Wi-Fi netifs are caught by the non-Ethernet scan.
    // Conservative by design: reporting "active" only selects the
    // network-safe RMT configuration. Limitation: raw esp_wifi_start() with
    // no netif at all (e.g. bare-IDF ESP-NOW) is not detected.
    return wifiNetif("WIFI_STA_DEF") != nullptr ||
           wifiNetif("WIFI_AP_DEF") != nullptr || anyNonEthernetNetif();
}

bool NetworkDetector::isWiFiConnected() FL_NO_EXCEPT {
    esp_netif_t* sta = wifiNetif("WIFI_STA_DEF");
    if (sta == nullptr || esp_netif_is_netif_up == nullptr ||
        esp_netif_get_ip_info == nullptr || !esp_netif_is_netif_up(sta)) {
        return false;
    }
    esp_netif_ip_info_t ip_info;
    return esp_netif_get_ip_info(sta, &ip_info) == ESP_OK &&
           ip_info.ip.addr != 0;
}

#else  // !FASTLED_RMT_WIFI_CAPABLE_PLATFORM

// Non-WiFi platforms (ESP32-C2, etc.) - stub implementations
bool NetworkDetector::isWiFiActive() FL_NO_EXCEPT {
    return false;  // No WiFi hardware
}

bool NetworkDetector::isWiFiConnected() FL_NO_EXCEPT {
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
    esp_netif_t* findEthernetInterface() FL_NO_EXCEPT {
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

bool NetworkDetector::isEthernetActive() FL_NO_EXCEPT {
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

bool NetworkDetector::isEthernetConnected() FL_NO_EXCEPT {
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
bool NetworkDetector::isEthernetActive() FL_NO_EXCEPT {
    return false;  // No Ethernet hardware
}

bool NetworkDetector::isEthernetConnected() FL_NO_EXCEPT {
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

bool NetworkDetector::isBluetoothActive() FL_NO_EXCEPT {
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
bool NetworkDetector::isBluetoothActive() FL_NO_EXCEPT {
    return false;  // No Bluetooth hardware or Bluetooth disabled in project configuration
}

#endif  // FL_ESP_32_BLUETOOTH_ENABLED

//=============================================================================
// Convenience Methods (All Platforms)
//=============================================================================

bool NetworkDetector::isAnyNetworkActive() FL_NO_EXCEPT {
    return isWiFiActive() || isEthernetActive() || isBluetoothActive();
}

bool NetworkDetector::isAnyNetworkConnected() FL_NO_EXCEPT {
    return isWiFiConnected() || isEthernetConnected();
}

} // namespace fl

#endif  // FASTLED_RMT5
