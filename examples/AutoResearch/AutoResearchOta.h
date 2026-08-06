// examples/AutoResearch/AutoResearchOta.h
//
// OTA autoresearch for ESP32: WiFi Soft AP + OTA HTTP server testing.
// Used by `bash autoresearch --ota`.
//
// The ESP32 creates a WiFi Soft AP with known credentials,
// then starts an OTA HTTP server with Basic Auth.
// The host Python script connects to the AP and tests HTTP endpoints.

#pragma once

#include "fl/stl/vector.h"

// WiFi AP configuration constants for OTA autoresearch
#define AUTORESEARCH_OTA_SSID "FastLED-OTA-Test"
#define AUTORESEARCH_OTA_PASSWORD "otavalid8"
#define AUTORESEARCH_OTA_AP_IP "192.168.4.1"
#define AUTORESEARCH_OTA_OTA_PASSWORD "testota123"
#define AUTORESEARCH_OTA_HOSTNAME "fastled-ota-test"
#define AUTORESEARCH_OTA_PORT 80

// Forward declarations
namespace fl {
class json;
}

/// @brief State for OTA autoresearch
struct AutoResearchOtaState {
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

/// @brief Stage one RP2350W firmware artifact on the ESP32-C6 fixture.
/// These functions are used only by the device-to-device peer OTA test.
fl::json beginOtaArtifact(size_t expected_size, const char* sha256);
fl::json writeOtaArtifact(fl::vector<fl::u8> bytes);
fl::json finishOtaArtifact();
fl::json startOtaArtifactServer();
fl::json otaArtifactStatus();

/// @brief Queue an RP2350W HTTP update so the RPC response can be sent first.
fl::json queueOtaArtifactUpdate(const char* host, uint16_t port);

/// @brief Execute a queued RP2350W update from the sketch loop.
void pollOtaArtifactUpdate();

/// @brief Get current OTA autoresearch state.
/// @return Reference to the global OTA state
AutoResearchOtaState& getOtaState();
