// examples/AutoResearch/AutoResearchNet.cpp
//
// Network autoresearch implementation for ESP32.
// Uses fl::asio::http::Server (unified HTTP API) for the HTTP server.
// Uses ESP-IDF native APIs for WiFi Soft AP and HTTP client.
// Guarded with FL_IS_ESP32 - no-op stubs on other platforms.

// Keep the Arduino-Pico WiFi library visible to the LDF before the
// header-derived low-memory guard below. The RP2350W peer owns
// WiFiClient/WiFiServer, so this is its direct framework dependency.
#if defined(PICO_CYW43_SUPPORTED)
// IWYU pragma: begin_keep
#include <WiFi.h>
// IWYU pragma: end_keep
#endif

// Gate out under low-memory mode -- the LowMemory bring-up surface
// (AutoResearchLowMemory.h) doesn't expose any network endpoints and the
// fl::net HTTP / asio machinery is several KB that won't fit on the
// LPC845-BRK / similar Low + Tiny memory targets. Matches the conditional
// structure in AutoResearch.ino itself.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchNet.h"
#include "FastLED.h"
#include "fl/log/log.h"
#include "fl/stl/json.h"
#include "fl/stl/singleton.h"

AutoResearchNetState& getNetState() {
    return fl::Singleton<AutoResearchNetState>::instance();
}

// ============================================================================
// ESP32 Implementation
// ============================================================================

// ESP32-H2 (and other WiFi-less ESP32 family members) must take the
// stub branch — esp_wifi.h does not even preprocess there
// (CONFIG_ESP_WIFI_* are absent from its sdkconfig). Gate on actual
// WiFi silicon, not on the family macro. (FastLED#3576 Phase 4)
#if defined(FL_IS_ESP32)
#include "soc/soc_caps.h"  // SOC_WIFI_SUPPORTED
#endif

#if defined(FL_IS_ESP32) && defined(SOC_WIFI_SUPPORTED) && SOC_WIFI_SUPPORTED

// Unified HTTP server API (must be included before Arduino.h to avoid INADDR_NONE conflict)
#include "fl/stl/asio/http/server.h"

#include "fl/stl/array.h"
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

struct EspNetState {
    fl::unique_ptr<fl::asio::http::Server> http_server;
    esp_netif_t* netif_ap = nullptr;
    bool event_loop_initialized = false;
    bool wifi_initialized = false;
};

EspNetState& espNetState() {
    return fl::Singleton<EspNetState>::instance();
}

// ============================================================================
// WiFi Soft AP Setup
// ============================================================================

static bool initWifiAP() {
    if (getNetState().wifi_ap_active) {
        return true;  // Already active
    }

    // Initialize TCP/IP stack (safe to call multiple times)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN("[NET] esp_netif_init failed: " << esp_err_to_name(err));
        return false;
    }

    // Create default event loop (safe to call multiple times)
    if (!espNetState().event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_event_loop_create_default failed: " << esp_err_to_name(err));
            return false;
        }
        espNetState().event_loop_initialized = true;
    }

    // Create default WiFi AP netif
    if (!espNetState().netif_ap) {
        espNetState().netif_ap = esp_netif_create_default_wifi_ap();
        if (!espNetState().netif_ap) {
            FL_WARN("[NET] esp_netif_create_default_wifi_ap failed");
            return false;
        }
    }

    // Initialize WiFi with default config
    if (!espNetState().wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_wifi_init failed: " << esp_err_to_name(err));
            return false;
        }
        espNetState().wifi_initialized = true;
    }

    // Configure AP
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, AUTORESEARCH_NET_SSID, strlen(AUTORESEARCH_NET_SSID));
    wifi_config.ap.ssid_len = strlen(AUTORESEARCH_NET_SSID);
    memcpy(wifi_config.ap.password, AUTORESEARCH_NET_PASSWORD, strlen(AUTORESEARCH_NET_PASSWORD));
    wifi_config.ap.max_connection = AUTORESEARCH_NET_MAX_CONNECTIONS;
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

    getNetState().wifi_ap_active = true;
    FL_WARN("[NET] WiFi AP started: SSID=" << AUTORESEARCH_NET_SSID << " IP=" << AUTORESEARCH_NET_AP_IP);
    return true;
}

// ============================================================================
// HTTP Server Setup (using unified fl::asio::http::Server)
// ============================================================================

static bool startHttpServer() {
    if (espNetState().http_server.get()) {
        return true;  // Already running
    }

    espNetState().http_server = fl::make_unique<fl::asio::http::Server>();

    // Register routes using the unified API
    espNetState().http_server->get("/ping", [](const fl::asio::http::Request&) {
        return fl::asio::http::Response::ok("pong");
    });

    // NOTE (FastLED#3588): these handlers execute ON the esp_http_server
    // task, concurrently with the main loop. Building fl::json here
    // reproducibly corrupted main-task json objects on classic ESP32
    // (bisect: string-only handlers are stable, fl::json handlers crash
    // the main task within one request). Until fl::json is audited for
    // cross-task use, handlers format responses with fl::snprintf.
    espNetState().http_server->get("/status", [](const fl::asio::http::Request&) {
        char body[128];
#if defined(FL_IS_ESP_32S3)
        const char* chip = "esp32s3";
#elif defined(FL_IS_ESP_32C6)
        const char* chip = "esp32c6";
#elif defined(FL_IS_ESP_32C3)
        const char* chip = "esp32c3";
#else
        const char* chip = "esp32";
#endif
        fl::snprintf(body, sizeof(body),
                     "{\"uptime_ms\":%u,\"free_heap\":%u,\"chip\":\"%s\"}",
                     static_cast<unsigned>(millis()),
                     static_cast<unsigned>(ESP.getFreeHeap()), chip);
        fl::asio::http::Response resp = fl::asio::http::Response::ok(body);
        resp.header("Content-Type", "application/json");
        return resp;
    });

    espNetState().http_server->post("/echo", [](const fl::asio::http::Request& req) {
        if (!req.has_body()) {
            return fl::asio::http::Response::bad_request("No body");
        }
        return fl::asio::http::Response::ok(req.body());
    });

    espNetState().http_server->get("/leds", [](const fl::asio::http::Request&) {
        // Static body — see the fl::json cross-task note above (#3588).
        fl::asio::http::Response resp =
            fl::asio::http::Response::ok("{\"num_leds\":10,\"brightness\":64}");
        resp.header("Content-Type", "application/json");
        return resp;
    });

    // Network-carried JSON-RPC contract for the peer fixture. These responses
    // stay string-only because the ESP HTTP task must not construct fl::json
    // objects concurrently with the sketch loop (#3588).
    espNetState().http_server->get("/rpc/discover", [](const fl::asio::http::Request&) {
        fl::asio::http::Response resp = fl::asio::http::Response::ok(
            "{\"jsonrpc\":\"2.0\",\"result\":{\"methods\":[\"rpc.discover\",\"ping\",\"debugTest\",\"status\"]},\"id\":1}");
        resp.header("Content-Type", "application/json");
        return resp;
    });
    espNetState().http_server->get("/rpc/ping", [](const fl::asio::http::Request&) {
        fl::asio::http::Response resp = fl::asio::http::Response::ok(
            "{\"jsonrpc\":\"2.0\",\"result\":{\"pong\":true},\"id\":1}");
        resp.header("Content-Type", "application/json");
        return resp;
    });
    espNetState().http_server->get("/rpc/status", [](const fl::asio::http::Request&) {
        fl::asio::http::Response resp = fl::asio::http::Response::ok(
            "{\"jsonrpc\":\"2.0\",\"result\":{\"ready\":true},\"id\":1}");
        resp.header("Content-Type", "application/json");
        return resp;
    });
    espNetState().http_server->get("/rpc/unknown", [](const fl::asio::http::Request&) {
        fl::asio::http::Response resp = fl::asio::http::Response::not_found();
        resp.body("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1}");
        resp.header("Content-Type", "application/json");
        return resp;
    });
    espNetState().http_server->post("/rpc", [](const fl::asio::http::Request& req) {
        const fl::string body = req.body();
        if (!fl::strstr(body.c_str(), "\"jsonrpc\":\"2.0\"") ||
            !fl::strstr(body.c_str(), "\"method\"")) {
            fl::asio::http::Response resp = fl::asio::http::Response::bad_request(
                "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":1}");
            resp.header("Content-Type", "application/json");
            return resp;
        }
        const char* response =
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1}";
        if (fl::strstr(body.c_str(), "\"method\":\"rpc.discover\"")) {
            response =
                "{\"jsonrpc\":\"2.0\",\"result\":{\"methods\":[\"rpc.discover\",\"ping\",\"debugTest\",\"status\"]},\"id\":1}";
        } else if (fl::strstr(body.c_str(), "\"method\":\"ping\"")) {
            response = "{\"jsonrpc\":\"2.0\",\"result\":{\"pong\":true},\"id\":1}";
        } else if (fl::strstr(body.c_str(), "\"method\":\"status\"")) {
            response = "{\"jsonrpc\":\"2.0\",\"result\":{\"ready\":true},\"id\":1}";
        }
        fl::asio::http::Response resp = fl::asio::http::Response::ok(response);
        resp.header("Content-Type", "application/json");
        return resp;
    });

    if (!espNetState().http_server->start(getNetState().server_port)) {
        FL_WARN("[NET] HTTP server failed to start: " << espNetState().http_server->last_error().c_str());
        espNetState().http_server.reset();
        return false;
    }

    getNetState().http_server_active = true;
    FL_WARN("[NET] HTTP server started on port " << getNetState().server_port);
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

    if (!espNetState().event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[NET] esp_event_loop_create_default failed: "
                    << esp_err_to_name(err));
            return false;
        }
        espNetState().event_loop_initialized = true;
    }
    return true;
}

struct HttpResponseCapture {
    char body[256] = {};
    size_t length = 0;
    bool truncated = false;
};

struct HttpResponseHash {
    uint32_t hash = 2166136261u;
    size_t bytes = 0;
};

static esp_err_t captureHttpResponse(esp_http_client_event_t* event) {
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data == nullptr ||
        event->data_len <= 0 || event->user_data == nullptr) {
        return ESP_OK;
    }
    HttpResponseCapture* capture =
        static_cast<HttpResponseCapture*>(event->user_data);
    const size_t available = sizeof(capture->body) - 1 - capture->length;
    const size_t incoming = static_cast<size_t>(event->data_len);
    const size_t copied = incoming < available ? incoming : available;
    fl::memcpy(capture->body + capture->length, event->data, copied);
    capture->length += copied;
    capture->body[capture->length] = '\0';
    capture->truncated = capture->truncated || copied != incoming;
    return ESP_OK;
}

static esp_err_t hashHttpResponse(esp_http_client_event_t* event) {
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data == nullptr ||
        event->data_len <= 0 || event->user_data == nullptr) {
        return ESP_OK;
    }
    HttpResponseHash* response = static_cast<HttpResponseHash*>(event->user_data);
    const char* data = static_cast<const char*>(event->data);
    for (int i = 0; i < event->data_len; ++i) {
        response->hash ^= static_cast<uint8_t>(data[i]);
        response->hash *= 16777619u;
    }
    response->bytes += static_cast<size_t>(event->data_len);
    return ESP_OK;
}

static uint32_t fnv1a(const char* bytes, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint8_t>(bytes[i]);
        hash *= 16777619u;
    }
    return hash;
}

static fl::json runHttpPayloadEchoTest(const char* url) {
    constexpr size_t kPayloadBytes = 4096;
    fl::array<char, kPayloadBytes> payload;
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>('A' + static_cast<char>(i % 23));
    }
    const uint32_t expected_hash = fnv1a(payload.data(), payload.size());

    fl::json result = fl::json::object();
    result.set("test", "POST /echo 4096-byte FNV-1a");
    result.set("bytes", static_cast<int64_t>(payload.size()));
    result.set("expected_hash", static_cast<int64_t>(expected_hash));

    HttpResponseHash response;
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = hashHttpResponse;
    config.user_data = &response;
    config.timeout_ms = 4000;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        result.set("passed", false);
        result.set("error", "Failed to init HTTP client");
        return result;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, payload.data(),
                                   static_cast<int>(payload.size()));

    const esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    const bool passed = err == ESP_OK && status == 200 &&
                        response.bytes == payload.size() &&
                        response.hash == expected_hash;
    result.set("status_code", static_cast<int64_t>(status));
    result.set("received_bytes", static_cast<int64_t>(response.bytes));
    result.set("received_hash", static_cast<int64_t>(response.hash));
    result.set("passed", passed);
    if (!passed) {
        result.set("error", "Echo payload hash mismatch");
    }
    return result;
}

/// @brief Run one bounded HTTP peer request and validate its response.
static fl::json runHttpRequestTest(const char* url, const char* test_name,
                                   const char* request_body,
                                   int expected_status,
                                   const char* expected_fragment) {
    fl::json result = fl::json::object();
    result.set("test", test_name);

    HttpResponseCapture capture;
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = captureHttpResponse;
    config.user_data = &capture;
    // Must stay below the task-WDT window: esp_http_client_perform()
    // blocks the loop task, and on single-core chips (ESP32-C6) a 5 s
    // block starved the idle task -> WDT panic_abort mid-test
    // (FastLED#3576 Phase 7 dual-device bench).
    config.timeout_ms = 2000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        result.set("passed", false);
        result.set("error", "Failed to init HTTP client");
        return result;
    }

    if (request_body != nullptr) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, request_body,
                                       static_cast<int>(fl::strlen(request_body)));
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

    const bool status_ok = status == expected_status;
    const bool content_ok = expected_fragment == nullptr ||
                            fl::strstr(capture.body, expected_fragment) != nullptr;
    result.set("body_truncated", capture.truncated);
    if (!status_ok || !content_ok || capture.truncated) {
        result.set("passed", false);
        fl::sstream ss;
        if (!status_ok) {
            ss << "Expected status " << expected_status << ", got " << status;
        } else if (capture.truncated) {
            ss << "Response body exceeded capture limit";
        } else {
            ss << "Unexpected response body";
        }
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
    response.set("ssid", AUTORESEARCH_NET_SSID);
    response.set("password", AUTORESEARCH_NET_PASSWORD);
    response.set("ip", AUTORESEARCH_NET_AP_IP);
    response.set("port", static_cast<int64_t>(getNetState().server_port));
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
    response.set("ssid", AUTORESEARCH_NET_SSID);
    response.set("password", AUTORESEARCH_NET_PASSWORD);
    response.set("gateway_ip", AUTORESEARCH_NET_AP_IP);
    return response;
}

fl::json runNetClientTest(const char* host_ip, uint16_t port) {
    fl::json response = fl::json::object();
    int tests_passed = 0;
    int tests_failed = 0;
    fl::json results = fl::json::array();

    // Build URLs
    char url_ping[128];
    char url_status[128];
    char url_leds[128];
    char url_rpc_discover[128];
    char url_rpc_ping[128];
    char url_rpc_status[128];
    char url_rpc_unknown[128];
    char url_rpc[128];
    char url_echo[128];
    snprintf(url_ping, sizeof(url_ping), "http://%s:%u/ping", host_ip, port);
    snprintf(url_status, sizeof(url_status), "http://%s:%u/status", host_ip, port);
    snprintf(url_leds, sizeof(url_leds), "http://%s:%u/leds", host_ip, port);
    snprintf(url_rpc_discover, sizeof(url_rpc_discover), "http://%s:%u/rpc/discover", host_ip, port);
    snprintf(url_rpc_ping, sizeof(url_rpc_ping), "http://%s:%u/rpc/ping", host_ip, port);
    snprintf(url_rpc_status, sizeof(url_rpc_status), "http://%s:%u/rpc/status", host_ip, port);
    snprintf(url_rpc_unknown, sizeof(url_rpc_unknown), "http://%s:%u/rpc/unknown", host_ip, port);
    snprintf(url_rpc, sizeof(url_rpc), "http://%s:%u/rpc", host_ip, port);
    snprintf(url_echo, sizeof(url_echo), "http://%s:%u/echo", host_ip, port);

    // Test 1: GET /ping
    {
        fl::json r = runHttpRequestTest(url_ping, "GET /ping", nullptr, 200,
                                        "pong");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    FastLED.watchdog().feed();  // between blocking HTTP tests (single-core WDT)

    // Test 2: GET /status
    {
        fl::json r = runHttpRequestTest(url_status, "GET /status", nullptr, 200,
                                        "\"chip\":\"rp2350w\"");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    FastLED.watchdog().feed();  // between blocking HTTP tests (single-core WDT)

    // Test 3: GET /leds
    {
        fl::json r = runHttpRequestTest(url_leds, "GET /leds", nullptr, 200,
                                        "num_leds");
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
    }

    const char* rpc_urls[] = {url_rpc_discover, url_rpc_ping, url_rpc_status,
                              url_rpc_unknown};
    const char* rpc_names[] = {"GET /rpc/discover", "GET /rpc/ping",
                               "GET /rpc/status", "GET /rpc/unknown"};
    const int rpc_statuses[] = {200, 200, 200, 404};
    const char* rpc_expected[] = {"\"methods\"", "\"pong\":true",
                                  "\"ready\":true", "-32601"};
    for (size_t i = 0; i < 4; ++i) {
        fl::json r = runHttpRequestTest(rpc_urls[i], rpc_names[i], nullptr,
                                        rpc_statuses[i], rpc_expected[i]);
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
        FastLED.watchdog().feed();
    }

    const char* rpc_post_names[] = {"POST /rpc discover", "POST /rpc ping",
                                    "POST /rpc status", "POST /rpc unknown"};
    const char* rpc_post_bodies[] = {
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.discover\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"status\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"unknown\",\"id\":1}",
    };
    const char* rpc_post_expected[] = {"\"methods\"", "\"pong\":true",
                                       "\"ready\":true", "-32601"};
    for (size_t i = 0; i < 4; ++i) {
        fl::json r = runHttpRequestTest(url_rpc, rpc_post_names[i],
                                        rpc_post_bodies[i], 200,
                                        rpc_post_expected[i]);
        auto passed = r[fl::string("passed")].as_bool();
        if (passed.has_value() && passed.value()) {
            tests_passed++;
        } else {
            tests_failed++;
        }
        results.push_back(r);
        FastLED.watchdog().feed();
    }

    fl::json payload_result = runHttpPayloadEchoTest(url_echo);
    const auto payload_passed = payload_result[fl::string("passed")].as_bool();
    if (payload_passed.has_value() && payload_passed.value()) {
        ++tests_passed;
    } else {
        ++tests_failed;
    }
    results.push_back(payload_result);
    FastLED.watchdog().feed();

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

    FL_WARN("[NET] Loopback test: server running on port " << getNetState().server_port);

    // Small delay to let server settle
    delay(100);

    // Build loopback URLs using 127.0.0.1
    char url_ping[128];
    char url_status[128];
    char url_leds[128];
    snprintf(url_ping, sizeof(url_ping), "http://127.0.0.1:%u/ping",
             getNetState().server_port);
    snprintf(url_status, sizeof(url_status), "http://127.0.0.1:%u/status",
             getNetState().server_port);
    snprintf(url_leds, sizeof(url_leds), "http://127.0.0.1:%u/leds",
             getNetState().server_port);

    // Test 1: GET /ping
    {
        fl::json r = runHttpRequestTest(url_ping, "GET /ping (loopback)",
                                        nullptr, 200, "pong");
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
        fl::json r = runHttpRequestTest(url_status, "GET /status (loopback)",
                                        nullptr, 200, "\"chip\"");
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
        fl::json r = runHttpRequestTest(url_leds, "GET /leds (loopback)",
                                        nullptr, 200, "num_leds");
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
    if (espNetState().http_server.get()) {
        espNetState().http_server->stop();
        espNetState().http_server.reset();
        getNetState().http_server_active = false;
        FL_WARN("[NET] HTTP server stopped");
    }

    // Stop WiFi
    if (getNetState().wifi_ap_active) {
        esp_wifi_stop();
        getNetState().wifi_ap_active = false;
        FL_WARN("[NET] WiFi AP stopped");
    }

    response.set("success", true);
    return response;
}

void pollNetServer() {}

#elif defined(FL_IS_RP2350) && defined(PICO_CYW43_SUPPORTED)

// ============================================================================
// RP2350W CYW43 peer implementation
// ============================================================================
//
// Arduino-Pico exposes lwIP through WiFiClient/WiFiServer rather than the
// ESP-IDF APIs used above. Keep this deliberately small: it provides the same
// GET endpoints as the C6 fixture and the matching outbound GET checks, while
// the sketch loop calls pollNetServer() to service one peer at a time.

#include "fl/net/wifi.h"
#include "fl/stl/array.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/unique_ptr.h"

namespace {

struct RpPeerState {
    fl::unique_ptr<WiFiServer> server;
    fl::unique_ptr<WiFiClient> client;
    char request[512] = {};
    size_t request_length = 0;
    char body[4097] = {};
    size_t body_length = 0;
    size_t expected_body_length = 0;
    bool headers_complete = false;
    uint32_t request_started_ms = 0;
};

RpPeerState& rpPeerState() {
    return fl::Singleton<RpPeerState>::instance();
}

void resetRpPeerRequest(RpPeerState& state) {
    state.request_length = 0;
    state.body_length = 0;
    state.expected_body_length = 0;
    state.headers_complete = false;
    state.request_started_ms = 0;
    state.request[0] = '\0';
    state.body[0] = '\0';
}

void writeHttpResponse(WiFiClient& client, const char* body,
                       const char* content_type) {
    client.print("HTTP/1.1 200 OK\r\nContent-Type: ");
    client.print(content_type);
    client.print("\r\nContent-Length: ");
    client.print(fl::strlen(body));
    client.print("\r\nConnection: close\r\n\r\n");
    client.print(body);
    client.stop();
}

void writeHttpNotFoundResponse(WiFiClient& client, const char* body) {
    client.print("HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: ");
    client.print(fl::strlen(body));
    client.print("\r\nConnection: close\r\n\r\n");
    client.print(body);
    client.stop();
}

fl::json runRpHttpRequestTest(const char* host_ip, uint16_t port,
                          const char* method, const char* path,
                          const char* request_body, const char* test_name,
                          int expected_status,
                          const char* expected_fragment) {
    fl::json result = fl::json::object();
    result.set("test", test_name);

    WiFiClient client;
    if (!client.connect(host_ip, port)) {
        result.set("passed", false);
        result.set("error", "TCP connect failed");
        return result;
    }

    client.print(method);
    client.print(" ");
    client.print(path);
    client.print(" HTTP/1.1\r\nHost: ");
    client.print(host_ip);
    if (request_body != nullptr) {
        client.print("\r\nContent-Type: application/json\r\nContent-Length: ");
        client.print(fl::strlen(request_body));
    }
    client.print("\r\nConnection: close\r\n\r\n");
    if (request_body != nullptr) {
        client.print(request_body);
    }

    const uint32_t deadline_ms = millis() + 2000;
    while (!client.available() && client.connected() &&
           static_cast<int32_t>(millis() - deadline_ms) < 0) {
        FastLED.watchdog().feed();
        delay(1);
    }

    char status_line[64];
    size_t length = 0;
    while (client.available() && length + 1 < sizeof(status_line)) {
        const int ch = client.read();
        if (ch < 0 || ch == '\n') {
            break;
        }
        if (ch != '\r') {
            status_line[length++] = static_cast<char>(ch);
        }
    }
    status_line[length] = '\0';
    char response[256] = {};
    size_t response_length = 0;
    while (static_cast<int32_t>(millis() - deadline_ms) < 0 &&
           (client.connected() || client.available())) {
        while (client.available() && response_length + 1 < sizeof(response)) {
            const int ch = client.read();
            if (ch < 0) {
                break;
            }
            response[response_length++] = static_cast<char>(ch);
        }
        FastLED.watchdog().feed();
        delay(1);
    }
    client.stop();
    response[response_length] = '\0';

    char expected_status_code[4] = {};
    fl::snprintf(expected_status_code, sizeof(expected_status_code), "%d",
                 expected_status);
    const bool has_http_prefix = fl::strncmp(status_line, "HTTP/1.1 ", 9) == 0 ||
                                 fl::strncmp(status_line, "HTTP/1.0 ", 9) == 0;
    const bool passed = has_http_prefix &&
                        fl::strncmp(status_line + 9, expected_status_code, 3) == 0;
    const bool content_ok = expected_fragment == nullptr ||
                            fl::strstr(response, expected_fragment) != nullptr;
    result.set("status_line", status_line);
    result.set("passed", passed && content_ok);
    if (!passed || !content_ok) {
        result.set("error", passed ? "Unexpected response body" : "Unexpected HTTP status");
    }
    return result;
}

uint32_t fnv1a(const char* bytes, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint8_t>(bytes[i]);
        hash *= 16777619u;
    }
    return hash;
}

fl::json runRpHttpPayloadEchoTest(const char* host_ip, uint16_t port) {
    constexpr size_t kPayloadBytes = 4096;
    fl::array<char, kPayloadBytes> payload;
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>('A' + static_cast<char>(i % 23));
    }
    const uint32_t expected_hash = fnv1a(payload.data(), payload.size());

    fl::json result = fl::json::object();
    result.set("test", "POST /echo 4096-byte FNV-1a");
    result.set("bytes", static_cast<int64_t>(payload.size()));
    result.set("expected_hash", static_cast<int64_t>(expected_hash));

    WiFiClient client;
    if (!client.connect(host_ip, port)) {
        result.set("passed", false);
        result.set("error", "TCP connect failed");
        return result;
    }
    client.print("POST /echo HTTP/1.1\r\nHost: ");
    client.print(host_ip);
    client.print("\r\nContent-Type: application/octet-stream\r\nContent-Length: ");
    client.print(payload.size());
    client.print("\r\nConnection: close\r\n\r\n");
    client.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    const uint32_t deadline_ms = millis() + 4000;
    char status_line[64] = {};
    size_t status_length = 0;
    bool status_done = false;
    bool header_done = false;
    uint8_t header_match = 0;
    uint32_t received_hash = 2166136261u;
    size_t received_bytes = 0;
    while (static_cast<int32_t>(millis() - deadline_ms) < 0 &&
           (client.connected() || client.available())) {
        while (client.available()) {
            const int value = client.read();
            if (value < 0) {
                break;
            }
            const char ch = static_cast<char>(value);
            if (!status_done) {
                if (ch == '\n') {
                    status_done = true;
                } else if (ch != '\r' && status_length + 1 < sizeof(status_line)) {
                    status_line[status_length++] = ch;
                }
                continue;
            }
            if (!header_done) {
                header_match = ch == "\r\n\r\n"[header_match]
                    ? static_cast<uint8_t>(header_match + 1) : 0;
                if (header_match == 4) {
                    header_done = true;
                }
                continue;
            }
            received_hash ^= static_cast<uint8_t>(ch);
            received_hash *= 16777619u;
            ++received_bytes;
        }
        FastLED.watchdog().feed();
        delay(1);
    }
    client.stop();
    status_line[status_length] = '\0';
    const bool status_ok = fl::strncmp(status_line, "HTTP/1.1 200", 12) == 0 ||
                           fl::strncmp(status_line, "HTTP/1.0 200", 12) == 0;
    const bool passed = status_ok && received_bytes == payload.size() &&
                        received_hash == expected_hash;
    result.set("received_bytes", static_cast<int64_t>(received_bytes));
    result.set("received_hash", static_cast<int64_t>(received_hash));
    result.set("passed", passed);
    if (!passed) {
        result.set("error", "Echo payload hash mismatch");
    }
    return result;
}

} // namespace

fl::json startNetServer() {
    fl::json response = fl::json::object();
    if (!fl::net::wifi::isConnected()) {
        response.set("success", false);
        response.set("error", "WiFi station is not connected");
        return response;
    }

    RpPeerState& state = rpPeerState();
    if (!state.server) {
        state.server = fl::make_unique<WiFiServer>(AUTORESEARCH_NET_SERVER_PORT);
        state.server->begin();
        state.server->setNoDelay(true);
    }
    getNetState().http_server_active = true;
    response.set("success", true);
    response.set("ip", fl::net::wifi::ipAddress().c_str());
    response.set("port", static_cast<int64_t>(AUTORESEARCH_NET_SERVER_PORT));
    return response;
}

fl::json startNetClient() {
    fl::json response = fl::json::object();
    response.set("success", fl::net::wifi::isConnected());
    response.set("ip", fl::net::wifi::ipAddress().c_str());
    return response;
}

fl::json runNetClientTest(const char* host_ip, uint16_t port) {
    fl::json response = fl::json::object();
    if (host_ip == nullptr || *host_ip == '\0' ||
        !fl::net::wifi::isConnected()) {
        response.set("success", false);
        response.set("error", "WiFi station is not connected");
        return response;
    }

    fl::json results = fl::json::array();
    const char* paths[] = {"/ping", "/status", "/leds", "/rpc/discover",
                           "/rpc/ping", "/rpc/status", "/rpc/unknown"};
    const char* names[] = {"GET /ping", "GET /status", "GET /leds",
                           "GET /rpc/discover", "GET /rpc/ping", "GET /rpc/status",
                           "GET /rpc/unknown"};
    const int expected_statuses[] = {200, 200, 200, 200, 200, 200, 404};
    const char* expected[] = {"pong", "\"chip\"", "num_leds", "\"methods\"",
                              "\"pong\":true", "\"ready\":true", "-32601"};
    int passed = 0;
    for (size_t i = 0; i < 7; ++i) {
        fl::json result = runRpHttpRequestTest(host_ip, port, "GET", paths[i],
                                               nullptr, names[i],
                                               expected_statuses[i], expected[i]);
        const auto result_passed = result[fl::string("passed")].as_bool();
        if (result_passed.has_value() && result_passed.value()) {
            ++passed;
        }
        results.push_back(result);
        FastLED.watchdog().feed();
    }

    const char* rpc_post_names[] = {"POST /rpc discover", "POST /rpc ping",
                                    "POST /rpc status", "POST /rpc unknown"};
    const char* rpc_post_bodies[] = {
        "{\"jsonrpc\":\"2.0\",\"method\":\"rpc.discover\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"status\",\"id\":1}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"unknown\",\"id\":1}",
    };
    const char* rpc_post_expected[] = {"\"methods\"", "\"pong\":true",
                                       "\"ready\":true", "-32601"};
    for (size_t i = 0; i < 4; ++i) {
        fl::json result = runRpHttpRequestTest(host_ip, port, "POST", "/rpc",
                                               rpc_post_bodies[i], rpc_post_names[i],
                                               200, rpc_post_expected[i]);
        const auto result_passed = result[fl::string("passed")].as_bool();
        if (result_passed.has_value() && result_passed.value()) {
            ++passed;
        }
        results.push_back(result);
        FastLED.watchdog().feed();
    }

    fl::json payload_result = runRpHttpPayloadEchoTest(host_ip, port);
    const auto payload_passed = payload_result[fl::string("passed")].as_bool();
    if (payload_passed.has_value() && payload_passed.value()) {
        ++passed;
    }
    results.push_back(payload_result);

    response.set("success", passed == 12);
    response.set("tests_passed", static_cast<int64_t>(passed));
    response.set("tests_failed", static_cast<int64_t>(12 - passed));
    response.set("results", results);
    return response;
}

fl::json runNetLoopback() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Loopback is not applicable to the CYW43 peer path");
    return response;
}

fl::json stopNet() {
    fl::json response = fl::json::object();
    RpPeerState& state = rpPeerState();
    if (state.client) {
        state.client->stop();
        state.client.reset();
    }
    resetRpPeerRequest(state);
    if (state.server) {
        state.server->stop();
        state.server.reset();
    }
    getNetState().http_server_active = false;
    getNetState().wifi_ap_active = false;
    fl::net::wifi::stop();
    response.set("success", true);
    return response;
}

void pollNetServer() {
    RpPeerState& state = rpPeerState();
    if (!state.server) {
        return;
    }
    if (!state.client && state.server->hasClient()) {
        state.client = fl::make_unique<WiFiClient>(state.server->accept());
        resetRpPeerRequest(state);
        state.request_started_ms = millis();
    }
    if (!state.client) {
        return;
    }
    if (static_cast<int32_t>(millis() - state.request_started_ms) >= 2000) {
        state.client->stop();
        state.client.reset();
        resetRpPeerRequest(state);
        return;
    }

    while (state.client->available() && !state.headers_complete &&
           state.request_length + 1 < sizeof(state.request)) {
        const int ch = state.client->read();
        if (ch < 0) {
            break;
        }
        state.request[state.request_length++] = static_cast<char>(ch);
        state.request[state.request_length] = '\0';
        if (state.request_length >= 4 &&
            fl::strncmp(state.request + state.request_length - 4, "\r\n\r\n", 4) == 0) {
            state.headers_complete = true;
        }
    }
    if (!state.headers_complete) {
        if (state.request_length + 1 == sizeof(state.request)) {
            state.client->print(
                "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\n\r\n");
            state.client->stop();
            state.client.reset();
            resetRpPeerRequest(state);
        }
        return;
    }

    const bool is_post = fl::strncmp(state.request, "POST ", 5) == 0;
    const bool is_post_rpc = fl::strncmp(state.request, "POST /rpc ", 10) == 0;
    const bool is_post_echo = fl::strncmp(state.request, "POST /echo ", 11) == 0;
    if (is_post && state.expected_body_length == 0) {
        const char* content_length = fl::strstr(state.request, "\r\nContent-Length:");
        if (content_length == nullptr) {
            state.client->print("HTTP/1.1 411 Length Required\r\nConnection: close\r\n\r\n");
            state.client->stop();
            state.client.reset();
            resetRpPeerRequest(state);
            return;
        }
        content_length = fl::strstr(content_length, ":");
        ++content_length;
        while (*content_length == ' ') {
            ++content_length;
        }
        while (*content_length >= '0' && *content_length <= '9') {
            if (state.expected_body_length > (sizeof(state.body) - 1) / 10) {
                state.client->print("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n");
                state.client->stop();
                state.client.reset();
                resetRpPeerRequest(state);
                return;
            }
            state.expected_body_length = state.expected_body_length * 10 +
                                         static_cast<size_t>(*content_length - '0');
            if (state.expected_body_length >= sizeof(state.body)) {
                state.client->print("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n");
                state.client->stop();
                state.client.reset();
                resetRpPeerRequest(state);
                return;
            }
            ++content_length;
        }
        if (state.expected_body_length == 0) {
            state.client->print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
            state.client->stop();
            state.client.reset();
            resetRpPeerRequest(state);
            return;
        }
    }
    while (is_post && state.client->available() &&
           state.body_length < state.expected_body_length) {
        const int ch = state.client->read();
        if (ch < 0) {
            break;
        }
        state.body[state.body_length++] = static_cast<char>(ch);
    }
    if (is_post && state.body_length < state.expected_body_length) {
        return;
    }
    state.body[state.body_length] = '\0';

    if (fl::strncmp(state.request, "GET /ping ", 10) == 0) {
        writeHttpResponse(*state.client, "pong", "text/plain");
    } else if (fl::strncmp(state.request, "GET /status ", 12) == 0) {
        writeHttpResponse(*state.client,
                          "{\"chip\":\"rp2350w\",\"network\":\"cyw43\"}",
                          "application/json");
    } else if (fl::strncmp(state.request, "GET /leds ", 10) == 0) {
        writeHttpResponse(*state.client, "{\"num_leds\":10,\"brightness\":64}",
                          "application/json");
    } else if (fl::strncmp(state.request, "GET /rpc/discover ", 18) == 0) {
        writeHttpResponse(*state.client,
                          "{\"jsonrpc\":\"2.0\",\"result\":{\"methods\":[\"rpc.discover\",\"ping\",\"debugTest\",\"status\"]},\"id\":1}",
                          "application/json");
    } else if (fl::strncmp(state.request, "GET /rpc/ping ", 14) == 0) {
        writeHttpResponse(*state.client,
                          "{\"jsonrpc\":\"2.0\",\"result\":{\"pong\":true},\"id\":1}",
                          "application/json");
    } else if (fl::strncmp(state.request, "GET /rpc/status ", 16) == 0) {
        writeHttpResponse(*state.client,
                          "{\"jsonrpc\":\"2.0\",\"result\":{\"ready\":true},\"id\":1}",
                          "application/json");
    } else if (fl::strncmp(state.request, "GET /rpc/unknown ", 17) == 0) {
        writeHttpNotFoundResponse(*state.client,
                                  "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1}");
    } else if (is_post_echo) {
        writeHttpResponse(*state.client, state.body, "application/octet-stream");
    } else if (is_post_rpc) {
        if (!fl::strstr(state.body, "\"jsonrpc\":\"2.0\"") ||
            !fl::strstr(state.body, "\"method\"")) {
            state.client->print("HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
            state.client->print("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":1}");
            state.client->stop();
        } else if (fl::strstr(state.body, "\"method\":\"rpc.discover\"")) {
            writeHttpResponse(*state.client,
                              "{\"jsonrpc\":\"2.0\",\"result\":{\"methods\":[\"rpc.discover\",\"ping\",\"debugTest\",\"status\"]},\"id\":1}",
                              "application/json");
        } else if (fl::strstr(state.body, "\"method\":\"ping\"")) {
            writeHttpResponse(*state.client,
                              "{\"jsonrpc\":\"2.0\",\"result\":{\"pong\":true},\"id\":1}",
                              "application/json");
        } else if (fl::strstr(state.body, "\"method\":\"status\"")) {
            writeHttpResponse(*state.client,
                              "{\"jsonrpc\":\"2.0\",\"result\":{\"ready\":true},\"id\":1}",
                              "application/json");
        } else {
            writeHttpResponse(*state.client,
                              "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1}",
                              "application/json");
        }
    } else {
        state.client->print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
        state.client->stop();
    }
    state.client.reset();
    resetRpPeerRequest(state);
}

#else  // !FL_IS_ESP32 && !FL_IS_RP2350

// ============================================================================
// Stub Implementation for Non-ESP32 Platforms
// ============================================================================

fl::json startNetServer() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net autoresearch only supported on ESP32");
    return response;
}

fl::json startNetClient() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net autoresearch only supported on ESP32");
    return response;
}

fl::json runNetClientTest(const char* host_ip, uint16_t port) {
    (void)host_ip;
    (void)port;
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net autoresearch only supported on ESP32");
    return response;
}

fl::json runNetLoopback() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Net loopback autoresearch only supported on ESP32");
    return response;
}

fl::json stopNet() {
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}

void pollNetServer() {}

#endif  // FL_IS_ESP32 || FL_IS_RP2350

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY
