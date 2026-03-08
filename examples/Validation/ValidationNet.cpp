// examples/Validation/ValidationNet.cpp
//
// Network validation implementation for ESP32.
// Uses fl::asio::http::Server (unified HTTP API) for the HTTP server.
// Uses ESP-IDF native APIs for WiFi Soft AP and HTTP client.
// Guarded with FL_IS_ESP32 - no-op stubs on other platforms.

#include "ValidationNet.h"
#include "fl/stl/json.h"
#include "fl/warn.h"

// Global net state
static ValidationNetState s_net_state;

ValidationNetState& getNetState() {
    return s_net_state;
}

// ============================================================================
// ESP32 Implementation
// ============================================================================

#if defined(FL_IS_ESP32)

// Unified HTTP server API (must be included before Arduino.h to avoid INADDR_NONE conflict)
#include "fl/stl/asio/http/server.h"

#include "fl/stl/cstring.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/sstream.h"
#include <Arduino.h>

// ESP-IDF headers for WiFi AP and HTTP client
// Must come after Arduino.h to get proper IPAddress definitions
// IWYU pragma: begin_keep
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_http_client.h>
// IWYU pragma: end_keep

// Static handles
static fl::unique_ptr<fl::asio::http::Server> s_http_server;
static esp_netif_t* s_netif_ap = nullptr;
static bool s_event_loop_initialized = false;
static bool s_wifi_initialized = false;

// ============================================================================
// WiFi Soft AP Setup
// ============================================================================

static bool initWifiAP() {
    if (s_net_state.wifi_ap_active) {
        return true;  // Already active
    }

    // Initialize TCP/IP stack (safe to call multiple times)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN("[NET] esp_netif_init failed: " << esp_err_to_name(err));
        return false;
    }

    // Create default event loop (safe to call multiple times)
    if (!s_event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_event_loop_create_default failed: " << esp_err_to_name(err));
            return false;
        }
        s_event_loop_initialized = true;
    }

    // Create default WiFi AP netif
    if (!s_netif_ap) {
        s_netif_ap = esp_netif_create_default_wifi_ap();
        if (!s_netif_ap) {
            FL_WARN("[NET] esp_netif_create_default_wifi_ap failed");
            return false;
        }
    }

    // Initialize WiFi with default config
    if (!s_wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_wifi_init failed: " << esp_err_to_name(err));
            return false;
        }
        s_wifi_initialized = true;
    }

    // Configure AP
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, VALIDATION_NET_SSID, strlen(VALIDATION_NET_SSID));
    wifi_config.ap.ssid_len = strlen(VALIDATION_NET_SSID);
    memcpy(wifi_config.ap.password, VALIDATION_NET_PASSWORD, strlen(VALIDATION_NET_PASSWORD));
    wifi_config.ap.max_connection = VALIDATION_NET_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.channel = 1;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        FL_WARN("[NET] esp_wifi_set_mode failed: " << esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        FL_WARN("[NET] esp_wifi_set_config failed: " << esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        FL_WARN("[NET] esp_wifi_start failed: " << esp_err_to_name(err));
        return false;
    }

    s_net_state.wifi_ap_active = true;
    FL_WARN("[NET] WiFi AP started: SSID=" << VALIDATION_NET_SSID << " IP=" << VALIDATION_NET_AP_IP);
    return true;
}

// ============================================================================
// HTTP Server Setup (using unified fl::asio::http::Server)
// ============================================================================

static bool startHttpServer() {
    if (s_http_server.get()) {
        return true;  // Already running
    }

    s_http_server = fl::make_unique<fl::asio::http::Server>();

    // Register routes using the unified API
    s_http_server->get("/ping", [](const fl::asio::http::Request&) {
        return fl::asio::http::Response::ok("pong");
    });

    s_http_server->get("/status", [](const fl::asio::http::Request&) {
        fl::json json = fl::json::object();
        json.set("uptime_ms", static_cast<int64_t>(millis()));
        json.set("free_heap", static_cast<int64_t>(ESP.getFreeHeap()));
#if defined(FL_IS_ESP_32S3)
        json.set("chip", "esp32s3");
#elif defined(FL_IS_ESP_32C6)
        json.set("chip", "esp32c6");
#elif defined(FL_IS_ESP_32C3)
        json.set("chip", "esp32c3");
#else
        json.set("chip", "esp32");
#endif
        fl::asio::http::Response resp;
        resp.json(json);
        return resp;
    });

    s_http_server->post("/echo", [](const fl::asio::http::Request& req) {
        if (!req.has_body()) {
            return fl::asio::http::Response::bad_request("No body");
        }
        return fl::asio::http::Response::ok(req.body());
    });

    s_http_server->get("/leds", [](const fl::asio::http::Request&) {
        fl::json json = fl::json::object();
        json.set("num_leds", static_cast<int64_t>(10));
        json.set("brightness", static_cast<int64_t>(64));
        fl::asio::http::Response resp;
        resp.json(json);
        return resp;
    });

    if (!s_http_server->start(s_net_state.server_port)) {
        FL_WARN("[NET] HTTP server failed to start: " << s_http_server->last_error().c_str());
        s_http_server.reset();
        return false;
    }

    s_net_state.http_server_active = true;
    FL_WARN("[NET] HTTP server started on port " << s_net_state.server_port);
    return true;
}

// ============================================================================
// HTTP Client Tests (using esp_http_client - no unified client for ESP32 yet)
// ============================================================================

/// @brief Ensure TCP/IP stack is initialized for loopback (no WiFi needed).
static bool initNetifForLoopback() {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN("[NET] esp_netif_init failed: " << esp_err_to_name(err));
        return false;
    }

    if (!s_event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_event_loop_create_default failed: "
                    << esp_err_to_name(err));
            return false;
        }
        s_event_loop_initialized = true;
    }
    return true;
}

/// @brief Run a single HTTP GET test and return result
static fl::json runHttpGetTest(const char* url, const char* test_name) {
    fl::json result = fl::json::object();
    result.set("test", test_name);

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 5000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        result.set("passed", false);
        result.set("error", "Failed to init HTTP client");
        return result;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        result.set("passed", false);
        fl::sstream ss;
        ss << "HTTP request failed: " << esp_err_to_name(err);
        result.set("error", ss.str().c_str());
        esp_http_client_cleanup(client);
        return result;
    }

    int status = esp_http_client_get_status_code(client);
    result.set("status_code", static_cast<int64_t>(status));

    if (status != 200) {
        result.set("passed", false);
        fl::sstream ss;
        ss << "Expected status 200, got " << status;
        result.set("error", ss.str().c_str());
    } else {
        result.set("passed", true);
    }

    esp_http_client_cleanup(client);
    return result;
}

// ============================================================================
// Public API
// ============================================================================

fl::json startNetServer() {
    fl::json response = fl::json::object();

    if (!initWifiAP()) {
        response.set("success", false);
        response.set("error", "Failed to start WiFi AP");
        return response;
    }

    if (!startHttpServer()) {
        response.set("success", false);
        response.set("error", "Failed to start HTTP server");
        return response;
    }

    response.set("success", true);
    response.set("ssid", VALIDATION_NET_SSID);
    response.set("password", VALIDATION_NET_PASSWORD);
    response.set("ip", VALIDATION_NET_AP_IP);
    response.set("port", static_cast<int64_t>(s_net_state.server_port));
    return response;
}

fl::json startNetClient() {
    fl::json response = fl::json::object();

    if (!initWifiAP()) {
        response.set("success", false);
        response.set("error", "Failed to start WiFi AP");
        return response;
    }

    response.set("success", true);
    response.set("ssid", VALIDATION_NET_SSID);
    response.set("password", VALIDATION_NET_PASSWORD);
    response.set("gateway_ip", VALIDATION_NET_AP_IP);
    return response;
}

fl::json runNetClientTest(const char* host_ip, uint16_t port) {
    fl::json response = fl::json::object();
    int tests_passed = 0;
    int tests_failed = 0;
    fl::json results = fl::json::array();

    // Build URLs
    char url_ping[128];
    char url_data[128];
    snprintf(url_ping, sizeof(url_ping), "http://%s:%u/ping", host_ip, port);
    snprintf(url_data, sizeof(url_data), "http://%s:%u/data", host_ip, port);

    // Test 1: GET /ping
    {
        fl::json r = runHttpGetTest(url_ping, "GET /ping");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    // Test 2: GET /data
    {
        fl::json r = runHttpGetTest(url_data, "GET /data");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    response.set("success", tests_failed == 0);
    response.set("tests_passed", static_cast<int64_t>(tests_passed));
    response.set("tests_failed", static_cast<int64_t>(tests_failed));
    response.set("results", results);
    return response;
}

fl::json runNetLoopback() {
    fl::json response = fl::json::object();
    int tests_passed = 0;
    int tests_failed = 0;
    fl::json results = fl::json::array();

    // Initialize TCP/IP stack for loopback (no WiFi needed)
    if (!initNetifForLoopback()) {
        response.set("success", false);
        response.set("error", "Failed to initialize network stack for loopback");
        return response;
    }

    // Start the HTTP server on localhost
    if (!startHttpServer()) {
        response.set("success", false);
        response.set("error", "Failed to start HTTP server for loopback test");
        return response;
    }

    FL_WARN("[NET] Loopback test: server running on port " << s_net_state.server_port);

    // Small delay to let server settle
    delay(100);

    // Build loopback URLs using 127.0.0.1
    char url_ping[128];
    char url_status[128];
    char url_leds[128];
    snprintf(url_ping, sizeof(url_ping), "http://127.0.0.1:%u/ping",
             s_net_state.server_port);
    snprintf(url_status, sizeof(url_status), "http://127.0.0.1:%u/status",
             s_net_state.server_port);
    snprintf(url_leds, sizeof(url_leds), "http://127.0.0.1:%u/leds",
             s_net_state.server_port);

    // Test 1: GET /ping
    {
        fl::json r = runHttpGetTest(url_ping, "GET /ping (loopback)");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    // Test 2: GET /status
    {
        fl::json r = runHttpGetTest(url_status, "GET /status (loopback)");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    // Test 3: GET /leds
    {
        fl::json r = runHttpGetTest(url_leds, "GET /leds (loopback)");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    response.set("success", tests_failed == 0);
    response.set("tests_passed", static_cast<int64_t>(tests_passed));
    response.set("tests_failed", static_cast<int64_t>(tests_failed));
    response.set("results", results);
    return response;
}

fl::json stopNet() {
    fl::json response = fl::json::object();

    // Stop HTTP server (unified API)
    if (s_http_server.get()) {
        s_http_server->stop();
        s_http_server.reset();
        s_net_state.http_server_active = false;
        FL_WARN("[NET] HTTP server stopped");
    }

    // Stop WiFi
    if (s_net_state.wifi_ap_active) {
        esp_wifi_stop();
        s_net_state.wifi_ap_active = false;
        FL_WARN("[NET] WiFi AP stopped");
    }

    response.set("success", true);
    return response;
}

#else  // !FL_IS_ESP32

// ============================================================================
// Stub Implementation for Non-ESP32 Platforms
// ============================================================================

fl::json startNetServer() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net validation only supported on ESP32");
    return response;
}

fl::json startNetClient() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net validation only supported on ESP32");
    return response;
}

fl::json runNetClientTest(const char* host_ip, uint16_t port) {
    (void)host_ip;
    (void)port;
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net validation only supported on ESP32");
    return response;
}

fl::json runNetLoopback() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net loopback validation only supported on ESP32");
    return response;
}

fl::json stopNet() {
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}

#endif  // FL_IS_ESP32
