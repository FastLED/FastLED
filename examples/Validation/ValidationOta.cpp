// examples/Validation/ValidationOta.cpp
//
// OTA validation implementation for ESP32.
// Uses fl::OTA for the OTA HTTP server with Basic Auth.
// Uses ESP-IDF native APIs for WiFi Soft AP.
// Guarded with FL_IS_ESP32 - no-op stubs on other platforms.

#include "ValidationOta.h"
#include "fl/stl/json.h"
#include "fl/system/log.h"

// Global OTA state
static ValidationOtaState s_ota_state;

ValidationOtaState& getOtaState() {
    return s_ota_state;
}

// ============================================================================
// ESP32 Implementation
// ============================================================================

#if defined(FL_IS_ESP32)

#include "fl/ota.h"
#include "fl/stl/cstring.h"
#include "fl/stl/unique_ptr.h"
#include <Arduino.h>

// ESP-IDF headers for WiFi AP
// IWYU pragma: begin_keep
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
// IWYU pragma: end_keep

// Static handles
static fl::unique_ptr<fl::OTA> s_ota;
static esp_netif_t* s_ota_netif_ap = nullptr;
static bool s_ota_event_loop_initialized = false;
static bool s_ota_wifi_initialized = false;

// ============================================================================
// WiFi Soft AP Setup (same pattern as ValidationNet.cpp::initWifiAP)
// ============================================================================

static bool initOtaWifiAP() {
    if (s_ota_state.wifi_ap_active) {
        return true;  // Already active
    }

    // Initialize TCP/IP stack (safe to call multiple times)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN("[OTA] esp_netif_init failed: " << esp_err_to_name(err));
        return false;
    }

    // Create default event loop (safe to call multiple times)
    if (!s_ota_event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[OTA] esp_event_loop_create_default failed: " << esp_err_to_name(err));
            return false;
        }
        s_ota_event_loop_initialized = true;
    }

    // Create default WiFi AP netif
    if (!s_ota_netif_ap) {
        s_ota_netif_ap = esp_netif_create_default_wifi_ap();
        if (!s_ota_netif_ap) {
            FL_WARN("[OTA] esp_netif_create_default_wifi_ap failed");
            return false;
        }
    }

    // Initialize WiFi with default config
    if (!s_ota_wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[OTA] esp_wifi_init failed: " << esp_err_to_name(err));
            return false;
        }
        s_ota_wifi_initialized = true;
    }

    // Configure AP
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, VALIDATION_OTA_SSID, strlen(VALIDATION_OTA_SSID));
    wifi_config.ap.ssid_len = strlen(VALIDATION_OTA_SSID);
    memcpy(wifi_config.ap.password, VALIDATION_OTA_PASSWORD, strlen(VALIDATION_OTA_PASSWORD));
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.channel = 1;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        FL_WARN("[OTA] esp_wifi_set_mode failed: " << esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        FL_WARN("[OTA] esp_wifi_set_config failed: " << esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        FL_WARN("[OTA] esp_wifi_start failed: " << esp_err_to_name(err));
        return false;
    }

    s_ota_state.wifi_ap_active = true;
    FL_WARN("[OTA] WiFi AP started: SSID=" << VALIDATION_OTA_SSID << " IP=" << VALIDATION_OTA_AP_IP);
    return true;
}

// ============================================================================
// Public API
// ============================================================================

fl::json startOta() {
    fl::json response = fl::json::object();

    // Start WiFi AP
    if (!initOtaWifiAP()) {
        response.set("success", false);
        response.set("error", "Failed to start WiFi AP for OTA");
        return response;
    }

    // Create and start OTA server
    s_ota = fl::make_unique<fl::OTA>();
    if (!s_ota->begin(VALIDATION_OTA_HOSTNAME, VALIDATION_OTA_OTA_PASSWORD)) {
        response.set("success", false);
        response.set("error", "Failed to start OTA server");
        s_ota.reset();
        return response;
    }

    s_ota_state.ota_active = true;
    FL_WARN("[OTA] OTA server started: hostname=" << VALIDATION_OTA_HOSTNAME);

    response.set("success", true);
    response.set("ssid", VALIDATION_OTA_SSID);
    response.set("password", VALIDATION_OTA_PASSWORD);
    response.set("ip", VALIDATION_OTA_AP_IP);
    response.set("port", static_cast<int64_t>(VALIDATION_OTA_PORT));
    response.set("ota_password", VALIDATION_OTA_OTA_PASSWORD);
    response.set("hostname", VALIDATION_OTA_HOSTNAME);
    return response;
}

fl::json stopOta() {
    fl::json response = fl::json::object();

    // Stop OTA server
    if (s_ota.get()) {
        s_ota.reset();
        s_ota_state.ota_active = false;
        FL_WARN("[OTA] OTA server stopped");
    }

    // Stop WiFi
    if (s_ota_state.wifi_ap_active) {
        esp_wifi_stop();
        s_ota_state.wifi_ap_active = false;
        FL_WARN("[OTA] WiFi AP stopped");
    }

    response.set("success", true);
    return response;
}

#else  // !FL_IS_ESP32

// ============================================================================
// Stub Implementation for Non-ESP32 Platforms
// ============================================================================

fl::json startOta() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "OTA validation only supported on ESP32");
    return response;
}

fl::json stopOta() {
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}

#endif  // FL_IS_ESP32
