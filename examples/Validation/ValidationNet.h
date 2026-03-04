// examples/Validation/ValidationNet.h
//
// Network validation for ESP32: WiFi Soft AP + HTTP server/client testing.
// Used by `bash validate --net-server` and `bash validate --net-client`.
//
// The ESP32 creates a WiFi Soft AP with known credentials.
// The host Python script connects to the AP and validates HTTP endpoints.

#pragma once

#include "fl/stl/stdint.h"

// WiFi AP configuration constants
#define VALIDATION_NET_SSID "FastLED-Validation"
#define VALIDATION_NET_PASSWORD "fastled123"
#define VALIDATION_NET_AP_IP "192.168.4.1"
#define VALIDATION_NET_SERVER_PORT 80
#define VALIDATION_NET_CLIENT_PORT 8080
#define VALIDATION_NET_MAX_CONNECTIONS 4

// Forward declarations
namespace fl {
class json;
}

/// @brief State for network validation
struct ValidationNetState {
    bool wifi_ap_active = false;
    bool http_server_active = false;
    bool http_client_active = false;
    uint16_t server_port = VALIDATION_NET_SERVER_PORT;
    uint16_t client_target_port = VALIDATION_NET_CLIENT_PORT;
};

/// @brief Start WiFi Soft AP and HTTP server with test endpoints.
/// @return JSON with {ssid, password, ip, port} on success, or {error} on failure.
fl::json startNetServer();

/// @brief Start WiFi Soft AP only (for net-client mode).
/// @return JSON with {ssid, password, gateway_ip} on success, or {error} on failure.
fl::json startNetClient();

/// @brief Run HTTP client tests against a host server.
/// @param host_ip IP address of the host HTTP server (e.g., "192.168.4.2")
/// @param port Port of the host HTTP server (e.g., 8080)
/// @return JSON with {success, tests_passed, tests_failed, results[]}
fl::json runNetClientTest(const char* host_ip, uint16_t port);

/// @brief Run self-contained loopback test: start HTTP server, client GETs localhost.
/// No WiFi needed — purely TCP over localhost (127.0.0.1).
/// @return JSON with {success, tests_passed, tests_failed, results[]}
fl::json runNetLoopback();

/// @brief Stop WiFi AP and HTTP server/client, release all resources.
/// @return JSON with {success: true} on success
fl::json stopNet();

/// @brief Get current network validation state.
/// @return Reference to the global net state
ValidationNetState& getNetState();
