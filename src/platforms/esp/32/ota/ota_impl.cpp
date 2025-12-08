/// @file platforms/esp/32/ota/ota_impl.cpp
/// ESP32-specific OTA implementation

#if defined(ESP32)

#include "platforms/ota.h"
#include "platforms/esp/esp_version.h"
#include "fl/ota.h"  // For OTAService enum

// OTA requires IDF 4.0 or higher for HTTP server and OTA APIs
// For IDF 3.3 and earlier, fall back to null implementation
// ESP32-H2 and ESP32-P4 don't have WiFi, so OTA is not supported
#if ESP_IDF_VERSION_4_OR_HIGHER && !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4)

// ESP-IDF headers
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <mdns.h>

// Arduino headers
#include <WiFi.h>
#include <ArduinoOTA.h>

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
    // Note: In a real implementation, you'd use a FreeRTOS task/timer for this
    delay(1000);
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

        // Connect to Wi-Fi using Arduino WiFi library (async mode)
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(mHostname.c_str());
        WiFi.begin(ssid, wifi_pass);

        // Async mode: Return immediately, user polls isConnected()
        // Note: We initialize services even if not connected yet

        // Initialize mDNS
        if (!initMDNS(mHostname.c_str())) {
            FL_WARN("mDNS init failed - device won't be discoverable at " << mHostname.c_str() << ".local");
            mFailedServices |= (uint8_t)fl::OTAService::MDNS_FAILED;
        }

        // Setup ArduinoOTA
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        mHttpServer = startHttpServer(&mHttpContext);
        if (!mHttpServer) {
            FL_WARN("HTTP server failed - Web OTA unavailable (ArduinoOTA still works)");
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

        // Setup ArduinoOTA
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        mHttpServer = startHttpServer(&mHttpContext);
        if (!mHttpServer) {
            FL_WARN("HTTP server failed - Web OTA unavailable (ArduinoOTA still works)");
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
        // Only need to handle ArduinoOTA
        // HTTP server runs in separate FreeRTOS task (zero polling overhead)
        ArduinoOTA.handle();
    }

    bool isConnected() const override {
        return WiFi.status() == WL_CONNECTED;
    }

    uint8_t getFailedServices() const override {
        return mFailedServices;
    }

private:
    void setupArduinoOTA() {
        ArduinoOTA.setHostname(mHostname.c_str());
        ArduinoOTA.setPassword(mPassword.c_str());

        ArduinoOTA.onStart([this]() {
            if (mStateCb) {
                mStateCb(1);  // State: Start
            }
        });

        ArduinoOTA.onEnd([this]() {
            if (mStateCb) {
                mStateCb(2);  // State: End
            }
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (mProgressCb) {
                mProgressCb(progress, total);
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            if (mErrorCb) {
                const char* msg = "Unknown error";
                switch (error) {
                    case OTA_AUTH_ERROR: msg = "Auth Failed"; break;
                    case OTA_BEGIN_ERROR: msg = "Begin Failed"; break;
                    case OTA_CONNECT_ERROR: msg = "Connect Failed"; break;
                    case OTA_RECEIVE_ERROR: msg = "Receive Failed"; break;
                    case OTA_END_ERROR: msg = "End Failed"; break;
                }
                mErrorCb(msg);
            }
        });

        ArduinoOTA.begin();
    }

    void stopHttpServer() {
        if (mHttpServer) {
            httpd_stop(mHttpServer);
            mHttpServer = nullptr;
        }
    }

    void cleanup() {
        stopHttpServer();
        ArduinoOTA.end();
    }

    // Configuration - using StrN for safe string storage
    fl::StrN<64> mHostname;
    fl::StrN<64> mPassword;
    fl::StrN<32> mApSsid;
    fl::StrN<64> mApPass;
    bool mApFallbackEnabled;

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
};

// ============================================================================
// Strong Override - ESP32 Factory
// ============================================================================

fl::shared_ptr<IOTA> platform_create_ota() {
    return fl::make_shared<ESP32OTA>();
}

}  // namespace platforms
}  // namespace fl

#endif  // ESP_IDF_VERSION_4_OR_HIGHER

#endif  // ESP32
