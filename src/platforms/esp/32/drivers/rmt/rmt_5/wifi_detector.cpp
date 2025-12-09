// WiFi Detection API Implementation
// Uses weak symbol fallback pattern to gracefully handle builds without WiFi
// component

#include "wifi_detector.h"

#if FASTLED_RMT5

#include "fl/compiler_control.h"

// Platform detection - only WiFi-capable platforms
// Note: ESP32-C2 doesn't have WiFi, so we exclude it
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) ||  \
    defined(CONFIG_IDF_TARGET_ESP32S3) ||                                      \
    defined(CONFIG_IDF_TARGET_ESP32C3) ||                                      \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#define FASTLED_RMT_WIFI_CAPABLE_PLATFORM 1
#else
#define FASTLED_RMT_WIFI_CAPABLE_PLATFORM 0
#endif

// Only include WiFi headers on WiFi-capable platforms
#if FASTLED_RMT_WIFI_CAPABLE_PLATFORM

FL_EXTERN_C_BEGIN
#include "esp_err.h"
#include "esp_wifi.h"
FL_EXTERN_C_END
#endif

namespace fl {

#if FASTLED_RMT_WIFI_CAPABLE_PLATFORM

// Weak symbol declarations for WiFi functions
// These allow compilation even if WiFi component is not linked
// If WiFi component IS linked, the real implementations will be used
// If WiFi component NOT linked, these weak symbols will be nullptr at runtime

FL_EXTERN_C_BEGIN

// Weak symbol: esp_wifi_get_mode
// Returns ESP_OK and fills mode parameter if WiFi initialized
// Returns error code if WiFi not initialized or failed
FL_LINK_WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);

// Weak symbol: esp_wifi_sta_get_ap_info
// Returns ESP_OK and fills ap_info if connected to AP
// Returns error code if not connected or failed
FL_LINK_WEAK esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);

FL_EXTERN_C_END

bool WiFiDetector::isWiFiActive() {
    // Check if WiFi function is linked (weak symbol resolution)
    if (esp_wifi_get_mode == nullptr) {
        // WiFi component not linked - graceful fallback
        return false;
    }

    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);

    if (err != ESP_OK) {
        // WiFi not initialized or query failed
        return false;
    }

    // WiFi is active if mode is not NULL
    // WIFI_MODE_NULL = 0 (WiFi disabled)
    // WIFI_MODE_STA = 1 (Station mode)
    // WIFI_MODE_AP = 2 (Access Point mode)
    // WIFI_MODE_APSTA = 3 (Both Station and AP)
    return (mode != WIFI_MODE_NULL);
}

bool WiFiDetector::isWiFiConnected() {
    // Check if WiFi function is linked (weak symbol resolution)
    if (esp_wifi_sta_get_ap_info == nullptr) {
        // WiFi component not linked - graceful fallback
        return false;
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

    // ESP_OK means connected to AP
    // ESP_ERR_WIFI_NOT_INIT, ESP_ERR_WIFI_CONN, etc. mean not connected
    return (err == ESP_OK);
}

#else // !FASTLED_RMT_WIFI_CAPABLE_PLATFORM

// Non-WiFi platforms (ESP32-C2, etc.) - stub implementations
bool WiFiDetector::isWiFiActive() {
    return false; // No WiFi hardware
}

bool WiFiDetector::isWiFiConnected() {
    return false; // No WiFi hardware
}

#endif // FASTLED_RMT_WIFI_CAPABLE_PLATFORM

} // namespace fl

#endif // FASTLED_RMT5
