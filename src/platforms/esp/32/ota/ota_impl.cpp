/// @file platforms/esp/32/ota/ota_impl.cpp
/// ESP32-specific OTA implementation

#if defined(ESP32)

#include "platforms/ota.h"
#include "platforms/esp/esp_version.h"

// OTA requires IDF 4.0 or higher for HTTP server and OTA APIs
// For IDF 3.3 and earlier, fall back to null implementation
#if ESP_IDF_VERSION_4_OR_HIGHER

// ESP-IDF headers
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <mdns.h>

// Arduino headers
#include <WiFi.h>
#include <ETH.h>
#include <ArduinoOTA.h>

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

/// @brief HTTP handler for GET requests to root path (serves upload page)
/// @param req HTTP request handle
/// @return ESP_OK on success
esp_err_t otaHttpGetHandler(httpd_req_t *req) {
    // Check Basic Auth
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        // No auth header, request authentication
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
        return ESP_OK;
    }

    // Get the auth header value
    char* auth_value = (char*)malloc(auth_len + 1);
    if (!auth_value) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_value, auth_len + 1) != ESP_OK) {
        free(auth_value);
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid authentication");
        return ESP_OK;
    }

    // Get HTTP context from user context
    OTAHttpContext* ctx = (OTAHttpContext*)req->user_ctx;
    const char* password = ctx->password;

    // Verify Basic Auth format: "Basic <base64>"
    if (strncmp(auth_value, "Basic ", 6) != 0) {
        free(auth_value);
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid authentication format");
        return ESP_OK;
    }

    // Decode Base64 credentials
    char decoded[256];
    int decoded_len = decodeBase64(auth_value + 6, decoded, sizeof(decoded));
    free(auth_value);

    if (decoded_len < 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid Base64 encoding");
        return ESP_OK;
    }

    // Expected format: "admin:password"
    // Find the colon separator
    char* colon = strchr(decoded, ':');
    if (!colon) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid credentials format");
        return ESP_OK;
    }

    // Split username and password
    *colon = '\0';
    const char* username = decoded;
    const char* user_password = colon + 1;

    // Verify username is "admin" and password matches
    if (strcmp(username, "admin") != 0 || strcmp(user_password, password) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid credentials");
        return ESP_OK;
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
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t* update_partition = nullptr;
    bool ota_started = false;
    size_t total_received = 0;

    // Get HTTP context from user context
    OTAHttpContext* ctx = (OTAHttpContext*)req->user_ctx;

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

    // Begin OTA
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        if (ctx->error_cb && *ctx->error_cb) {
            (*ctx->error_cb)("OTA begin failed");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    ota_started = true;

    // Receive data in chunks
    char buffer[1024];
    int received;

    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
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
        esp_ota_abort(ota_handle);
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
        : hostname_(nullptr)
        , password_(nullptr)
        , ap_ssid_(nullptr)
        , ap_pass_(nullptr)
        , ap_fallback_enabled_(false)
        , http_server_(nullptr)
    {
        http_context_.password = nullptr;
        http_context_.progress_cb = &progress_cb_;
        http_context_.error_cb = &error_cb_;
    }

    ~ESP32OTA() override {
        cleanup();
    }

    bool beginWiFi(const char* hostname, const char* password,
                   const char* ssid, const char* wifi_pass) override {
        hostname_ = hostname;
        password_ = password;
        http_context_.password = password;

        // Connect to Wi-Fi using Arduino WiFi library
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(hostname);
        WiFi.begin(ssid, wifi_pass);

        // Wait for connection with timeout (30 seconds)
        int timeout = 30;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(1000);
            timeout--;
        }

        if (WiFi.status() != WL_CONNECTED) {
            // Connection failed
            if (ap_fallback_enabled_) {
                // Try AP fallback mode
                WiFi.mode(WIFI_AP);
                WiFi.softAP(ap_ssid_, ap_pass_);
            } else {
                return false;
            }
        }

        // Initialize mDNS
        if (!initMDNS(hostname)) {
            // mDNS failed, but continue anyway
        }

        // Setup ArduinoOTA
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        http_server_ = startHttpServer(&http_context_);
        if (!http_server_) {
            // HTTP server failed, but ArduinoOTA still works
        }

        return true;
    }

    bool beginEthernet(const char* hostname, const char* password) override {
        hostname_ = hostname;
        password_ = password;
        http_context_.password = password;

        // Start Ethernet using Arduino ETH library
        // This uses ESP32 internal EMAC
        if (!ETH.begin()) {
            return false;
        }

        // Set hostname
        ETH.setHostname(hostname);

        // Wait for link with timeout (10 seconds)
        int timeout = 10;
        while (!ETH.linkUp() && timeout > 0) {
            delay(1000);
            timeout--;
        }

        if (!ETH.linkUp()) {
            return false;
        }

        // Initialize mDNS
        if (!initMDNS(hostname)) {
            // mDNS failed, but continue anyway
        }

        // Setup ArduinoOTA
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        http_server_ = startHttpServer(&http_context_);
        if (!http_server_) {
            // HTTP server failed, but ArduinoOTA still works
        }

        return true;
    }

    bool begin(const char* hostname, const char* password) override {
        hostname_ = hostname;
        password_ = password;
        http_context_.password = password;

        // Assume network is already configured
        // Just start OTA services

        // Initialize mDNS
        if (!initMDNS(hostname)) {
            // mDNS failed, but continue anyway
        }

        // Setup ArduinoOTA
        setupArduinoOTA();

        // Start HTTP server for Web OTA
        http_server_ = startHttpServer(&http_context_);
        if (!http_server_) {
            // HTTP server failed, but ArduinoOTA still works
        }

        return true;
    }

    void enableApFallback(const char* ap_ssid, const char* ap_pass) override {
        ap_fallback_enabled_ = true;
        ap_ssid_ = ap_ssid;
        ap_pass_ = ap_pass;
    }

    void onProgress(fl::function<void(size_t, size_t)> callback) override {
        progress_cb_ = callback;
    }

    void onError(fl::function<void(const char*)> callback) override {
        error_cb_ = callback;
    }

    void onState(fl::function<void(uint8_t)> callback) override {
        state_cb_ = callback;
    }

    void poll() override {
        // Only need to handle ArduinoOTA
        // HTTP server runs in separate FreeRTOS task (zero polling overhead)
        ArduinoOTA.handle();
    }

private:
    void setupArduinoOTA() {
        ArduinoOTA.setHostname(hostname_);
        ArduinoOTA.setPassword(password_);

        ArduinoOTA.onStart([this]() {
            if (state_cb_) {
                state_cb_(1);  // State: Start
            }
        });

        ArduinoOTA.onEnd([this]() {
            if (state_cb_) {
                state_cb_(2);  // State: End
            }
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (progress_cb_) {
                progress_cb_(progress, total);
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            if (error_cb_) {
                const char* msg = "Unknown error";
                switch (error) {
                    case OTA_AUTH_ERROR: msg = "Auth Failed"; break;
                    case OTA_BEGIN_ERROR: msg = "Begin Failed"; break;
                    case OTA_CONNECT_ERROR: msg = "Connect Failed"; break;
                    case OTA_RECEIVE_ERROR: msg = "Receive Failed"; break;
                    case OTA_END_ERROR: msg = "End Failed"; break;
                }
                error_cb_(msg);
            }
        });

        ArduinoOTA.begin();
    }

    void cleanup() {
        if (http_server_) {
            httpd_stop(http_server_);
            http_server_ = nullptr;
        }
        ArduinoOTA.end();
    }

    // Configuration
    const char* hostname_;
    const char* password_;
    const char* ap_ssid_;
    const char* ap_pass_;
    bool ap_fallback_enabled_;

    // Callbacks
    fl::function<void(size_t, size_t)> progress_cb_;
    fl::function<void(const char*)> error_cb_;
    fl::function<void(uint8_t)> state_cb_;

    // HTTP server handle
    httpd_handle_t http_server_;

    // HTTP context (shared with handlers)
    OTAHttpContext http_context_;
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
