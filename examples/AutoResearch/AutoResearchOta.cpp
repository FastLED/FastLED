// examples/AutoResearch/AutoResearchOta.cpp
//
// OTA autoresearch implementation for ESP32.
// Uses fl::net::OTA for the OTA HTTP server with Basic Auth.
// Uses ESP-IDF native APIs for WiFi Soft AP.
// Guarded with FL_IS_ESP32 - no-op stubs on other platforms.

// Gate out under low-memory mode -- no OTA path on parts that don't have
// WiFi / a network stack. Matches the conditional structure in
// AutoResearch.ino itself.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchOta.h"
#include "fl/stl/json.h"
#include "fl/stl/singleton.h"
#include "fl/log/log.h"
#include "fl/stl/vector.h"

struct OtaStateHolder {
    AutoResearchOtaState state;
};

struct RpOtaUpdateState {
    bool pending = false;
    char host[16] = {};
    uint16_t port = 0;
};

RpOtaUpdateState& getRpOtaUpdateState() {
    return fl::Singleton<RpOtaUpdateState>::instance();
}

AutoResearchOtaState& getOtaState() {
    return fl::Singleton<OtaStateHolder>::instance().state;
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

#include "fl/net/ota.h"
#include "fl/stl/cstring.h"
#include "fl/stl/unique_ptr.h"
#include <Arduino.h>

// ESP-IDF headers for WiFi AP
// IWYU pragma: begin_keep
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>
// IWYU pragma: end_keep

struct EspOtaRuntimeState {
    fl::unique_ptr<fl::net::OTA> ota;
    esp_netif_t* netif_ap = nullptr;
    bool event_loop_initialized = false;
    bool wifi_initialized = false;
};

struct OtaArtifactState {
    File file;
    httpd_handle_t server = nullptr;
    size_t expected_size = 0;
    size_t received_size = 0;
    size_t served_requests = 0;
    char expected_sha256[65] = {};
};

OtaArtifactState& getOtaArtifactState() {
    return fl::Singleton<OtaArtifactState>::instance();
}

EspOtaRuntimeState& getEspOtaRuntimeState() {
    return fl::Singleton<EspOtaRuntimeState>::instance();
}

static bool copySha256(char* destination, size_t capacity, const char* source) {
    if (source == nullptr || capacity != 65) {
        return false;
    }
    size_t index = 0;
    while (source[index] != '\0') {
        const char value = source[index];
        const bool is_digit = value >= '0' && value <= '9';
        const bool is_lower = value >= 'a' && value <= 'f';
        if ((!is_digit && !is_lower) || index + 1 >= capacity) {
            return false;
        }
        destination[index] = value;
        ++index;
    }
    if (index != 64) {
        return false;
    }
    destination[index] = '\0';
    return true;
}

static bool sha256File(const char* path, char* output, size_t output_capacity) {
    if (output_capacity != 65) {
        return false;
    }
    File file = LittleFS.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        return false;
    }
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    uint8_t bytes[512];
    while (file.available()) {
        const size_t read = file.read(bytes, sizeof(bytes));
        if (read == 0) {
            file.close();
            mbedtls_sha256_free(&context);
            return false;
        }
        mbedtls_sha256_update(&context, bytes, read);
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&context, digest);
    mbedtls_sha256_free(&context);
    file.close();
    for (size_t index = 0; index < sizeof(digest); ++index) {
        output[index * 2] = "0123456789abcdef"[digest[index] >> 4];
        output[index * 2 + 1] = "0123456789abcdef"[digest[index] & 0x0f];
    }
    output[64] = '\0';
    return true;
}

static esp_err_t serveOtaArtifact(httpd_req_t* request) {
    File file = LittleFS.open("/rp2350.bin", FILE_READ);
    if (!file || file.isDirectory()) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Artifact unavailable");
    }
    httpd_resp_set_type(request, "application/octet-stream");
    // Content-Length is REQUIRED by the client, even though the body streams
    // out chunked. In the pinned arduino-pico core (rp2040-5.7.0),
    // HTTPClient::_size is initialised to -1 and only ever assigned from a
    // Content-Length header (HTTPClient.cpp:1111), and HTTPUpdate takes the
    // `len > 0` branch or fails with HTTP_UE_SERVER_NOT_REPORT_SIZE
    // (HTTPUpdate.cpp:232, :295). Dropping this header makes every
    // applyOtaArtifact fail with "Server did not report size".
    //
    // So the #3956 review question "confirm this Arduino-Pico OTA client
    // accepts chunked responses" resolves to: it does not — it decodes the
    // chunked body but takes the image size from Content-Length. Keep both.
    char length[16];
    snprintf(length, sizeof(length), "%u", static_cast<unsigned>(file.size()));
    httpd_resp_set_hdr(request, "Content-Length", length);
    uint8_t bytes[512];
    while (file.available()) {
        const size_t read = file.read(bytes, sizeof(bytes));
        if (read == 0 || httpd_resp_send_chunk(request,
                                                reinterpret_cast<const char*>(bytes),
                                                read) != ESP_OK) {
            file.close();
            // Deliberately NO terminating zero chunk here: in chunked framing
            // a zero-length chunk means "body ended normally", so sending one
            // after a failed read would present a half-written firmware image
            // as a complete one. Returning ESP_FAIL drops the connection
            // mid-stream, which is how the client detects the truncation.
            return ESP_FAIL;
        }
    }
    file.close();
    const esp_err_t finished = httpd_resp_send_chunk(request, nullptr, 0);
    if (finished != ESP_OK) {
        return finished;
    }
    // Count the request only once the whole artifact is on the wire. Bumping
    // this up front made a 404 or a partial send look like a successful
    // transfer, and the host asserts `servedRequests >= 1` as proof the
    // RP2350W actually downloaded the image.
    ++getOtaArtifactState().served_requests;
    return ESP_OK;
}

// ============================================================================
// WiFi Soft AP Setup (same pattern as AutoResearchNet.cpp::initWifiAP)
// ============================================================================

static bool initOtaWifiAP() {
    AutoResearchOtaState& otaState = getOtaState();
    EspOtaRuntimeState& runtimeState = getEspOtaRuntimeState();
    if (otaState.wifi_ap_active) {
        return true;  // Already active
    }

    // Initialize TCP/IP stack (safe to call multiple times)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN("[OTA] esp_netif_init failed: " << esp_err_to_name(err));
        return false;
    }

    // Create default event loop (safe to call multiple times)
    if (!runtimeState.event_loop_initialized) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[OTA] esp_event_loop_create_default failed: " << esp_err_to_name(err));
            return false;
        }
        runtimeState.event_loop_initialized = true;
    }

    // Create default WiFi AP netif
    if (!runtimeState.netif_ap) {
        runtimeState.netif_ap = esp_netif_create_default_wifi_ap();
        if (!runtimeState.netif_ap) {
            FL_WARN("[OTA] esp_netif_create_default_wifi_ap failed");
            return false;
        }
    }

    // Initialize WiFi with default config
    if (!runtimeState.wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("[OTA] esp_wifi_init failed: " << esp_err_to_name(err));
            return false;
        }
        runtimeState.wifi_initialized = true;
    }

    // Configure AP
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, AUTORESEARCH_OTA_SSID, strlen(AUTORESEARCH_OTA_SSID));
    wifi_config.ap.ssid_len = strlen(AUTORESEARCH_OTA_SSID);
    memcpy(wifi_config.ap.password, AUTORESEARCH_OTA_PASSWORD, strlen(AUTORESEARCH_OTA_PASSWORD));
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

    otaState.wifi_ap_active = true;
    FL_WARN("[OTA] WiFi AP started: SSID=" << AUTORESEARCH_OTA_SSID << " IP=" << AUTORESEARCH_OTA_AP_IP);
    return true;
}

// ============================================================================
// Public API
// ============================================================================

fl::json startOta() {
    fl::json response = fl::json::object();
    AutoResearchOtaState& otaState = getOtaState();
    EspOtaRuntimeState& runtimeState = getEspOtaRuntimeState();

    // Start WiFi AP
    if (!initOtaWifiAP()) {
        response.set("success", false);
        response.set("error", "Failed to start WiFi AP for OTA");
        return response;
    }

    // Create and start OTA server
    runtimeState.ota = fl::make_unique<fl::net::OTA>();
    if (!runtimeState.ota->begin(AUTORESEARCH_OTA_HOSTNAME, AUTORESEARCH_OTA_OTA_PASSWORD)) {
        response.set("success", false);
        response.set("error", "Failed to start OTA server");
        runtimeState.ota.reset();
        return response;
    }

    otaState.ota_active = true;
    FL_WARN("[OTA] OTA server started: hostname=" << AUTORESEARCH_OTA_HOSTNAME);

    response.set("success", true);
    response.set("ssid", AUTORESEARCH_OTA_SSID);
    response.set("password", AUTORESEARCH_OTA_PASSWORD);
    response.set("ip", AUTORESEARCH_OTA_AP_IP);
    response.set("port", static_cast<int64_t>(AUTORESEARCH_OTA_PORT));
    response.set("ota_password", AUTORESEARCH_OTA_OTA_PASSWORD);
    response.set("hostname", AUTORESEARCH_OTA_HOSTNAME);
    return response;
}

fl::json stopOta() {
    fl::json response = fl::json::object();
    AutoResearchOtaState& otaState = getOtaState();
    EspOtaRuntimeState& runtimeState = getEspOtaRuntimeState();

    // Stop OTA server
    if (runtimeState.ota.get()) {
        runtimeState.ota.reset();
        otaState.ota_active = false;
        FL_WARN("[OTA] OTA server stopped");
    }

    // Stop WiFi
    if (otaState.wifi_ap_active) {
        esp_wifi_stop();
        otaState.wifi_ap_active = false;
        FL_WARN("[OTA] WiFi AP stopped");
    }

    response.set("success", true);
    return response;
}

fl::json beginOtaArtifact(size_t expected_size, const char* sha256) {
    fl::json response = fl::json::object();
    OtaArtifactState& state = getOtaArtifactState();
    if (expected_size == 0 || !copySha256(state.expected_sha256,
                                           sizeof(state.expected_sha256), sha256)) {
        response.set("success", false);
        response.set("error", "Invalid artifact metadata");
        return response;
    }
    if (!LittleFS.begin(true)) {
        response.set("success", false);
        response.set("error", "LittleFS mount failed");
        return response;
    }
    if (expected_size > LittleFS.totalBytes()) {
        response.set("success", false);
        response.set("error", "Artifact exceeds fixture filesystem");
        return response;
    }
    if (state.file) {
        state.file.close();
    }
    LittleFS.remove("/rp2350.bin.part");
    state.file = LittleFS.open("/rp2350.bin.part", FILE_WRITE);
    if (!state.file) {
        response.set("success", false);
        response.set("error", "Artifact staging open failed");
        return response;
    }
    state.expected_size = expected_size;
    state.received_size = 0;
    response.set("success", true);
    response.set("capacity", static_cast<int64_t>(LittleFS.totalBytes()));
    return response;
}

fl::json writeOtaArtifact(fl::vector<fl::u8> bytes) {
    fl::json response = fl::json::object();
    OtaArtifactState& state = getOtaArtifactState();
    if (!state.file || bytes.empty() ||
        bytes.size() > state.expected_size - state.received_size) {
        response.set("success", false);
        response.set("error", "Invalid artifact chunk");
        return response;
    }
    const size_t written = state.file.write(bytes.data(), bytes.size());
    state.received_size += written;
    response.set("success", written == bytes.size());
    response.set("received", static_cast<int64_t>(state.received_size));
    if (written != bytes.size()) {
        response.set("error", "Artifact write failed");
    }
    return response;
}

fl::json finishOtaArtifact() {
    fl::json response = fl::json::object();
    OtaArtifactState& state = getOtaArtifactState();
    if (!state.file || state.received_size != state.expected_size) {
        response.set("success", false);
        response.set("error", "Artifact size mismatch");
        return response;
    }
    state.file.close();
    char actual_sha256[65];
    if (!sha256File("/rp2350.bin.part", actual_sha256, sizeof(actual_sha256)) ||
        fl::strcmp(actual_sha256, state.expected_sha256) != 0) {
        LittleFS.remove("/rp2350.bin.part");
        response.set("success", false);
        response.set("error", "Artifact SHA-256 mismatch");
        return response;
    }
    LittleFS.remove("/rp2350.bin");
    if (!LittleFS.rename("/rp2350.bin.part", "/rp2350.bin")) {
        response.set("success", false);
        response.set("error", "Artifact publish failed");
        return response;
    }
    response.set("success", true);
    response.set("size", static_cast<int64_t>(state.received_size));
    response.set("sha256", actual_sha256);
    return response;
}

fl::json startOtaArtifactServer() {
    fl::json response = fl::json::object();
    OtaArtifactState& state = getOtaArtifactState();
    if (!LittleFS.begin(true) || !LittleFS.exists("/rp2350.bin")) {
        response.set("success", false);
        response.set("error", "No published RP2350 artifact");
        return response;
    }
    if (!state.server) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = 8081;
        if (httpd_start(&state.server, &config) != ESP_OK) {
            response.set("success", false);
            response.set("error", "Artifact HTTP server start failed");
            return response;
        }
        httpd_uri_t artifact = {};
        artifact.uri = "/rp2350.bin";
        artifact.method = HTTP_GET;
        artifact.handler = serveOtaArtifact;
        if (httpd_register_uri_handler(state.server, &artifact) != ESP_OK) {
            httpd_stop(state.server);
            state.server = nullptr;
            response.set("success", false);
            response.set("error", "Artifact HTTP route registration failed");
            return response;
        }
    }
    response.set("success", true);
    response.set("ip", AUTORESEARCH_OTA_AP_IP);
    response.set("port", static_cast<int64_t>(8081));
    return response;
}

fl::json otaArtifactStatus() {
    fl::json response = fl::json::object();
    OtaArtifactState& state = getOtaArtifactState();
    response.set("success", LittleFS.begin(true) && LittleFS.exists("/rp2350.bin"));
    response.set("servedRequests", static_cast<int64_t>(state.served_requests));
    response.set("received", static_cast<int64_t>(state.received_size));
    return response;
}

#else  // !FL_IS_ESP32 || !SOC_WIFI_SUPPORTED

// ============================================================================
// Stub Implementation for Non-ESP32 Platforms
// ============================================================================

fl::json startOta() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "OTA autoresearch only supported on ESP32");
    return response;
}

fl::json stopOta() {
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}

fl::json beginOtaArtifact(size_t expected_size, const char* sha256) {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact staging only supported on ESP32-C6");
    return response;
}

fl::json writeOtaArtifact(fl::vector<fl::u8> bytes) {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact staging only supported on ESP32-C6");
    return response;
}

fl::json finishOtaArtifact() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact staging only supported on ESP32-C6");
    return response;
}

fl::json startOtaArtifactServer() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact server only supported on ESP32-C6");
    return response;
}

fl::json otaArtifactStatus() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact server only supported on ESP32-C6");
    return response;
}

#endif  // FL_IS_ESP32

#if defined(FL_IS_RP2350) && defined(PICO_CYW43_SUPPORTED)
#include "FastLED.h"       // FastLED.watchdog()
#include "fl/wdt/watchdog.h"
#include <HTTPUpdate.h>
#include <WiFi.h>

fl::json queueOtaArtifactUpdate(const char* host, uint16_t port) {
    fl::json response = fl::json::object();
    RpOtaUpdateState& state = getRpOtaUpdateState();
    if (host == nullptr || port == 0) {
        response.set("success", false);
        response.set("error", "Invalid artifact endpoint");
        return response;
    }
    size_t index = 0;
    while (host[index] != '\0' && index + 1 < sizeof(state.host)) {
        state.host[index] = host[index];
        ++index;
    }
    if (host[index] != '\0') {
        response.set("success", false);
        response.set("error", "Artifact host is too long");
        return response;
    }
    state.host[index] = '\0';
    state.port = port;
    state.pending = true;
    response.set("success", true);
    response.set("accepted", true);
    return response;
}

void pollOtaArtifactUpdate(uint32_t watchdog_restore_ms) {
    RpOtaUpdateState& state = getRpOtaUpdateState();
    if (!state.pending) {
        return;
    }
    state.pending = false;

    // updater.update() is a synchronous, multi-second firmware download run
    // straight from loop(). The sketch feeds the watchdog only at the top and
    // bottom of the loop scope, so a download longer than that window resets
    // the board mid-flash-write.
    //
    // Re-arming with a longer timeout does NOT work here: Watchdog::begin()
    // clamps to FL_WATCHDOG_MAX_TIMEOUT_MS, which is 16777 ms on RP2350
    // (watchdog_rp.impl.hpp:47, :111) because the hardware counter is 24-bit
    // microseconds. Asking for more silently yields ~16.8 s, and a real image
    // download outruns that too. Halt the counter for the transfer and re-arm
    // afterwards, so the loop is protected again the moment it returns.
    fl::Watchdog& wdt = FastLED.watchdog();
    wdt.feed();
    wdt.disable();

    WiFiClient client;
    HTTPUpdate updater;
    const t_httpUpdate_return result = updater.update(client, state.host, state.port,
                                                       "/rp2350.bin");

    // Re-arm before reporting, so a slow log write is already covered.
    wdt.begin(watchdog_restore_ms);
    wdt.feed();

    if (result != HTTP_UPDATE_OK) {
        FL_WARN("[OTA] RP2350W artifact update failed: " << updater.getLastErrorString());
    }
}

#else

fl::json queueOtaArtifactUpdate(const char* host, uint16_t port) {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "Artifact update only supported on RP2350W");
    return response;
}

void pollOtaArtifactUpdate(uint32_t /*watchdog_restore_ms*/) {}

#endif

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY
