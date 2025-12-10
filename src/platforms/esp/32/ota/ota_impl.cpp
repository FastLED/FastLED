/// @file platforms/esp/32/ota/ota_impl.cpp
/// ESP32-specific OTA implementation

#if defined(ESP32)

#include "platforms/ota.h"
#include "platforms/esp/esp_version.h"
#include "fl/ota.h"  // For OTAService enum

// OTA support detection flag
// OTA requires IDF 4.0+ for HTTP server and OTA APIs
// ESP32-H2 and ESP32-P4 lack WiFi hardware
// Arduino framework uses different OTA implementation
#if ESP_IDF_VERSION_4_OR_HIGHER && !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(ARDUINO)
#define FL_ESP_OTA_SUPPORTED
#endif

// Pure ESP-IDF OTA implementation (no Arduino framework required)
#ifdef FL_ESP_OTA_SUPPORTED

// ESP-IDF headers (available in both Arduino and pure ESP-IDF builds)
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <mdns.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>  // ok include
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <mbedtls/md.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>

// FastLED headers
#include "fl/str.h"
#include "fl/warn.h"
#include "fl/dbg.h"

namespace fl {
namespace platforms {

// ============================================================================
// HTTP Context and Helper Structures
// ============================================================================

/// @brief Context structure for HTTP handlers (stores password and callback access)
struct OTAHttpContext {
    const char* password;
    fl::function<void(size_t, size_t)>* progress_cb;
    fl::function<void(const char*)>* error_cb;
    void (**before_reboot_cb)();  // Pointer to function pointer
};

// ============================================================================
// Helper Functions (Internal)
// ============================================================================

namespace {

/// @brief Initialize mDNS service with hostname
/// @param hostname Device hostname (without .local suffix)
/// @return true if successful, false otherwise
bool initMDNS(const char* hostname) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        return false;
    }

    err = mdns_hostname_set(hostname);
    if (err != ESP_OK) {
        return false;
    }

    // Add Arduino OTA service for Arduino IDE discovery
    mdns_service_add(nullptr, "_arduino", "_tcp", 3232, nullptr, 0);

    return true;
}

/// @brief HTML content for the OTA upload page
/// @return Pointer to static HTML string
const char* getOtaHtmlPage() {
    return R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FastLED OTA Update</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            margin-top: 0;
        }
        .info {
            background-color: #e3f2fd;
            padding: 15px;
            border-radius: 4px;
            margin-bottom: 20px;
            border-left: 4px solid #2196F3;
        }
        form {
            margin-top: 20px;
        }
        input[type="file"] {
            display: block;
            margin: 15px 0;
            padding: 10px;
            width: 100%;
            box-sizing: border-box;
        }
        button {
            background-color: #4CAF50;
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
        }
        button:hover {
            background-color: #45a049;
        }
        button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
        }
        #progress {
            display: none;
            margin-top: 20px;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background-color: #f0f0f0;
            border-radius: 4px;
            overflow: hidden;
        }
        .progress-fill {
            height: 100%;
            background-color: #4CAF50;
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }
        .status {
            margin-top: 10px;
            padding: 10px;
            border-radius: 4px;
        }
        .status.success {
            background-color: #d4edda;
            color: #155724;
        }
        .status.error {
            background-color: #f8d7da;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>FastLED OTA Update</h1>
        <div class="info">
            <strong>Instructions:</strong>
            <ul>
                <li>Select a .bin firmware file</li>
                <li>Click "Upload Firmware"</li>
                <li>Wait for upload to complete</li>
                <li>Device will automatically reboot</li>
            </ul>
        </div>
        <form id="uploadForm">
            <input type="file" id="firmwareFile" accept=".bin" required>
            <button type="submit" id="uploadBtn">Upload Firmware</button>
        </form>
        <div id="progress">
            <div class="progress-bar">
                <div class="progress-fill" id="progressFill">0%</div>
            </div>
        </div>
        <div id="status"></div>
    </div>
    <script>
        const form = document.getElementById('uploadForm');
        const fileInput = document.getElementById('firmwareFile');
        const uploadBtn = document.getElementById('uploadBtn');
        const progress = document.getElementById('progress');
        const progressFill = document.getElementById('progressFill');
        const status = document.getElementById('status');

        form.addEventListener('submit', async (e) => {
            e.preventDefault();

            const file = fileInput.files[0];
            if (!file) {
                showStatus('Please select a file', 'error');
                return;
            }

            if (!file.name.endsWith('.bin')) {
                showStatus('Please select a .bin file', 'error');
                return;
            }

            uploadBtn.disabled = true;
            progress.style.display = 'block';
            status.innerHTML = '';

            try {
                const xhr = new XMLHttpRequest();

                xhr.upload.addEventListener('progress', (e) => {
                    if (e.lengthComputable) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        progressFill.style.width = percent + '%';
                        progressFill.textContent = percent + '%';
                    }
                });

                xhr.addEventListener('load', () => {
                    if (xhr.status === 200) {
                        showStatus('Upload successful! Device rebooting...', 'success');
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    } else {
                        showStatus('Upload failed: ' + xhr.responseText, 'error');
                        uploadBtn.disabled = false;
                    }
                });

                xhr.addEventListener('error', () => {
                    showStatus('Upload failed: Network error', 'error');
                    uploadBtn.disabled = false;
                });

                xhr.open('POST', '/update', true);
                xhr.send(file);
            } catch (err) {
                showStatus('Upload failed: ' + err.message, 'error');
                uploadBtn.disabled = false;
            }
        });

        function showStatus(message, type) {
            status.innerHTML = message;
            status.className = 'status ' + type;
        }
    </script>
</body>
</html>
)html";
}

/// @brief Simple Base64 decoder for Basic Auth
/// @param input Base64 encoded string
/// @param output Output buffer (must be at least (inputLen * 3 / 4) + 1 bytes)
/// @param max_output_len Maximum size of output buffer
/// @return Length of decoded data, or -1 on error
int decodeBase64(const char* input, char* output, size_t max_output_len) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!input || !output || max_output_len == 0) {
        return -1;
    }

    int in_len = strlen(input);
    if (in_len == 0) {
        output[0] = '\0';
        return 0;
    }

    int out_len = 0;

    for (int i = 0; i < in_len && i + 3 < in_len; i += 4) {
        // Get 4 base64 characters
        const char* pos_a = strchr(base64_chars, input[i]);
        const char* pos_b = strchr(base64_chars, input[i + 1]);
        const char* pos_c = (input[i + 2] == '=') ? nullptr : strchr(base64_chars, input[i + 2]);
        const char* pos_d = (input[i + 3] == '=') ? nullptr : strchr(base64_chars, input[i + 3]);

        // Check for invalid characters
        if (!pos_a || !pos_b) {
            return -1;
        }

        uint32_t sextet_a = pos_a - base64_chars;
        uint32_t sextet_b = pos_b - base64_chars;
        uint32_t sextet_c = pos_c ? (pos_c - base64_chars) : 0;
        uint32_t sextet_d = pos_d ? (pos_d - base64_chars) : 0;

        // Combine into 3 bytes
        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        // Write output bytes with bounds checking
        if (out_len + 1 <= (int)max_output_len) {
            output[out_len++] = (triple >> 16) & 0xFF;
        } else {
            return -1;  // Buffer overflow
        }

        if (input[i + 2] != '=' && out_len + 1 <= (int)max_output_len) {
            output[out_len++] = (triple >> 8) & 0xFF;
        } else if (input[i + 2] != '=') {
            return -1;  // Buffer overflow
        }

        if (input[i + 3] != '=' && out_len + 1 <= (int)max_output_len) {
            output[out_len++] = triple & 0xFF;
        } else if (input[i + 3] != '=') {
            return -1;  // Buffer overflow
        }
    }

    // Null terminate
    if (out_len < (int)max_output_len) {
        output[out_len] = '\0';
    } else {
        return -1;  // No space for null terminator
    }

    return out_len;
}

/// @brief Check Basic Authentication for HTTP request
/// @param req HTTP request handle
/// @param password Expected password
/// @return true if authentication successful, false otherwise
bool checkBasicAuth(httpd_req_t *req, const char* password) {
    // Check Basic Auth
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        // No auth header, request authentication
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
        return false;
    }

    // Get the auth header value
    char* auth_value = (char*)malloc(auth_len + 1);
    if (!auth_value) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return false;
    }

    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_value, auth_len + 1) != ESP_OK) {
        free(auth_value);
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid authentication");
        return false;
    }

    // Verify Basic Auth format: "Basic <base64>"
    if (strncmp(auth_value, "Basic ", 6) != 0) {
        free(auth_value);
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid authentication format");
        return false;
    }

    // Decode Base64 credentials
    char decoded[256];
    int decoded_len = decodeBase64(auth_value + 6, decoded, sizeof(decoded));
    free(auth_value);

    if (decoded_len < 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid Base64 encoding");
        return false;
    }

    // Expected format: "admin:password"
    // Find the colon separator
    char* colon = strchr(decoded, ':');
    if (!colon) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid credentials format");
        return false;
    }

    // Split username and password
    *colon = '\0';
    const char* username = decoded;
    const char* user_password = colon + 1;

    // Verify username is "admin" and password matches
    if (strcmp(username, "admin") != 0 || strcmp(user_password, password) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid credentials");
        return false;
    }

    return true;
}

/// @brief Validate ESP32 firmware image header
/// @param data Pointer to firmware data (at least 24 bytes)
/// @param len Length of firmware data
/// @return true if firmware header is valid, false otherwise
bool validateESP32Firmware(const uint8_t* data, size_t len) {
    // Need at least 24 bytes for ESP32 image header
    if (len < 24) {
        FL_WARN("Firmware validation: header too small (" << len << " bytes)");
        return false;
    }

    // Check ESP32 magic byte (0xE9)
    if (data[0] != 0xE9) {
        FL_WARN("Firmware validation: invalid magic byte 0x"
                << (int)data[0] << " (expected 0xE9)");
        return false;
    }

    // Check segment count is reasonable (1-16)
    uint8_t segments = data[1];
    if (segments == 0 || segments > 16) {
        FL_WARN("Firmware validation: invalid segment count " << (int)segments);
        return false;
    }

    FL_DBG("Firmware validation passed: magic=0xE9, segments=" << (int)segments);
    return true;
}

/// @brief HTTP handler for GET requests to root path (serves upload page)
/// @param req HTTP request handle
/// @return ESP_OK on success
esp_err_t otaHttpGetHandler(httpd_req_t *req) {
    // Get HTTP context from user context
    OTAHttpContext* ctx = (OTAHttpContext*)req->user_ctx;
    const char* password = ctx->password;

    // Check authentication
    if (!checkBasicAuth(req, password)) {
        return ESP_OK;  // Response already sent by checkBasicAuth
    }

    // Authentication successful - serve the OTA upload page
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, getOtaHtmlPage(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/// @brief HTTP handler for POST requests to /update (firmware upload)
/// @param req HTTP request handle
/// @return ESP_OK on success
esp_err_t otaHttpPostHandler(httpd_req_t *req) {
    // Get HTTP context from user context
    OTAHttpContext* ctx = (OTAHttpContext*)req->user_ctx;

    // SECURITY: Require authentication for firmware upload
    if (!checkBasicAuth(req, ctx->password)) {
        return ESP_FAIL;  // Response already sent by checkBasicAuth
    }

    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t* update_partition = nullptr;
    bool ota_started = false;
    size_t total_received = 0;

    // Get expected content length for progress tracking
    size_t content_length = req->content_len;

    // Get OTA partition
    update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition) {
        if (ctx->error_cb && *ctx->error_cb) {
            (*ctx->error_cb)("No OTA partition found");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition found");
        return ESP_FAIL;
    }

    // Receive data in chunks
    char buffer[1024];
    int received;
    bool first_chunk = true;
    esp_err_t err;

    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
        // Validate firmware header on first chunk
        if (first_chunk) {
            if (!validateESP32Firmware((const uint8_t*)buffer, received)) {
                if (ctx->error_cb && *ctx->error_cb) {
                    (*ctx->error_cb)("Invalid ESP32 firmware image");
                }
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ESP32 firmware image");
                return ESP_FAIL;
            }

            // Begin OTA after validation passes
            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (err != ESP_OK) {
                if (ctx->error_cb && *ctx->error_cb) {
                    (*ctx->error_cb)("OTA begin failed");
                }
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
                return ESP_FAIL;
            }
            ota_started = true;
            first_chunk = false;
        }

        // Write to flash
        err = esp_ota_write(ota_handle, buffer, received);
        if (err != ESP_OK) {
            if (ctx->error_cb && *ctx->error_cb) {
                (*ctx->error_cb)("OTA write failed");
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        total_received += received;

        // Call progress callback if available
        if (ctx->progress_cb && *ctx->progress_cb && content_length > 0) {
            (*ctx->progress_cb)(total_received, content_length);
        }
    }

    if (received < 0) {
        // Error receiving data
        if (ctx->error_cb && *ctx->error_cb) {
            (*ctx->error_cb)("Upload interrupted");
        }
        if (ota_started) {
            esp_ota_abort(ota_handle);
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload interrupted");
        return ESP_FAIL;
    }

    // Finalize OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (ctx->error_cb && *ctx->error_cb) {
            (*ctx->error_cb)("OTA end failed");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        if (ctx->error_cb && *ctx->error_cb) {
            (*ctx->error_cb)("Failed to set boot partition");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set boot partition");
        return ESP_FAIL;
    }

    // Success
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    // Call before-reboot callback if configured
    if (ctx->before_reboot_cb && *ctx->before_reboot_cb) {
        (*ctx->before_reboot_cb)();
    }

    // Reboot after a short delay (allow response to be sent)
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

/// @brief Start HTTP server for Web OTA
/// @param ctx HTTP context (password and callbacks)
/// @return httpd_handle_t on success, nullptr on failure
httpd_handle_t startHttpServer(OTAHttpContext* ctx) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;  // Control port for stopping server
    config.max_uri_handlers = 2;  // Only need GET / and POST /update

    httpd_handle_t server = nullptr;

    if (httpd_start(&server, &config) != ESP_OK) {
        return nullptr;
    }

    // Register GET handler for root path
    httpd_uri_t uri_get = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = otaHttpGetHandler,
        .user_ctx  = (void*)ctx  // Pass context for auth check and callbacks
    };
    httpd_register_uri_handler(server, &uri_get);

    // Register POST handler for /update
    httpd_uri_t uri_post = {
        .uri       = "/update",
        .method    = HTTP_POST,
        .handler   = otaHttpPostHandler,
        .user_ctx  = (void*)ctx  // Pass context for auth check and callbacks
    };
    httpd_register_uri_handler(server, &uri_post);

    return server;
}

}  // anonymous namespace

// ============================================================================
// ESP32 OTA Implementation
// ============================================================================

class ESP32OTA : public IOTA {
public:
    ESP32OTA()
        : mApFallbackEnabled(false)
        , mHttpServer(nullptr)
        , mFailedServices(0)
        , mWifiConnected(false)
    {
        mHttpContext.password = nullptr;
        mHttpContext.progress_cb = &mProgressCb;
        mHttpContext.error_cb = &mErrorCb;
        mHttpContext.before_reboot_cb = &mBeforeRebootCb;
    }

    ~ESP32OTA() override {
        cleanup();
    }

    bool beginWiFi(const char* hostname, const char* password,
                   const char* ssid, const char* wifi_pass) override {
        // Stop any existing HTTP server to prevent leaks
        stopHttpServer();

        // Reset failure tracking
        mFailedServices = 0;

        // Store configuration strings safely
        mHostname = hostname ? hostname : "";
        mPassword = password ? password : "";
        mHttpContext.password = mPassword.c_str();

        // Connect to Wi-Fi using ESP-IDF WiFi API (async mode)
        if (!initEspIdfWifi(ssid, wifi_pass)) {
            FL_WARN("ESP-IDF WiFi initialization failed");
            // Continue anyway - some services might still work
        }

        // Async mode: Return immediately, user polls isConnected()
        // Note: We initialize services even if not connected yet

        // Initialize mDNS
        if (!initMDNS(mHostname.c_str())) {
            FL_WARN("mDNS init failed - device won't be discoverable at " << mHostname.c_str() << ".local");
            mFailedServices |= (uint8_t)fl::OTAService::MDNS_FAILED;
        }

        // Setup custom ESP-IDF OTA server (TCP listener on port 3232)
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        mHttpServer = startHttpServer(&mHttpContext);
        if (!mHttpServer) {
            FL_WARN("HTTP server failed - Web OTA unavailable (TCP OTA still works)");
            mFailedServices |= (uint8_t)fl::OTAService::HTTP_FAILED;
        }

        return true;
    }

    bool begin(const char* hostname, const char* password) override {
        // Stop any existing HTTP server to prevent leaks
        stopHttpServer();

        // Reset failure tracking
        mFailedServices = 0;

        // Store configuration strings safely
        mHostname = hostname ? hostname : "";
        mPassword = password ? password : "";
        mHttpContext.password = mPassword.c_str();

        // Assume network is already configured
        // Just start OTA services

        // Initialize mDNS
        if (!initMDNS(mHostname.c_str())) {
            FL_WARN("mDNS init failed - device won't be discoverable at " << mHostname.c_str() << ".local");
            mFailedServices |= (uint8_t)fl::OTAService::MDNS_FAILED;
        }

        // Setup custom ESP-IDF OTA server (TCP listener on port 3232)
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        mHttpServer = startHttpServer(&mHttpContext);
        if (!mHttpServer) {
            FL_WARN("HTTP server failed - Web OTA unavailable (TCP OTA still works)");
            mFailedServices |= (uint8_t)fl::OTAService::HTTP_FAILED;
        }

        return true;
    }

    bool enableApFallback(const char* ap_ssid, const char* ap_pass) override {
        // Validate SSID
        if (!ap_ssid || strlen(ap_ssid) == 0) {
            FL_WARN("AP SSID cannot be empty");
            return false;
        }

        // Validate password (WPA2 requires minimum 8 characters)
        if (ap_pass && strlen(ap_pass) > 0 && strlen(ap_pass) < 8) {
            FL_WARN("AP password must be at least 8 characters or nullptr for open network");
            return false;
        }

        mApFallbackEnabled = true;
        mApSsid = ap_ssid;
        mApPass = ap_pass ? ap_pass : "";
        return true;
    }

    void onProgress(fl::function<void(size_t, size_t)> callback) override {
        mProgressCb = callback;
    }

    void onError(fl::function<void(const char*)> callback) override {
        mErrorCb = callback;
    }

    void onState(fl::function<void(uint8_t)> callback) override {
        mStateCb = callback;
    }

    void onBeforeReboot(void (*callback)()) override {
        mBeforeRebootCb = callback;
    }

    void poll() override {
        // Custom OTA server runs in separate FreeRTOS task (zero polling overhead)
        // HTTP server also runs in separate FreeRTOS task
        // Nothing to poll
    }

    bool isConnected() const override {
        return mWifiConnected;
    }

    uint8_t getFailedServices() const override {
        return mFailedServices;
    }

private:
    /// @brief WiFi event handler for connection state tracking
    /// @param arg User data (pointer to ESP32OTA instance)
    /// @param event_base Event base (WIFI_EVENT or IP_EVENT)
    /// @param event_id Event ID (e.g., WIFI_EVENT_STA_CONNECTED)
    /// @param event_data Event-specific data
    static void wifiEventHandler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
        ESP32OTA* self = static_cast<ESP32OTA*>(arg);

        if (event_base == WIFI_EVENT) {
            if (event_id == WIFI_EVENT_STA_CONNECTED) {
                FL_DBG("WiFi: Station connected to AP");
            } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                FL_DBG("WiFi: Station disconnected from AP");
                self->mWifiConnected = false;
            }
        } else if (event_base == IP_EVENT) {
            if (event_id == IP_EVENT_STA_GOT_IP) {
                ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
                FL_DBG("WiFi: Got IP address: " << ip4addr_ntoa(&event->ip_info.ip));
                self->mWifiConnected = true;
            }
        }
    }

    /// @brief Initialize ESP-IDF WiFi with STA mode
    /// @param ssid WiFi SSID
    /// @param password WiFi password
    /// @return true if initialization successful, false otherwise
    bool initEspIdfWifi(const char* ssid, const char* password) {
        // Initialize network interface (if not already initialized)
        // Note: This is safe to call multiple times
        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("esp_netif_init failed: " << esp_err_to_name(err));
            return false;
        }

        // Create default event loop (if not already created)
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("esp_event_loop_create_default failed: " << esp_err_to_name(err));
            return false;
        }

        // Create default WiFi STA interface (if not already created)
        // Note: Idempotent - safe to call multiple times
        static esp_netif_t* sta_netif = nullptr;
        if (!sta_netif) {
            sta_netif = esp_netif_create_default_wifi_sta();
            if (!sta_netif) {
                FL_WARN("esp_netif_create_default_wifi_sta failed");
                return false;
            }
        }

        // Register event handlers
        err = esp_event_handler_instance_register(WIFI_EVENT,
                                                    ESP_EVENT_ANY_ID,
                                                    &wifiEventHandler,
                                                    this,
                                                    nullptr);
        if (err != ESP_OK) {
            FL_WARN("Failed to register WIFI_EVENT handler: " << esp_err_to_name(err));
            return false;
        }

        err = esp_event_handler_instance_register(IP_EVENT,
                                                    IP_EVENT_STA_GOT_IP,
                                                    &wifiEventHandler,
                                                    this,
                                                    nullptr);
        if (err != ESP_OK) {
            FL_WARN("Failed to register IP_EVENT handler: " << esp_err_to_name(err));
            return false;
        }

        // Initialize WiFi driver (if not already initialized)
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            FL_WARN("esp_wifi_init failed: " << esp_err_to_name(err));
            return false;
        }

        // Set WiFi mode to STA (Station)
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            FL_WARN("esp_wifi_set_mode failed: " << esp_err_to_name(err));
            return false;
        }

        // Configure WiFi credentials
        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (err != ESP_OK) {
            FL_WARN("esp_wifi_set_config failed: " << esp_err_to_name(err));
            return false;
        }

        // Set hostname (must be done before starting WiFi)
        err = esp_netif_set_hostname(sta_netif, mHostname.c_str());
        if (err != ESP_OK) {
            FL_WARN("esp_netif_set_hostname failed: " << esp_err_to_name(err));
            // Non-fatal, continue
        }

        // Start WiFi
        err = esp_wifi_start();
        if (err != ESP_OK) {
            FL_WARN("esp_wifi_start failed: " << esp_err_to_name(err));
            return false;
        }

        // Connect to AP (async)
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            FL_WARN("esp_wifi_connect failed: " << esp_err_to_name(err));
            return false;
        }

        FL_DBG("ESP-IDF WiFi initialization successful");
        return true;
    }

    /// @brief Generate authentication nonce for OTA
    /// @param nonce Output buffer for nonce (64 characters for SHA256)
    static void generateNonce(char* nonce, size_t len) {
        // Generate nonce from timestamp and random data
        unsigned char hash[32];
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)

        uint32_t seed = esp_random();
        mbedtls_sha256_update(&ctx, (unsigned char*)&seed, sizeof(seed));

        int64_t time_us = esp_timer_get_time();
        mbedtls_sha256_update(&ctx, (unsigned char*)&time_us, sizeof(time_us));

        mbedtls_sha256_finish(&ctx, hash);
        mbedtls_sha256_free(&ctx);

        // Convert to hex string
        for (size_t i = 0; i < 32 && i * 2 + 1 < len; i++) {
            snprintf(nonce + i * 2, 3, "%02x", hash[i]);
        }
    }

    /// @brief Verify OTA authentication response
    /// @param password Password to verify against
    /// @param nonce Server nonce (64 chars hex)
    /// @param cnonce Client nonce from response
    /// @param response Client response to verify
    /// @return true if authentication successful
    static bool verifyAuth(const char* password, const char* nonce,
                          const char* cnonce, const char* response) {
        // Compute password hash (SHA256)
        unsigned char pass_hash[32];
        mbedtls_sha256((unsigned char*)password, strlen(password), pass_hash, 0);

        // Convert password hash to hex
        char pass_hash_hex[65];
        for (int i = 0; i < 32; i++) {
            snprintf(pass_hash_hex + i * 2, 3, "%02x", pass_hash[i]);
        }
        pass_hash_hex[64] = '\0';

        // Derive key using simple iteration (simplified PBKDF2-like)
        unsigned char derived_key[32];
        memcpy(derived_key, pass_hash, 32);
        for (int i = 0; i < 1000; i++) {  // 1000 iterations (lighter than 10000)
            mbedtls_sha256(derived_key, 32, derived_key, 0);
        }

        // Convert derived key to hex
        char derived_key_hex[65];
        for (int i = 0; i < 32; i++) {
            snprintf(derived_key_hex + i * 2, 3, "%02x", derived_key[i]);
        }
        derived_key_hex[64] = '\0';

        // Compute expected response: SHA256(derived_key_hex:nonce:cnonce)
        char auth_string[256];
        snprintf(auth_string, sizeof(auth_string), "%s:%s:%s",
                derived_key_hex, nonce, cnonce);

        unsigned char expected_hash[32];
        mbedtls_sha256((unsigned char*)auth_string, strlen(auth_string),
                      expected_hash, 0);

        char expected_response[65];
        for (int i = 0; i < 32; i++) {
            snprintf(expected_response + i * 2, 3, "%02x", expected_hash[i]);
        }
        expected_response[64] = '\0';

        return strcmp(response, expected_response) == 0;
    }

    /// @brief Handle TCP firmware upload after successful invitation/auth
    /// @param client_addr UDP client address
    /// @param port TCP port where client is listening
    /// @param expected_size Expected firmware size in bytes
    /// @param expected_md5 Expected MD5 hash (32-char hex string)
    /// @param cmd OTA command (0=FLASH, 100=SPIFFS, etc.)
    void handleFirmwareUpload(struct sockaddr_in* client_addr, int port,
                             int expected_size, const char* expected_md5, int cmd) {
        // Only handle FLASH command (0) for now
        if (cmd != 0) {
            FL_WARN("OTA: Unsupported command " << cmd << " (only FLASH supported)");
            if (mErrorCb) {
                mErrorCb("Unsupported OTA command");
            }
            return;
        }

        // Call start state callback
        if (mStateCb) {
            mStateCb(1);  // OTA_START
        }

        // Create TCP socket to connect to client
        int tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcp_socket < 0) {
            FL_WARN("OTA: Failed to create TCP socket");
            if (mErrorCb) {
                mErrorCb("TCP socket creation failed");
            }
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        // Set socket timeout to 10 seconds
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Connect to client's TCP server
        struct sockaddr_in tcp_addr;
        memcpy(&tcp_addr, client_addr, sizeof(struct sockaddr_in));
        tcp_addr.sin_port = htons(port);

        FL_DBG("OTA: Connecting to client TCP server on port " << port);
        if (connect(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
            FL_WARN("OTA: Failed to connect to client TCP server");
            if (mErrorCb) {
                mErrorCb("TCP connection failed");
            }
            close(tcp_socket);
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        FL_DBG("OTA: TCP connected, receiving firmware (" << expected_size << " bytes)");

        // Get OTA partition
        const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
        if (!update_partition) {
            FL_WARN("OTA: No OTA partition found");
            if (mErrorCb) {
                mErrorCb("No OTA partition");
            }
            close(tcp_socket);
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        // Begin OTA operation
        esp_ota_handle_t ota_handle;
        esp_err_t err = esp_ota_begin(update_partition, expected_size, &ota_handle);
        if (err != ESP_OK) {
            FL_WARN("OTA: esp_ota_begin failed: " << esp_err_to_name(err));
            if (mErrorCb) {
                mErrorCb("OTA begin failed");
            }
            close(tcp_socket);
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        // Initialize MD5 context for verification
        mbedtls_md5_context md5_ctx;
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);

        // Receive and write firmware data
        uint8_t buffer[1024];
        size_t total_received = 0;
        bool write_error = false;

        while (total_received < (size_t)expected_size) {
            int remaining = expected_size - total_received;
            int to_recv = (remaining < (int)sizeof(buffer)) ? remaining : (int)sizeof(buffer);

            int received = recv(tcp_socket, buffer, to_recv, 0);
            if (received <= 0) {
                FL_WARN("OTA: TCP receive error or timeout");
                if (mErrorCb) {
                    mErrorCb("Upload interrupted");
                }
                write_error = true;
                break;
            }

            // Update MD5 hash
            mbedtls_md5_update(&md5_ctx, buffer, received);

            // Write to flash
            err = esp_ota_write(ota_handle, buffer, received);
            if (err != ESP_OK) {
                FL_WARN("OTA: esp_ota_write failed: " << esp_err_to_name(err));
                if (mErrorCb) {
                    mErrorCb("Flash write failed");
                }
                write_error = true;
                break;
            }

            total_received += received;

            // Call progress callback
            if (mProgressCb) {
                mProgressCb(total_received, expected_size);
            }
        }

        close(tcp_socket);

        // Check for errors
        if (write_error) {
            esp_ota_abort(ota_handle);
            mbedtls_md5_free(&md5_ctx);
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        // Finalize MD5 and verify
        unsigned char md5_hash[16];
        mbedtls_md5_finish(&md5_ctx, md5_hash);
        mbedtls_md5_free(&md5_ctx);

        char computed_md5[33];
        for (int i = 0; i < 16; i++) {
            snprintf(computed_md5 + i * 2, 3, "%02x", md5_hash[i]);
        }
        computed_md5[32] = '\0';

        FL_DBG("OTA: Expected MD5: " << expected_md5);
        FL_DBG("OTA: Computed MD5: " << computed_md5);

        if (strcmp(computed_md5, expected_md5) != 0) {
            FL_WARN("OTA: MD5 mismatch!");
            if (mErrorCb) {
                mErrorCb("MD5 verification failed");
            }
            esp_ota_abort(ota_handle);
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        FL_DBG("OTA: MD5 verification passed");

        // Finalize OTA
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            FL_WARN("OTA: esp_ota_end failed: " << esp_err_to_name(err));
            if (mErrorCb) {
                mErrorCb("OTA finalization failed");
            }
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        // Set boot partition
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            FL_WARN("OTA: Failed to set boot partition: " << esp_err_to_name(err));
            if (mErrorCb) {
                mErrorCb("Failed to set boot partition");
            }
            if (mStateCb) {
                mStateCb(3);  // OTA_ERROR
            }
            return;
        }

        FL_DBG("OTA: Firmware update successful!");

        // Call end state callback
        if (mStateCb) {
            mStateCb(2);  // OTA_END
        }

        // Call before-reboot callback
        if (mBeforeRebootCb) {
            mBeforeRebootCb();
        }

        // Reboot after short delay
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    /// @brief OTA UDP server task - handles OTA invitations and protocol
    /// @param pvParameters Pointer to ESP32OTA instance
    static void otaServerTask(void* pvParameters) {
        ESP32OTA* self = static_cast<ESP32OTA*>(pvParameters);

        // Create UDP socket
        self->mOtaUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (self->mOtaUdpSocket < 0) {
            FL_WARN("OTA: Failed to create UDP socket");
            self->mOtaRunning = false;
            vTaskDelete(nullptr);
            return;
        }

        // Bind to port 3232
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(3232);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(self->mOtaUdpSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            FL_WARN("OTA: Failed to bind UDP socket to port 3232");
            close(self->mOtaUdpSocket);
            self->mOtaUdpSocket = -1;
            self->mOtaRunning = false;
            vTaskDelete(nullptr);
            return;
        }

        FL_DBG("OTA: UDP server listening on port 3232");

        // Set receive timeout for responsive shutdown
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(self->mOtaUdpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        char buffer[512];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        while (self->mOtaRunning) {
            int len = recvfrom(self->mOtaUdpSocket, buffer, sizeof(buffer) - 1, 0,
                              (struct sockaddr*)&client_addr, &client_len);

            if (len <= 0) {
                continue;  // Timeout or error, check mOtaRunning flag
            }

            buffer[len] = '\0';
            FL_DBG("OTA: Received UDP packet: " << buffer);

            // Parse command: "<cmd> <port> <size> <md5>\n"
            int cmd, port, size;
            char md5[33];
            if (sscanf(buffer, "%d %d %d %32s", &cmd, &port, &size, md5) != 4) {
                FL_WARN("OTA: Invalid invitation format");
                continue;
            }

            // Check if password is required
            const char* response_msg;
            if (self->mPassword.length() > 0) {
                // Generate nonce and send AUTH challenge
                char nonce[65];
                generateNonce(nonce, sizeof(nonce));
                self->mOtaNonce = nonce;

                char auth_response[128];
                snprintf(auth_response, sizeof(auth_response), "AUTH %s", nonce);
                sendto(self->mOtaUdpSocket, auth_response, strlen(auth_response), 0,
                      (struct sockaddr*)&client_addr, client_len);
                FL_DBG("OTA: Sent AUTH challenge");

                // Wait for authentication response (with timeout)
                len = recvfrom(self->mOtaUdpSocket, buffer, sizeof(buffer) - 1, 0,
                              (struct sockaddr*)&client_addr, &client_len);
                if (len <= 0) {
                    FL_WARN("OTA: Authentication timeout");
                    continue;
                }

                buffer[len] = '\0';
                FL_DBG("OTA: Received auth response: " << buffer);

                // Parse auth response: "200 <cnonce> <response>\n"
                int auth_cmd;
                char cnonce[65], auth_hash[65];
                if (sscanf(buffer, "%d %64s %64s", &auth_cmd, cnonce, auth_hash) != 3 || auth_cmd != 200) {
                    FL_WARN("OTA: Invalid auth response format");
                    sendto(self->mOtaUdpSocket, "FAIL", 4, 0,
                          (struct sockaddr*)&client_addr, client_len);
                    continue;
                }

                // Verify authentication
                if (!verifyAuth(self->mPassword.c_str(), nonce, cnonce, auth_hash)) {
                    FL_WARN("OTA: Authentication failed");
                    if (self->mErrorCb) {
                        self->mErrorCb("Auth Failed");
                    }
                    sendto(self->mOtaUdpSocket, "FAIL", 4, 0,
                          (struct sockaddr*)&client_addr, client_len);
                    continue;
                }

                FL_DBG("OTA: Authentication successful");
            }

            // Send OK response
            sendto(self->mOtaUdpSocket, "OK", 2, 0,
                  (struct sockaddr*)&client_addr, client_len);

            // Handle TCP connection for firmware upload
            FL_DBG("OTA: Ready for TCP connection on client port " << port);
            self->handleFirmwareUpload(&client_addr, port, size, md5, cmd);
        }

        close(self->mOtaUdpSocket);
        self->mOtaUdpSocket = -1;
        FL_DBG("OTA: UDP server stopped");
        vTaskDelete(nullptr);
    }

    void setupArduinoOTA() {
        // Start custom OTA server (replaces ArduinoOTA)
        if (mOtaRunning) {
            FL_WARN("OTA: Server already running");
            return;
        }

        mOtaRunning = true;

        // Create OTA server task
        BaseType_t result = xTaskCreate(
            otaServerTask,
            "ota_server",
            4096,  // Stack size
            this,  // Parameter (ESP32OTA instance)
            5,     // Priority
            &mOtaServerTask
        );

        if (result != pdPASS) {
            FL_WARN("OTA: Failed to create server task");
            mOtaRunning = false;
            mFailedServices |= static_cast<uint8_t>(OTAService::IDE);
        } else {
            FL_DBG("OTA: Custom server started (port 3232)");
        }
    }

    void stopHttpServer() {
        if (mHttpServer) {
            httpd_stop(mHttpServer);
            mHttpServer = nullptr;
        }
    }

    void cleanup() {
        stopHttpServer();

        // Stop custom OTA server
        if (mOtaRunning) {
            mOtaRunning = false;

            // Wait for task to finish (it will exit on next timeout)
            if (mOtaServerTask) {
                // Give it 2 seconds to gracefully shut down
                for (int i = 0; i < 20 && mOtaServerTask; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                mOtaServerTask = nullptr;
            }

            // Close socket if still open
            if (mOtaUdpSocket >= 0) {
                close(mOtaUdpSocket);
                mOtaUdpSocket = -1;
            }

            FL_DBG("OTA: Custom server stopped");
        }
    }

    // Configuration - using StrN for safe string storage
    fl::StrN<64> mHostname;
    fl::StrN<64> mPassword;
    fl::StrN<32> mApSsid;
    fl::StrN<64> mApPass;
    bool mApFallbackEnabled;
    bool mWifiConnected;

    // Callbacks
    fl::function<void(size_t, size_t)> mProgressCb;
    fl::function<void(const char*)> mErrorCb;
    fl::function<void(uint8_t)> mStateCb;
    void (*mBeforeRebootCb)() = nullptr;

    // HTTP server handle
    httpd_handle_t mHttpServer;

    // HTTP context (shared with handlers)
    OTAHttpContext mHttpContext;

    // Service initialization status
    uint8_t mFailedServices;

    // Custom ESP-IDF OTA server state (pure ESP-IDF, no Arduino)
    int mOtaUdpSocket = -1;              // UDP socket for OTA invitations
    TaskHandle_t mOtaServerTask = nullptr;  // FreeRTOS task handle for OTA server
    fl::StrN<64> mOtaNonce;              // Authentication nonce
    bool mOtaRunning = false;            // OTA server running flag
};

// ============================================================================
// Strong Override - ESP32 Factory
// ============================================================================

fl::shared_ptr<IOTA> platform_create_ota() {
    return fl::make_shared<ESP32OTA>();
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_ESP_OTA_SUPPORTED

#endif  // ESP32
