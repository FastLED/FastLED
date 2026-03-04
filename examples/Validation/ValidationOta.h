// examples/Validation/ValidationOta.h
//
// OTA validation for ESP32: WiFi Soft AP + OTA HTTP server testing.
// Used by `bash validate --ota`.
//
// The ESP32 creates a WiFi Soft AP with known credentials,
// then starts an OTA HTTP server with Basic Auth.
// The host Python script connects to the AP and validates HTTP endpoints.

#pragma once

// WiFi AP configuration constants for OTA validation
#define VALIDATION_OTA_SSID "FastLED-OTA-Test"
#define VALIDATION_OTA_PASSWORD "otavalid8"
#define VALIDATION_OTA_AP_IP "192.168.4.1"
#define VALIDATION_OTA_OTA_PASSWORD "testota123"
#define VALIDATION_OTA_HOSTNAME "fastled-ota-test"
#define VALIDATION_OTA_PORT 80

// Forward declarations
namespace fl {
class json;
}

/// @brief State for OTA validation
struct ValidationOtaState {
    bool wifi_ap_active = false;
    bool ota_active = false;
};

/// @brief Start WiFi Soft AP and OTA HTTP server.
/// @return JSON with {success, ssid, password, ip, port, ota_password, hostname}
///         on success, or {success: false, error} on failure.
fl::json startOta();

/// @brief Stop OTA server and WiFi AP, release all resources.
/// @return JSON with {success: true}
fl::json stopOta();

/// @brief Get current OTA validation state.
/// @return Reference to the global OTA state
ValidationOtaState& getOtaState();
