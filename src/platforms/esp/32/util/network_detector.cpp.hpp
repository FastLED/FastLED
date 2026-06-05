// IWYU pragma: private

// Network Detection API Implementation
// Uses weak symbol fallback pattern to gracefully handle builds without
// network components (WiFi, Ethernet, Bluetooth)

#include "platforms/esp/32/util/network_detector.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)

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

#define FL_ESP32_WIFI_CAPABLE_PLATFORM SOC_WIFI_SUPPORTED
#define FL_ESP32_ETHERNET_CAPABLE_PLATFORM SOC_EMAC_SUPPORTED
#define FL_ESP32_BLUETOOTH_CAPABLE_PLATFORM SOC_BT_SUPPORTED

// Bluetooth enabled requires both hardware support AND build config enabled
#ifndef FL_ESP_32_BLUETOOTH_ENABLED
#if FL_ESP32_BLUETOOTH_CAPABLE_PLATFORM && defined(CONFIG_BT_ENABLED)
#define FL_ESP_32_BLUETOOTH_ENABLED 1
#else
#define FL_ESP_32_BLUETOOTH_ENABLED 0
#endif
#endif

//=============================================================================
// ESP-IDF Includes (Platform-Specific)
//=============================================================================

#if FL_ESP32_WIFI_CAPABLE_PLATFORM
FL_EXTERN_C_BEGIN
#include "esp_err.h"
#include "esp_wifi.h"
FL_EXTERN_C_END
#endif

#if FL_ESP32_ETHERNET_CAPABLE_PLATFORM
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

#if FL_ESP32_WIFI_CAPABLE_PLATFORM

FL_EXTERN_C_BEGIN
FL_LINK_WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);
FL_LINK_WEAK esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
FL_EXTERN_C_END

bool NetworkDetector::isWiFiActive() FL_NOEXCEPT {
    if (esp_wifi_get_mode == nullptr) {
        return false;
    }
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        return false;
    }
    return (mode != WIFI_MODE_NULL);
}

bool NetworkDetector::isWiFiConnected() FL_NOEXCEPT {
    if (esp_wifi_sta_get_ap_info == nullptr) {
        return false;
    }
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    return (err == ESP_OK);
}

#else

bool NetworkDetector::isWiFiActive() FL_NOEXCEPT { return false; }
bool NetworkDetector::isWiFiConnected() FL_NOEXCEPT { return false; }

#endif // FL_ESP32_WIFI_CAPABLE_PLATFORM

//=============================================================================
// Ethernet Detection (Ethernet-capable platforms only)
//=============================================================================

#if FL_ESP32_ETHERNET_CAPABLE_PLATFORM

FL_EXTERN_C_BEGIN
FL_LINK_WEAK esp_netif_t* esp_netif_next(esp_netif_t* netif);
FL_LINK_WEAK const char* esp_netif_get_desc(esp_netif_t* esp_netif);
FL_LINK_WEAK bool esp_netif_is_netif_up(esp_netif_t* esp_netif);
FL_LINK_WEAK esp_err_t esp_netif_get_ip_info(esp_netif_t* esp_netif, esp_netif_ip_info_t* ip_info);
FL_EXTERN_C_END

namespace {
    esp_netif_t* findEthernetInterface() FL_NOEXCEPT {
        if (esp_netif_next == nullptr || esp_netif_get_desc == nullptr) {
            return nullptr;
        }
        esp_netif_t* netif = esp_netif_next(nullptr);
        while (netif != nullptr) {
            const char* desc = esp_netif_get_desc(netif);
            if (desc != nullptr) {
                if (fl::strstr(desc, "eth") != nullptr ||
                    fl::strstr(desc, "ETH") != nullptr) {
                    return netif;
                }
            }
            netif = esp_netif_next(netif);
        }
        return nullptr;
    }
}

bool NetworkDetector::isEthernetActive() FL_NOEXCEPT {
    esp_netif_t* eth_netif = findEthernetInterface();
    if (eth_netif == nullptr) {
        return false;
    }
    if (esp_netif_is_netif_up == nullptr) {
        return false;
    }
    return esp_netif_is_netif_up(eth_netif);
}

bool NetworkDetector::isEthernetConnected() FL_NOEXCEPT {
    esp_netif_t* eth_netif = findEthernetInterface();
    if (eth_netif == nullptr) {
        return false;
    }
    if (esp_netif_get_ip_info == nullptr) {
        return false;
    }
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(eth_netif, &ip_info);
    if (err != ESP_OK) {
        return false;
    }
    return (ip_info.ip.addr != 0);
}

#else

bool NetworkDetector::isEthernetActive() FL_NOEXCEPT { return false; }
bool NetworkDetector::isEthernetConnected() FL_NOEXCEPT { return false; }

#endif // FL_ESP32_ETHERNET_CAPABLE_PLATFORM

//=============================================================================
// Bluetooth Detection
//=============================================================================

#if FL_ESP_32_BLUETOOTH_ENABLED

FL_EXTERN_C_BEGIN
FL_LINK_WEAK esp_bt_controller_status_t esp_bt_controller_get_status(void);
FL_EXTERN_C_END

bool NetworkDetector::isBluetoothActive() FL_NOEXCEPT {
    if (esp_bt_controller_get_status == nullptr) {
        return false;
    }
    esp_bt_controller_status_t status = esp_bt_controller_get_status();
    return (status == ESP_BT_CONTROLLER_STATUS_ENABLED);
}

#else

bool NetworkDetector::isBluetoothActive() FL_NOEXCEPT { return false; }

#endif // FL_ESP_32_BLUETOOTH_ENABLED

//=============================================================================
// Convenience Methods
//=============================================================================

bool NetworkDetector::isAnyNetworkActive() FL_NOEXCEPT {
    return isWiFiActive() || isEthernetActive() || isBluetoothActive();
}

bool NetworkDetector::isAnyNetworkConnected() FL_NOEXCEPT {
    return isWiFiConnected() || isEthernetConnected();
}

} // namespace fl

#endif // FL_IS_ESP32
