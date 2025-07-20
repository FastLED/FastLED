# FastLED Networking API Design

## Overview

This document outlines the design for a FastLED networking API that provides BSD-style socket functionality under the `fl::` namespace. The API is designed to work across multiple platforms including native systems, ESP32 (via lwIP), and WebAssembly/Emscripten (via WebSocket/WebRTC).

## Design Goals

- **Consistent API**: Uniform interface across all supported platforms
- **FastLED Style**: Follow FastLED coding conventions and patterns
- **Platform Abstraction**: Hide platform-specific implementation details
- **Type Safety**: Leverage C++ type system for compile-time safety
- **RAII**: Automatic resource management for sockets and connections
- **Header-Only**: Minimize build complexity where possible
- **Embedded-Friendly**: Work well on resource-constrained devices

# HIGH-LEVEL NETWORKING API

## Overview

The high-level networking API provides HTTP client functionality with both async callback and future-based interfaces. It's designed to be non-blocking and integrate seamlessly with FastLED's event system.

## Core Components

### 1. HTTP Client with Header Support

```cpp
#include "fl/networking/http_client.h"

namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    HttpClient();
    ~HttpClient();
    
    // ========== SIMPLIFIED HEADER API ==========
    
    /// HTTP GET request with customizable timeout and headers (returns future)
    /// @param url the URL to request
    /// @param timeout_ms request timeout in milliseconds (default: 5000ms)
    /// @param headers key-value pairs as fl::span (default: empty, supports initializer lists)
    /// @returns fl::future<Response> for non-blocking result checking
    /// 
    /// @code
    /// // Simplest usage - just URL (uses defaults: 5s timeout, no headers)
    /// auto future = client.get("http://localhost:8899/health");
    /// 
    /// // Custom timeout only
    /// auto future = client.get("http://slow-api.com/data", 10000);
    /// 
    /// // Inline headers (fl::span automatically converts from initializer list)
    /// auto future = client.get("http://api.example.com/data", 5000, {
    ///     {"Authorization", "Bearer token123"},
    ///     {"Content-Type", "application/json"},
    ///     {"Accept", "application/json"}
    /// });
    /// 
    /// // Vector headers (fl::span automatically converts from vector)
    /// fl::vector<fl::pair<fl::string, fl::string>> headers;
    /// headers.push_back({"Authorization", "Bearer token123"});
    /// auto future = client.get("http://api.example.com/data", 15000, headers);
    /// 
    /// // Later in loop():
    /// if (future.is_ready()) {
    ///     auto result = future.try_result();
    ///     if (result && result->ok()) {
    ///         FL_WARN("Got: " << result->text());
    ///     }
    /// }
    /// @endcode
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// HTTP POST request with customizable timeout and headers (returns future)
    /// @param url the URL to post to
    /// @param data binary data to send (fl::vector automatically converts to span)
    /// @param timeout_ms request timeout in milliseconds (default: 5000ms)
    /// @param headers key-value pairs as fl::span (default: empty, supports initializer lists)
    /// @returns fl::future<Response> for non-blocking result checking
    /// 
    /// @code
    /// // Simplest usage - just URL and data (uses defaults: 5s timeout, no headers)
    /// fl::vector<fl::u8> sensor_data = get_sensor_readings();
    /// auto future = client.post("http://localhost:8899/upload", sensor_data);
    /// 
    /// // Custom timeout only
    /// auto future = client.post("http://slow-api.com/upload", sensor_data, 15000);
    /// 
    /// // Inline headers (fl::span automatically converts from initializer list)
    /// auto future = client.post("http://api.example.com/sensors", sensor_data, 5000, {
    ///     {"Content-Type", "application/octet-stream"},
    ///     {"Authorization", "Bearer " + api_token},
    ///     {"X-Device-ID", "sensor-001"}
    /// });
    /// 
    /// // Vector headers (fl::span automatically converts from vector)
    /// fl::vector<fl::pair<fl::string, fl::string>> headers;
    /// headers.push_back({"Content-Type", "application/octet-stream"});
    /// headers.push_back({"Authorization", "Bearer " + api_token});
    /// auto future = client.post("http://api.example.com/sensors", sensor_data, 10000, headers);
    /// 
    /// // Later in loop():
    /// if (future.is_ready()) {
    ///     auto result = future.try_result();
    ///     if (result) {
    ///         FL_WARN("Upload result: " << result->status_code());
    ///     }
    /// }
    /// @endcode
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ========== CALLBACK-BASED INTERFACE ==========
    
    /// Async GET request with callback and customizable timeout/headers
    /// @param url the URL to request
    /// @param callback function called when response arrives
    /// @param timeout_ms request timeout in milliseconds (default: 5000ms)
    /// @param headers key-value pairs as fl::span (default: empty, supports initializer lists)
    /// 
    /// @code
    /// // Simplest usage - just URL and callback (uses defaults)
    /// client.get_async("http://localhost:8899/health", [](const Response& response) {
    ///     if (response.ok()) {
    ///         FL_WARN("Health check: OK");
    ///     }
    /// });
    /// 
    /// // Custom timeout
    /// client.get_async("http://slow-api.com/data", [](const Response& response) {
    ///     FL_WARN("Slow API result: " << response.status_code());
    /// }, 15000);
    /// 
    /// // Inline headers (fl::span automatically converts from initializer list)
    /// client.get_async("http://api.example.com/data", [](const Response& response) {
    ///     if (response.ok()) {
    ///         FL_WARN("Got data: " << response.text());
    ///     }
    /// }, 5000, {
    ///     {"Authorization", "Bearer " + token},
    ///     {"Accept", "application/json"}
    /// });
    /// @endcode
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async POST request with callback and customizable timeout/headers
    /// @param url the URL to post to
    /// @param data binary data to send
    /// @param callback function called when response arrives
    /// @param timeout_ms request timeout in milliseconds (default: 5000ms)
    /// @param headers key-value pairs as fl::span (default: empty, supports initializer lists)
    /// 
    /// @code
    /// fl::vector<fl::u8> sensor_data = get_readings();
    /// 
    /// // Simplest usage - just URL, data, and callback (uses defaults)
    /// client.post_async("http://localhost:8899/upload", sensor_data, [](const Response& response) {
    ///     FL_WARN("Upload: " << (response.ok() ? "SUCCESS" : "FAILED"));
    /// });
    /// 
    /// // Custom timeout
    /// client.post_async("http://slow-api.com/upload", sensor_data, [](const Response& response) {
    ///     FL_WARN("Slow upload: " << response.status_code());
    /// }, 20000);
    /// 
    /// // Inline headers (fl::span automatically converts from initializer list)
    /// client.post_async("http://api.example.com/sensors", sensor_data, [](const Response& response) {
    ///     FL_WARN("Upload: " << (response.ok() ? "SUCCESS" : "FAILED"));
    /// }, 5000, {
    ///     {"Content-Type", "application/octet-stream"},
    ///     {"Authorization", "Bearer " + token}
    /// });
    /// @endcode
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ========== STREAMING INTERFACE (ASYNC ONLY) ==========
    
    /// Streaming APIs are inherently callback-based and do NOT return fl::future!
    /// 
    /// **Why No Futures for Streaming?**
    /// - Streaming delivers multiple chunks over time (not a single result)
    /// - Futures are designed for single "eventual" results
    /// - Chunk processing needs immediate callbacks for memory efficiency
    /// - Progress monitoring requires real-time chunk notifications
    /// - Streaming APIs naturally use callbacks for data flow control
    ///
    /// **Naming Convention:**
    /// - Methods returning futures: get(), post()
    /// - Methods taking callbacks: get_async(), post_async()  
    /// - Streaming methods: get_stream_async(), post_stream_async() (always async!)
    
    /// Streaming GET request - receives data in chunks as it arrives
    /// Note: Streaming APIs are inherently callback-based (no fl::future - use callbacks!)
    /// @param url the URL to request
    /// @param headers key-value pairs as fl::span  
    /// @param chunk_callback called for each data chunk received
    /// @param complete_callback called when stream completes (success or error)
    /// 
    /// @code
    /// client.get_stream_async("http://api.example.com/large-data", 
    ///                         fl::span<const fl::pair<fl::string, fl::string>>(),
    ///                         [](fl::span<const fl::u8> chunk, bool is_final) {
    ///                             // Process each chunk as it arrives
    ///                             process_data_chunk(chunk);
    ///                             if (is_final) {
    ///                                 FL_WARN("Stream complete, total processed: " << total_bytes);
    ///                             }
    ///                             return true; // Continue streaming
    ///                         },
    ///                         [](int status_code, const fl::string& error_msg) {
    ///                             if (status_code == 200) {
    ///                                 FL_WARN("Stream completed successfully");
    ///                             } else {
    ///                                 FL_WARN("Stream error: " << status_code << " - " << error_msg);
    ///                             }
    ///                         });
    /// @endcode
    void get_stream_async(const char* url,
                          fl::span<const fl::pair<fl::string, fl::string>> headers,
                          fl::function<bool(fl::span<const fl::u8> chunk, bool is_final)> chunk_callback,
                          fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    /// Streaming GET request with initializer list headers
    /// @param url the URL to request
    /// @param headers initializer list of key-value pairs
    /// @param chunk_callback called for each data chunk received  
    /// @param complete_callback called when stream completes
    /// 
    /// @code
    /// client.get_stream_async("http://api.example.com/sensor-log", {
    ///     {"Authorization", "Bearer " + token},
    ///     {"Accept", "application/octet-stream"}
    /// }, [](fl::span<const fl::u8> chunk, bool is_final) {
    ///     // Write chunk to SD card or process incrementally
    ///     sd_card.write(chunk);
    ///     return true; // Continue
    /// }, [](int status, const fl::string& msg) {
    ///     FL_WARN("Download complete: " << status);
    /// });
    /// @endcode
    void get_stream_async(const char* url,
                          fl::initializer_list<fl::pair<fl::string, fl::string>> headers,
                          fl::function<bool(fl::span<const fl::u8> chunk, bool is_final)> chunk_callback,
                          fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    /// Streaming GET request without headers
    /// @param url the URL to request
    /// @param chunk_callback called for each data chunk received
    /// @param complete_callback called when stream completes
    void get_stream_async(const char* url,
                          fl::function<bool(fl::span<const fl::u8> chunk, bool is_final)> chunk_callback,
                          fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    /// Streaming POST request - sends data in chunks
    /// @param url the URL to post to
    /// @param headers key-value pairs as fl::span
    /// @param data_provider callback that provides next chunk of data to send
    /// @param complete_callback called when upload completes
    /// 
    /// @code
    /// fl::size total_sent = 0;
    /// client.post_stream_async("http://api.example.com/upload", 
    ///                          fl::span<const fl::pair<fl::string, fl::string>>(),
    ///                          [&](fl::span<fl::u8> buffer) -> fl::size {
    ///                              // Fill buffer with next chunk of data
    ///                              fl::size chunk_size = read_sensor_data(buffer);
    ///                              total_sent += chunk_size;
    ///                              return chunk_size; // Return 0 when done
    ///                          },
    ///                          [](int status_code, const fl::string& error_msg) {
    ///                              FL_WARN("Upload result: " << status_code);
    ///                          });
    /// @endcode
    void post_stream_async(const char* url,
                           fl::span<const fl::pair<fl::string, fl::string>> headers,
                           fl::function<fl::size(fl::span<fl::u8> buffer)> data_provider,
                           fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    /// Streaming POST request with initializer list headers
    /// @param url the URL to post to
    /// @param headers initializer list of key-value pairs
    /// @param data_provider callback that provides next chunk of data to send
    /// @param complete_callback called when upload completes
    /// 
    /// @code
    /// client.post_stream_async("http://api.example.com/logs", {
    ///     {"Content-Type", "application/octet-stream"},
    ///     {"Authorization", "Bearer " + upload_token}
    /// }, [](fl::span<fl::u8> buffer) -> fl::size {
    ///     return read_log_chunk(buffer); // Returns bytes read, 0 when done
    /// }, [](int status, const fl::string& msg) {
    ///     FL_WARN("Log upload: " << (status == 200 ? "SUCCESS" : "FAILED"));
    /// });
    /// @endcode
    void post_stream_async(const char* url,
                           fl::initializer_list<fl::pair<fl::string, fl::string>> headers,
                           fl::function<fl::size(fl::span<fl::u8> buffer)> data_provider,
                           fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    /// Streaming POST request without headers
    /// @param url the URL to post to
    /// @param data_provider callback that provides next chunk of data to send  
    /// @param complete_callback called when upload completes
    void post_stream_async(const char* url,
                           fl::function<fl::size(fl::span<fl::u8> buffer)> data_provider,
                           fl::function<void(int status_code, const fl::string& error_msg)> complete_callback);
    
    // ========== CONFIGURATION ==========
    
    /// Manual network pumping (alternative to EngineEvents integration)
    void pump_network_events();
    
    /// Set request timeout in milliseconds
    void set_timeout(uint32_t timeout_ms) { mTimeoutMs = timeout_ms; }
    
    /// Set maximum concurrent requests
    void set_max_concurrent_requests(int max) { mMaxConcurrent = max; }
    
    /// Set user agent string for all requests
    void set_user_agent(const char* user_agent) { mUserAgent = user_agent; }

    // ========== TLS/SSL SECURITY ==========
    
    /// TLS configuration options
    struct TlsConfig {
        bool verify_certificates = true;        ///< Verify server certificates against root CAs
        bool verify_hostname = true;            ///< Verify hostname matches certificate
        bool allow_self_signed = false;        ///< Allow self-signed certificates (dev only)
        bool require_tls_v12_or_higher = true; ///< Require TLS 1.2+ for security
        fl::size max_certificate_chain = 3;    ///< Maximum certificate chain length
        fl::size connection_timeout_ms = 10000; ///< TLS handshake timeout
    };
    
    /// Set TLS configuration for all HTTPS requests
    /// @param config TLS security settings
    /// 
    /// @code
    /// HttpClient::TlsConfig tls_config;
    /// tls_config.verify_certificates = true;
    /// tls_config.verify_hostname = true;
    /// tls_config.require_tls_v12_or_higher = true;
    /// client.set_tls_config(tls_config);
    /// 
    /// // Now all HTTPS calls use these settings
    /// auto future = client.get("https://secure-api.example.com/data");
    /// @endcode
    void set_tls_config(const TlsConfig& config);
    
    /// Get current TLS configuration
    const TlsConfig& get_tls_config() const;
    
    /// Add root certificate for validating HTTPS connections
    /// @param certificate_pem PEM-encoded root certificate
    /// @param name friendly name for the certificate (for debugging)
    /// @returns true if certificate was added successfully
    /// 
    /// @code
    /// // Add common root CAs
    /// const char* lets_encrypt_ca = R"(
    /// -----BEGIN CERTIFICATE-----
    /// MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
    /// TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
    /// ...certificate content...
    /// -----END CERTIFICATE-----
    /// )";
    /// 
    /// client.add_root_certificate(lets_encrypt_ca, "Let's Encrypt Authority X3");
    /// @endcode
    bool add_root_certificate(const char* certificate_pem, const char* name = nullptr);
    
    /// Load root certificates from common certificate bundle
    /// @param bundle_type which certificate bundle to load
    /// @returns number of certificates loaded
    /// 
    /// @code
    /// // Load common CA certificates for most HTTPS sites
    /// int count = client.load_ca_bundle(HttpClient::CA_BUNDLE_MOZILLA);
    /// FL_WARN("Loaded " << count << " root certificates");
    /// @endcode
    enum CaBundleType {
        CA_BUNDLE_MOZILLA,     ///< Mozilla's trusted root certificate store
        CA_BUNDLE_SYSTEM,      ///< System-provided certificates (platform-specific)
        CA_BUNDLE_MINIMAL,     ///< Minimal set (Let's Encrypt, DigiCert, etc.)
        CA_BUNDLE_IOT_COMMON   ///< Common IoT service providers
    };
    int load_ca_bundle(CaBundleType bundle_type);
    
    /// Clear all root certificates
    /// @code
    /// client.clear_certificates();
    /// // Now only manually added certificates or pinned certificates work
    /// @endcode
    void clear_certificates();
    
    /// Pin a specific certificate for a domain (highest security)
    /// @param hostname domain to pin certificate for
    /// @param certificate_sha256 SHA256 fingerprint of the certificate
    /// 
    /// @code
    /// // Pin GitHub's certificate (example fingerprint)
    /// client.pin_certificate("api.github.com", 
    ///                        "a031c46782e6e6c662c2c87c76da9aa62ccabd8e2f4f9a301d1d9b9c3e6e6e6e");
    /// 
    /// // Now connections to api.github.com MUST use this exact certificate
    /// auto future = client.get("https://api.github.com/user");
    /// @endcode
    void pin_certificate(const char* hostname, const char* certificate_sha256);
    
    /// Remove certificate pin for a domain
    /// @param hostname domain to remove pin for
    void unpin_certificate(const char* hostname);
    
    /// Get TLS connection information for the last request
    /// @returns TLS details, or empty if not HTTPS or connection failed
    struct TlsInfo {
        bool is_secure = false;              ///< True if TLS was used
        fl::string tls_version;              ///< "TLSv1.2", "TLSv1.3", etc.
        fl::string cipher_suite;             ///< Cipher suite used
        fl::string server_certificate_sha256; ///< Server cert fingerprint
        bool certificate_verified = false;   ///< True if certificate was validated
        bool hostname_verified = false;      ///< True if hostname matched certificate
    };
    const TlsInfo& get_last_tls_info() const;
    
    /// Check if a URL requires TLS (starts with https://)
    /// @param url URL to check
    /// @returns true if URL uses HTTPS
    static bool requires_tls(const char* url);
    
    // ========== CERTIFICATE STORAGE ==========
    
    /// Save current certificate store to persistent storage
    /// @param storage_path platform-specific storage location
    /// @returns true if certificates were saved successfully
    /// 
    /// @code
    /// // Save certificates to SPIFFS/LittleFS on ESP32
    /// client.save_certificates("/spiffs/ca_certs.bin");
    /// 
    /// // Save to SD card
    /// client.save_certificates("/sd/certificates.dat");
    /// @endcode
    bool save_certificates(const char* storage_path);
    
    /// Load certificates from persistent storage
    /// @param storage_path platform-specific storage location
    /// @returns number of certificates loaded
    /// 
    /// @code
    /// // Load saved certificates on startup
    /// int count = client.load_certificates("/spiffs/ca_certs.bin");
    /// if (count == 0) {
    ///     // No saved certificates, load defaults
    ///     client.load_ca_bundle(HttpClient::CA_BUNDLE_MINIMAL);
    ///     client.save_certificates("/spiffs/ca_certs.bin");
    /// }
    /// @endcode
    int load_certificates(const char* storage_path);
    
    /// Get certificate storage statistics
    /// @returns information about certificate memory usage
    struct CertificateStats {
        fl::size certificate_count;          ///< Number of loaded certificates
        fl::size total_certificate_size;     ///< Total memory used by certificates
        fl::size pinned_certificate_count;   ///< Number of pinned certificates
        fl::size max_certificate_capacity;   ///< Maximum certificates that can be stored
    };
    CertificateStats get_certificate_stats() const;

private:
    // EngineEvents::Listener interface
    void onEndFrame() override;
    
    // Internal request management
    struct PendingRequest {
        fl::unique_ptr<Socket> socket;
        fl::string url;
        fl::vector<fl::u8> request_data;
        fl::vector<fl::u8> response_buffer;
        fl::vector<fl::pair<fl::string, fl::string>> headers;
        fl::function<void(const Response&)> callback;
        fl::shared_ptr<FutureState<Response>> future_state;
        fl::u32 start_time;
        enum State { CONNECTING, SENDING, RECEIVING, COMPLETE, ERROR } state;
    };
    
    fl::vector<fl::unique_ptr<PendingRequest>> mPendingRequests;
    uint32_t mTimeoutMs = 5000;
    int mMaxConcurrent = 4;
    fl::string mUserAgent = "FastLED-HttpClient/1.0";
    
    // Header processing
    fl::string build_http_request(const char* method, const char* url, 
                                  fl::span<const fl::pair<fl::string, fl::string>> headers,
                                  fl::span<const fl::u8> body = fl::span<const fl::u8>());
    
    void process_pending_requests();
    void complete_request(PendingRequest* req, const Response& response);
    void fail_request(PendingRequest* req, const fl::string& error_message);
};

} // namespace fl
```

### 2. Response Object (Updated)

```cpp
namespace fl {

class Response {
public:
    Response() = default;
    Response(int status_code, fl::vector<fl::u8> body, fl::vector<fl::pair<fl::string, fl::string>> headers);
    
    // Status information
    int status_code() const { return mStatusCode; }
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    bool is_error() const { return mStatusCode >= 400; }
    
    // Binary content access (no UTF-8 support)
    fl::span<const fl::u8> data() const { return fl::span<const fl::u8>(mBody); }
    fl::span<const fl::u8> binary() const { return fl::span<const fl::u8>(mBody); }
    fl::size size() const { return mBody.size(); }
    
    // Text content (using fl::string with copy-on-write semantics)
    /// Returns response body as fl::string (copy-on-write makes this very efficient!)
    /// Can be stored directly - shared buffer semantics prevent unnecessary copies
    fl::string text() const { return mBodyText; }
    
    /// Legacy C-style access (when you need raw char* immediately)
    const char* c_str() const { return mBodyText.c_str(); }
    fl::size text_length() const { return mBodyText.size(); }
    
    // Headers access  
    fl::optional<fl::string> get_header(const fl::string& name) const;
    fl::optional<fl::string> get_header(const char* name) const; // Convenience overload
    
    // Get all headers as span (follows FastLED preference)
    fl::span<const fl::pair<fl::string, fl::string>> headers() const { 
        return fl::span<const fl::pair<fl::string, fl::string>>(mHeaders); 
    }
    
    // Get headers as vector (for compatibility)
    const fl::vector<fl::pair<fl::string, fl::string>>& headers_vector() const { 
        return mHeaders; 
    }
    
    // Template access for any container
    template<typename HeaderContainer>
    void copy_headers_to(HeaderContainer& container) const {
        for (const auto& header : mHeaders) {
            container.insert(container.end(), header);
        }
    }
    
    // Error information (using fl::string for shared ownership)  
    bool has_error() const { return !mErrorMessage.empty(); }
    const fl::string& error_message() const { return mErrorMessage; }

private:
    int mStatusCode = 0;
    fl::vector<fl::u8> mBody;
    fl::string mBodyText;  // Shared string for text access
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::string mErrorMessage;  // Shared error message
    
    friend class HttpClient;
    void set_error(const fl::string& message) { mErrorMessage = message; }
    void set_error(const char* message) { mErrorMessage = message; } // Convenience overload
    
    // Update both binary and text representations
    void set_body(fl::vector<fl::u8> body) {
        mBody = fl::move(body);
        // Convert to string (efficient - no extra copy if small)
        mBodyText = fl::string(reinterpret_cast<const char*>(mBody.data()), mBody.size());
    }
};

} // namespace fl
```

## fl::future Integration

The FastLED networking API uses a custom **completable future** implementation that provides non-blocking, event-driven async programming patterns.

> **📄 For complete implementation details, see:** [`CONCURRENCY_FUTURE_API_DESIGN.md`](./CONCURRENCY_FUTURE_API_DESIGN.md)

### Quick Overview

The `fl::future<T>` implementation provides:
- **Non-blocking result checking** via `try_result()` - never blocks
- **Event-driven pattern** perfect for `setup()` + `loop()` 
- **Shared string semantics** for efficient memory usage
- **Thread-safe operations** with zero overhead on single-threaded platforms
- **Producer/consumer interface** in a single object (no separate promises)

### Basic Integration Pattern

```cpp
class HttpClient {
private:
    fl::vector<fl::future<Response>> mActiveFutures;
    
public:
    fl::future<Response> get(const char* url) {
        auto future = fl::future<Response>::create();
        mActiveFutures.push_back(future);
        start_network_request(url, mActiveFutures.size() - 1);
        return future; // Return immediately (non-blocking)
    }
    
private:
    void on_network_complete(fl::size index, const Response& response) {
        mActiveFutures[index].complete_with_value(response);
    }
};
```

### User-Facing API

```cpp
void setup() {
    weather_future = client.get("http://weather.api.com/current");
}

void loop() {
    if (weather_future.is_ready()) {
        auto result = weather_future.try_result();
        if (result) {
            FL_WARN("Weather: " << result->text());
        }
        weather_future.reset();
    }
    
    FastLED.show(); // Always responsive
}
```

## Internal Request Architecture

### Request Object Design

The HTTP client uses an internal `Request` object that encapsulates all request parameters. The public API methods (`get()`, `post()`, etc.) create `Request` objects and delegate to a common `send()` method.

```cpp
namespace fl {

/// HTTP method enumeration
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

/// Internal request representation
/// All public API methods create Request objects and delegate to HttpClient::send()
class Request {
public:
    /// Default constructor - creates invalid request
    Request() = default;
    
    /// Create GET request
    static Request get(const char* url, 
                       uint32_t timeout_ms = 5000,
                       fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        Request req;
        req.mMethod = HttpMethod::GET;
        req.mUrl = url;
        req.mTimeoutMs = timeout_ms;
        req.copy_headers(headers);
        return req;
    }
    
    /// Create POST request  
    static Request post(const char* url,
                        fl::span<const fl::u8> data,
                        uint32_t timeout_ms = 5000,
                        fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        Request req;
        req.mMethod = HttpMethod::POST;
        req.mUrl = url;
        req.mData = fl::vector<fl::u8>(data.begin(), data.end());
        req.mTimeoutMs = timeout_ms;
        req.copy_headers(headers);
        return req;
    }
    
    /// Create PUT request
    static Request put(const char* url,
                       fl::span<const fl::u8> data,
                       uint32_t timeout_ms = 5000,
                       fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        Request req;
        req.mMethod = HttpMethod::PUT;
        req.mUrl = url;
        req.mData = fl::vector<fl::u8>(data.begin(), data.end());
        req.mTimeoutMs = timeout_ms;
        req.copy_headers(headers);
        return req;
    }
    
    /// Create DELETE request
    static Request del(const char* url,
                       uint32_t timeout_ms = 5000,
                       fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        Request req;
        req.mMethod = HttpMethod::DELETE;
        req.mUrl = url;
        req.mTimeoutMs = timeout_ms;
        req.copy_headers(headers);
        return req;
    }
    
    // ========== ACCESSORS ==========
    
    HttpMethod method() const { return mMethod; }
    const fl::string& url() const { return mUrl; }
    uint32_t timeout_ms() const { return mTimeoutMs; }
    fl::span<const fl::u8> data() const { return fl::span<const fl::u8>(mData); }
    fl::span<const fl::pair<fl::string, fl::string>> headers() const { 
        return fl::span<const fl::pair<fl::string, fl::string>>(mHeaders); 
    }
    
    bool has_data() const { return !mData.empty(); }
    bool is_valid() const { return mMethod != HttpMethod::GET || !mUrl.empty(); }
    
    // ========== MODIFIERS ==========
    
    /// Set request timeout
    Request& timeout(uint32_t timeout_ms) { 
        mTimeoutMs = timeout_ms; 
        return *this; 
    }
    
    /// Add single header
    Request& header(const fl::string& name, const fl::string& value) {
        mHeaders.push_back({name, value});
        return *this;
    }
    
    /// Add single header (const char* convenience)
    Request& header(const char* name, const char* value) {
        mHeaders.push_back({fl::string(name), fl::string(value)});
        return *this;
    }
    
    /// Add multiple headers
    Request& headers(fl::span<const fl::pair<fl::string, fl::string>> headers) {
        copy_headers(headers);
        return *this;
    }
    
    /// Set request data/body
    Request& data(fl::span<const fl::u8> data) {
        mData = fl::vector<fl::u8>(data.begin(), data.end());
        return *this;
    }
    
    /// Clear all headers
    Request& clear_headers() {
        mHeaders.clear();
        return *this;
    }
    
    /// Clear request data
    Request& clear_data() {
        mData.clear();
        return *this;
    }
    
    // ========== UTILITY ==========
    
    /// Get HTTP method as string
    const char* method_string() const {
        switch (mMethod) {
            case HttpMethod::GET: return "GET";
            case HttpMethod::POST: return "POST";
            case HttpMethod::PUT: return "PUT";
            case HttpMethod::DELETE: return "DELETE";
            case HttpMethod::PATCH: return "PATCH";
            case HttpMethod::HEAD: return "HEAD";
            case HttpMethod::OPTIONS: return "OPTIONS";
            default: return "UNKNOWN";
        }
    }
    
    /// Check if method supports request body
    bool method_supports_body() const {
        return mMethod == HttpMethod::POST || 
               mMethod == HttpMethod::PUT || 
               mMethod == HttpMethod::PATCH;
    }
    
private:
    HttpMethod mMethod = HttpMethod::GET;
    fl::string mUrl;
    uint32_t mTimeoutMs = 5000;
    fl::vector<fl::u8> mData;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    
    void copy_headers(fl::span<const fl::pair<fl::string, fl::string>> headers) {
        for (const auto& header : headers) {
            mHeaders.push_back(header);
        }
    }
};

} // namespace fl
```

### Updated HttpClient Architecture

The `HttpClient` now uses the `Request` object internally, with all public methods delegating to a common `send()` implementation:

```cpp
namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    HttpClient();
    ~HttpClient();
    
    // ========== PUBLIC API - DELEGATES TO send() ==========
    
    /// HTTP GET request (delegates to send)
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        return send(Request::get(url, timeout_ms, headers));
    }
    
    /// HTTP POST request (delegates to send)
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        return send(Request::post(url, data, timeout_ms, headers));
    }
    
    /// HTTP PUT request (delegates to send)
    fl::future<Response> put(const char* url,
                             fl::span<const fl::u8> data,
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        return send(Request::put(url, data, timeout_ms, headers));
    }
    
    /// HTTP DELETE request (delegates to send)
    fl::future<Response> del(const char* url,
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        return send(Request::del(url, timeout_ms, headers));
    }
    
    // ========== ASYNC API - DELEGATES TO send_async() ==========
    
    /// Async GET request with callback (delegates to send_async)
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        send_async(Request::get(url, timeout_ms, headers), callback);
    }
    
    /// Async POST request with callback (delegates to send_async)
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
        send_async(Request::post(url, data, timeout_ms, headers), callback);
    }
    
    // ========== CORE IMPLEMENTATION METHODS ==========
    
    /// Send request and return future (CORE METHOD - all others delegate here)
    /// @param request the request to send
    /// @returns fl::future<Response> for non-blocking result checking
    /// 
    /// @code
    /// // All public methods delegate to this:
    /// auto request = Request::get("http://api.com/data", 5000, {{"Auth", "Bearer token"}});
    /// auto future = client.send(request);
    /// @endcode
    fl::future<Response> send(const Request& request);
    
    /// Send request asynchronously with callback (CORE METHOD - all others delegate here)
    /// @param request the request to send
    /// @param callback function called when response arrives
    void send_async(const Request& request, fl::function<void(const Response&)> callback);
    
    // ========== FLUENT API (OPTIONAL ADVANCED USAGE) ==========
    
    /// Create a reusable request builder for complex scenarios
    /// @code
    /// // Build complex request step by step
    /// auto request = client.request()
    ///     .get("http://api.example.com/data")
    ///     .timeout(10000)
    ///     .header("Authorization", "Bearer " + token)
    ///     .header("Accept", "application/json");
    /// 
    /// auto future = client.send(request);
    /// 
    /// // Reuse and modify request
    /// auto modified = request.header("Cache-Control", "no-cache");
    /// auto future2 = client.send(modified);
    /// @endcode
    Request request() { return Request(); }
    
    // ... rest of HttpClient methods (streaming, TLS, etc.)
    
private:
    // EngineEvents::Listener interface
    void onEndFrame() override;
    
    // Internal request management (now uses Request objects)
    struct PendingRequest {
        fl::unique_ptr<Socket> socket;
        Request request;                           // Store Request instead of individual fields
        fl::vector<fl::u8> response_buffer;
        fl::function<void(const Response&)> callback;
        fl::shared_ptr<FutureState<Response>> future_state;
        fl::u32 start_time;
        enum State { CONNECTING, SENDING, RECEIVING, COMPLETE, ERROR } state;
    };
    
    fl::vector<fl::unique_ptr<PendingRequest>> mPendingRequests;
    fl::string mUserAgent = "FastLED-HttpClient/1.0";
    
    // Core implementation methods (now take Request objects)
    fl::string build_http_request(const Request& request);
    void process_pending_requests();
    void complete_request(PendingRequest* req, const Response& response);
    void fail_request(PendingRequest* req, const fl::string& error_message);
};

} // namespace fl
```

### Benefits of Request Object Architecture

The `Request` object pattern provides significant architectural advantages:

**🎯 Clean Internal Architecture:**
```cpp
// ✅ Before: Multiple parameters everywhere  
void build_http_request(const char* method, const char* url, 
                        fl::span<headers> headers, fl::span<data> body, uint32_t timeout);

// ✅ After: Single Request object  
fl::string build_http_request(const Request& request);
```

**🔄 Easy to Add New HTTP Methods:**
```cpp
// Adding PATCH method is trivial:
fl::future<Response> patch(const char* url,
                           fl::span<const fl::u8> data,
                           uint32_t timeout_ms = 5000,
                           fl::span<const fl::pair<fl::string, fl::string>> headers = {}) {
    return send(Request::patch(url, data, timeout_ms, headers));  // Just add static factory
}
```

**🧪 Easier Testing:**
```cpp
// Test the core send() logic with mock Request objects
TEST(HttpClientTest, SendGetRequest) {
    auto request = Request::get("http://test.com", 1000, {{"Auth", "Bearer test"}});
    auto future = client.send(request);
    // Verify request was processed correctly
}
```

**🔧 Request Reuse and Modification:**
```cpp
// Create base request template
auto base_request = Request::get("http://api.example.com/data", 5000)
    .header("Accept", "application/json")
    .header("User-Agent", "FastLED-Device/1.0");

// Reuse with different auth tokens
auto user1_request = base_request.header("Authorization", "Bearer " + user1_token);
auto user2_request = base_request.header("Authorization", "Bearer " + user2_token);

auto future1 = client.send(user1_request);
auto future2 = client.send(user2_request);
```

**💡 Fluent Request Building (Advanced Usage):**
```cpp
void complex_api_interaction() {
    HttpClient client;
    
    // Build complex request step by step
    auto complex_request = client.request()
        .post("http://api.example.com/complex")
        .timeout(30000)
        .header("Content-Type", "application/json")
        .header("Authorization", "Bearer " + get_api_token())
        .header("X-Request-ID", generate_uuid())
        .header("X-Retry-Count", "0")
        .data(build_json_payload());
    
    // Send and handle response
    auto future = client.send(complex_request);
    
    // Later, retry with modified headers
    if (need_retry) {
        auto retry_request = complex_request
            .clear_headers()
            .header("Authorization", "Bearer " + refresh_token())
            .header("X-Retry-Count", "1")
            .timeout(60000);  // Longer timeout for retry
            
        auto retry_future = client.send(retry_request);
    }
}
```

### Implementation Flow

**User API Call Flow:**
```
1. client.get("http://api.com/data", 5000, {{"Auth", "Bearer token"}})
     ↓
2. Request::get(url, timeout, headers) creates Request object
     ↓
3. client.send(request) - SINGLE IMPLEMENTATION POINT
     ↓
4. build_http_request(request) - generates HTTP string
     ↓
5. Network transport layer
```

**Internal Request Processing:**
```cpp
// HttpClient::send() implementation (simplified)
fl::future<Response> HttpClient::send(const Request& request) {
    // Validate request
    if (!request.is_valid()) {
        return fl::make_error_future<Response>("Invalid request");
    }
    
    // Create future for this request
    auto future = fl::future<Response>::create();
    
    // Create pending request object
    auto pending = fl::make_unique<PendingRequest>();
    pending->request = request;  // Store complete Request
    pending->future_state = future.get_shared_state();
    pending->start_time = millis();
    
    // Build HTTP request string from Request object
    fl::string http_request = build_http_request(request);
    
    // Start network connection
    pending->socket = create_socket(request.url());
    if (pending->socket->connect()) {
        pending->state = PendingRequest::CONNECTING;
        mPendingRequests.push_back(fl::move(pending));
    } else {
        future.complete_with_error("Connection failed");
    }
    
    return future;
}
```

### Real-World Request Examples

**Simple Health Check (No Request Object Needed):**
```cpp
// User sees simple API
auto future = client.get("http://localhost:8899/health");

// Under the hood:
// 1. Request::get("http://localhost:8899/health", 5000, {}) creates Request
// 2. client.send(request) processes it
// 3. Single code path handles all requests
```

**API with Authentication (Using Request Internally):**
```cpp
// User sees simple API
auto future = client.get("http://api.com/data", 10000, {
    {"Authorization", "Bearer token123"},
    {"Accept", "application/json"}
});

// Under the hood:
// 1. Request::get(url, 10000, headers) creates Request object
// 2. Request stores: method=GET, url=url, timeout=10000, headers=headers
// 3. client.send(request) uses single implementation path
// 4. build_http_request(request) generates proper HTTP
```

**Advanced Request Building (When Needed):**
```cpp
// Advanced users can access Request directly for complex scenarios
void handle_api_with_retries() {
    HttpClient client;
    
    // Build base request
    auto base_request = Request::get("http://api.example.com/data", 5000)
        .header("Accept", "application/json")
        .header("User-Agent", "FastLED-Device/1.0");
    
    // Try with primary auth
    auto primary_request = base_request.header("Authorization", "Bearer " + primary_token);
    auto future = client.send(primary_request);
    
    // Later, if that fails, try with backup auth
    auto backup_request = base_request
        .clear_headers()
        .header("Authorization", "Bearer " + backup_token)
        .timeout(15000);  // Longer timeout for backup
        
    auto backup_future = client.send(backup_request);
}
```

**Multiple HTTP Methods (Easy to Add):**
```cpp
// All methods delegate to same send() implementation
auto get_future = client.get("http://api.com/users/1");        // -> Request::get() -> send()
auto post_future = client.post("http://api.com/users", data);  // -> Request::post() -> send()  
auto put_future = client.put("http://api.com/users/1", data);  // -> Request::put() -> send()
auto del_future = client.del("http://api.com/users/1");        // -> Request::del() -> send()
```

### 4. Network Event Pumping Integration

```cpp
namespace fl {

class NetworkEventPump : public EngineEvents::Listener {
public:
    static NetworkEventPump& instance() {
        static NetworkEventPump pump;
        return pump;
    }
    
    void register_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it == mClients.end()) {
            mClients.push_back(client);
        }
    }
    
    void unregister_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it != mClients.end()) {
            mClients.erase(it);
        }
    }
    
    // Manual pumping for users who don't want EngineEvents integration
    static void pump_all_clients() {
        instance().pump_clients();
    }

private:
    NetworkEventPump() {
        EngineEvents::addListener(this, 10); // Low priority, run after UI
    }
    
    ~NetworkEventPump() {
        EngineEvents::removeListener(this);
    }
    
    // EngineEvents::Listener interface - pump after frame is drawn
    void onEndFrame() override {
        pump_clients();
    }
    
    void pump_clients() {
        for (auto* client : mClients) {
            client->pump_network_events();
        }
    }
    
    fl::vector<HttpClient*> mClients;
};

} // namespace fl
```

## Usage Examples

### 1. **Simplified API with Defaults (EASIEST)**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

HttpClient client;

void setup() {
    // SIMPLEST usage - just the URL! (uses 5s timeout, no headers)
    auto health_future = client.get("http://localhost:8899/health");
    
    // Simple POST with just URL and data
    fl::vector<fl::u8> sensor_data = get_sensor_readings();
    auto upload_future = client.post("http://localhost:8899/upload", sensor_data);
    
    // Add custom timeout when needed
    auto slow_api_future = client.get("http://slow-api.com/data", 15000);
    
    // Add headers when needed (using default 5s timeout)
    auto auth_future = client.get("http://api.weather.com/current", 5000, {
        {"Authorization", "Bearer weather_api_key"},
        {"Accept", "application/json"},
        {"User-Agent", "WeatherStation/1.0"}
    });
    
    // Store futures for checking in loop()
    pending_futures.push_back(fl::move(health_future));
    pending_futures.push_back(fl::move(upload_future));
    pending_futures.push_back(fl::move(slow_api_future));
    pending_futures.push_back(fl::move(auth_future));
}

void loop() {
    // Check all pending requests
    for (auto it = pending_futures.begin(); it != pending_futures.end();) {
        if (it->is_ready()) {
            auto result = it->try_result();
            if (result && result->ok()) {
                FL_WARN("Success: " << result->text());
            }
            it = pending_futures.erase(it);
        } else {
            ++it;
        }
    }
    
    FastLED.show();
}
```

### 2. **Callback API with Defaults**

```cpp
void setup() {
    // Ultra-simple callback API - just URL and callback!
    client.get_async("http://localhost:8899/status", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("Status: " << response.text());
        }
    });
    
    // Simple POST with just URL, data, and callback
    fl::vector<fl::u8> telemetry = collect_telemetry();
    client.post_async("http://localhost:8899/telemetry", telemetry, [](const Response& response) {
        FL_WARN("Telemetry upload: " << response.status_code());
    });
    
    // Add headers when needed (using default timeout)
    client.get_async("http://api.example.com/user", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("User data: " << response.text());
        }
    }, 5000, {
        {"Authorization", "Bearer user_token"},
        {"Accept", "application/json"}
    });
    
    // Custom timeout with headers
    client.post_async("http://slow-api.com/upload", telemetry, [](const Response& response) {
        FL_WARN("Slow upload: " << response.status_code());
    }, 20000, {
        {"Content-Type", "application/octet-stream"},
        {"X-Telemetry-Version", "1.0"}
    });
}

void loop() {
    // Callbacks fire automatically via EngineEvents
    FastLED.show();
}
```

### 3. **Mixed API Usage**

```cpp
void setup() {
    // No headers - simplest case
    auto simple_future = client.get("http://httpbin.org/json");
    
    // With headers using vector (when you need to build headers dynamically)
    fl::vector<fl::pair<fl::string, fl::string>> dynamic_headers;
    dynamic_headers.push_back({"Authorization", "Bearer " + get_current_token()});
    if (device_has_location()) {
        dynamic_headers.push_back({"X-Location", get_location_string()});
    }
    auto dynamic_future = client.get("http://api.example.com/location", dynamic_headers);
    
    // With headers using initializer list (when headers are known at compile time)
    auto static_future = client.get("http://api.example.com/config", {
        {"Authorization", "Bearer config_token"},
        {"Accept", "application/json"},
        {"Cache-Control", "no-cache"}
    });
}
```

### 4. **Real-World IoT Example**

```cpp
class IoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceToken;
    fl::string mDeviceId;
    
public:
    void authenticate(const char* token) { mDeviceToken = token; }
    void setDeviceId(const char* id) { mDeviceId = id; }
    
    void upload_sensor_data(fl::span<const fl::u8> data) {
        // Simple local upload (no auth needed)
        if (use_local_server) {
            auto future = mClient.post("http://192.168.1.100:8080/sensors", data);
            sensor_upload_future = fl::move(future);
            return;
        }
        
        // Cloud upload with custom timeout and auth headers
        auto future = mClient.post("https://iot.example.com/sensors", data, 10000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Timestamp", fl::to_string(millis())}
        });
        
        sensor_upload_future = fl::move(future);
    }
    
    void get_device_config() {
        // Local config (simple)
        if (use_local_server) {
            auto future = mClient.get("http://192.168.1.100:8080/config");
            config_future = fl::move(future);
            return;
        }
        
        // Cloud config with auth
        auto future = mClient.get("https://iot.example.com/device/config", 5000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"X-Device-ID", mDeviceId}
        });
        
        config_future = fl::move(future);
    }
    
    void ping_health_check() {
        // Simplest possible API usage!
        auto future = mClient.get("http://localhost:8899/health");
        health_future = fl::move(future);
    }
    
    void check_responses() {
        if (sensor_upload_future.is_ready()) {
            auto result = sensor_upload_future.try_result();
            FL_WARN("Sensor upload: " << (result && result->ok() ? "OK" : "FAILED"));
            sensor_upload_future = fl::future<Response>();
        }
        
        if (config_future.is_ready()) {
            auto result = config_future.try_result();
            if (result && result->ok()) {
                FL_WARN("Config: " << result->text());
                // Parse and apply config...
            }
            config_future = fl::future<Response>();
        }
        
        if (health_future.is_ready()) {
            auto result = health_future.try_result();
            FL_WARN("Health: " << (result && result->ok() ? "UP" : "DOWN"));
            health_future = fl::future<Response>();
        }
    }
    
private:
    fl::future<Response> sensor_upload_future;
    fl::future<Response> config_future;
    fl::future<Response> health_future;
    bool use_local_server = true; // Start with local, fall back to cloud
};

// Usage
IoTDevice device;

void setup() {
    device.authenticate("your_device_token");
    device.setDeviceId("fastled-sensor-001");
    
    // Health check every 10 seconds
    EVERY_N_SECONDS(10) {
        device.ping_health_check();
    }
    
    // Upload sensor reading every 30 seconds
    EVERY_N_SECONDS(30) {
        fl::vector<fl::u8> sensor_data = {temperature, humidity, light_level};
        device.upload_sensor_data(sensor_data);
    }
    
    // Get updated config every 5 minutes
    EVERY_N_SECONDS(300) {
        device.get_device_config();
    }
}

void loop() {
    device.check_responses();
    FastLED.show();
}
```

### 5. **Advanced: Multiple API Endpoints**

```cpp
class WeatherStationAPI {
private:
    HttpClient mClient;
    
public:
    void send_weather_data(float temp, float humidity, float pressure) {
        fl::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(temp));
        data.push_back(static_cast<uint8_t>(humidity));
        data.push_back(static_cast<uint8_t>(pressure));
        
        // Multiple APIs with different headers - all using initializer lists!
        
        // Send to primary weather service
        mClient.post_async("https://weather.primary.com/upload", data, {
            {"Authorization", "Bearer primary_key"},
            {"Content-Type", "application/octet-stream"},
            {"X-Station-ID", "station-001"}
        }, [](const Response& response) {
            FL_WARN("Primary upload: " << response.status_code());
        });
        
        // Send to backup weather service  
        mClient.post_async("https://weather.backup.com/data", data, {
            {"API-Key", "backup_api_key"},
            {"Content-Type", "application/octet-stream"},
            {"Station-Name", "FastLED Weather Station"}
        }, [](const Response& response) {
            FL_WARN("Backup upload: " << response.status_code());
        });
        
        // Send to local logger
        mClient.post_async("http://192.168.1.100/log", data, {
            {"Content-Type", "application/octet-stream"}
        }, [](const Response& response) {
            FL_WARN("Local log: " << response.status_code());
        });
    }
};
```

### 6. **Streaming Downloads (Memory-Efficient)**

```cpp
class DataLogger {
private:
    HttpClient mClient;
    fl::size mTotalBytes = 0;
    
public:
    void download_large_dataset() {
        // Download 100MB dataset with only 1KB RAM buffer
        mTotalBytes = 0;
        
        mClient.get_stream_async("https://api.example.com/large-dataset", {
            {"Authorization", "Bearer " + api_token},
            {"Accept", "application/octet-stream"}
        }, 
        // Chunk callback - called for each piece of data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process chunk immediately (only ~1KB in memory at a time)
            process_data_chunk(chunk);
            
            mTotalBytes += chunk.size();
            
            if (mTotalBytes % 10000 == 0) {
                FL_WARN("Downloaded: " << mTotalBytes << " bytes");
            }
            
            if (is_final) {
                FL_WARN("Download complete! Total: " << mTotalBytes << " bytes");
            }
            
            // Return true to continue, false to abort
            return true;
        },
        // Complete callback - called when done or error
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("Stream completed successfully");
                finalize_data_processing();
            } else {
                FL_WARN("Stream error " << status_code << ": " << error_msg);
                cleanup_partial_data();
            }
        });
    }
    
private:
    void process_data_chunk(fl::span<const uint8_t> chunk) {
        // Write directly to SD card, process incrementally, etc.
        // Only this small chunk is in RAM at any time!
        for (const uint8_t& byte : chunk) {
            // Process each byte...
            analyze_sensor_data(byte);
        }
    }
    
    void finalize_data_processing() {
        FL_WARN("All data processed with minimal RAM usage!");
    }
    
    void cleanup_partial_data() {
        FL_WARN("Cleaning up after failed download");
    }
};
```

### 7. **Streaming Uploads (Sensor Data Logging)**

```cpp
class SensorDataUploader {
private:
    HttpClient mClient;
    fl::size mDataIndex = 0;
    fl::vector<uint8_t> mSensorHistory; // Large accumulated data
    
public:
    void upload_accumulated_data() {
        mDataIndex = 0;
        
        mClient.post_stream("https://api.example.com/sensor-bulk", {
            {"Content-Type", "application/octet-stream"},
            {"Authorization", "Bearer " + upload_token},
            {"X-Data-Source", "weather-station-001"}
        },
        // Data provider callback - called when client needs more data
        [this](fl::span<uint8_t> buffer) -> fl::size {
            fl::size remaining = mSensorHistory.size() - mDataIndex;
            fl::size to_copy = fl::min(buffer.size(), remaining);
            
            if (to_copy > 0) {
                // Copy next chunk into the provided buffer
                fl::memcpy(buffer.data(), 
                          mSensorHistory.data() + mDataIndex, 
                          to_copy);
                mDataIndex += to_copy;
                
                FL_WARN("Uploaded chunk: " << to_copy << " bytes");
            }
            
            // Return 0 when no more data (signals end of stream)
            return to_copy;
        },
        // Complete callback
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("All sensor data uploaded successfully!");
                mSensorHistory.clear(); // Free memory
            } else {
                FL_WARN("Upload failed: " << status_code << " - " << error_msg);
                // Could retry later...
            }
        });
    }
    
    void add_sensor_reading(float temperature, float humidity) {
        // Accumulate data over time
        uint8_t temp_byte = static_cast<uint8_t>(temperature);
        uint8_t humidity_byte = static_cast<uint8_t>(humidity);
        
        mSensorHistory.push_back(temp_byte);
        mSensorHistory.push_back(humidity_byte);
        
        // Upload when we have enough data
        if (mSensorHistory.size() > 1000) {
            upload_accumulated_data();
        }
    }
};
```

### 8. **Real-Time Data Processing Stream**

```cpp
class RealTimeProcessor {
private:
    HttpClient mClient;
    fl::vector<float> mMovingAverage;
    fl::size mWindowSize = 100;
    
public:
    void start_realtime_stream() {
        // Connect to real-time sensor data stream
        mClient.get_stream_async("https://api.realtime.com/sensor-feed", {
            {"Authorization", "Bearer realtime_token"},
            {"Accept", "application/octet-stream"},
            {"X-Stream-Type", "continuous"}
        },
        // Process each chunk of real-time data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Convert binary data to sensor readings
            for (fl::size i = 0; i + 3 < chunk.size(); i += 4) {
                float sensor_value = parse_float_from_bytes(chunk.subspan(i, 4));
                
                // Update moving average
                mMovingAverage.push_back(sensor_value);
                if (mMovingAverage.size() > mWindowSize) {
                    mMovingAverage.erase(mMovingAverage.begin());
                }
                
                // Process in real-time
                process_sensor_value(sensor_value);
                
                // Update LED display based on current average
                update_led_visualization();
            }
            
            // Continue streaming unless we want to stop
            return !should_stop_stream();
        },
        // Handle stream completion or errors
        [](int status_code, const char* error_msg) {
            if (status_code != 200) {
                FL_WARN("Stream error: " << status_code << " - " << error_msg);
                // Could automatically reconnect here
            }
        });
    }
    
private:
    float parse_float_from_bytes(fl::span<const uint8_t> bytes) {
        // Convert 4 bytes to float (handle endianness)
        union { uint32_t i; float f; } converter;
        converter.i = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        return converter.f;
    }
    
    void process_sensor_value(float value) {
        // Real-time processing logic
        if (value > threshold) {
            trigger_alert();
        }
    }
    
    void update_led_visualization() {
        float avg = calculate_average();
        // Update FastLED display based on current data
        update_leds_with_value(avg);
    }
    
    bool should_stop_stream() {
        // Logic to determine when to stop streaming
        return false; // Run continuously in this example
    }
};
```

### 9. **Secure HTTPS Communication**

```cpp
class SecureIoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceId;
    fl::string mApiToken;
    
public:
    void setup_security() {
        // Configure TLS for maximum security
        HttpClient::TlsConfig tls_config;
        tls_config.verify_certificates = true;      // Always verify server certs
        tls_config.verify_hostname = true;          // Verify hostname matches cert
        tls_config.allow_self_signed = false;       // No self-signed certs
        tls_config.require_tls_v12_or_higher = true; // Modern TLS only
        mClient.set_tls_config(tls_config);
        
        // Load trusted root certificates
        int cert_count = mClient.load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << cert_count << " trusted certificates");
        
        // Pin certificate for critical API endpoint (optional, highest security)
        mClient.pin_certificate("api.critical-iot.com", 
                                "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3");
        
        // Save certificates to persistent storage
        mClient.save_certificates("/spiffs/ca_certs.bin");
    }
    
    void send_secure_telemetry() {
        fl::vector<uint8_t> encrypted_data = encrypt_sensor_readings();
        
        // All HTTPS calls automatically use TLS with configured security
        auto future = mClient.post("https://api.secure-iot.com/telemetry", encrypted_data, {
            {"Authorization", "Bearer " + mApiToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Security-Level", "high"}
        });
        
        // Check TLS connection details
        if (future.is_ready()) {
            auto result = future.try_result();
            if (result) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("TLS Version: " << tls_info.tls_version);
                FL_WARN("Cipher: " << tls_info.cipher_suite);
                FL_WARN("Cert Verified: " << (tls_info.certificate_verified ? "YES" : "NO"));
                FL_WARN("Hostname Verified: " << (tls_info.hostname_verified ? "YES" : "NO"));
            }
        }
    }
    
    void handle_certificate_update() {
        // Download and verify new certificate bundle
        mClient.get_stream_async("https://ca-updates.iot-service.com/latest-certs", {
            {"Authorization", "Bearer " + mApiToken},
            {"Accept", "application/x-pem-file"}
        },
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process certificate data as it arrives
            process_certificate_chunk(chunk);
            
            if (is_final) {
                // Validate and install new certificates
                if (validate_certificate_bundle()) {
                    mClient.clear_certificates();
                    install_new_certificates();
                    mClient.save_certificates("/spiffs/ca_certs.bin");
                    FL_WARN("Certificate bundle updated successfully");
                } else {
                    FL_WARN("Certificate validation failed - keeping old certificates");
                }
            }
            return true;
        },
        [](int status, const char* msg) {
            FL_WARN("Certificate update complete: " << status);
        });
    }
    
private:
    fl::vector<uint8_t> encrypt_sensor_readings() {
        // Your encryption logic here
        return fl::vector<uint8_t>{1, 2, 3, 4}; // Placeholder
    }
    
    void process_certificate_chunk(fl::span<const uint8_t> chunk) {
        // Accumulate certificate data
    }
    
    bool validate_certificate_bundle() {
        // Verify certificate signatures, validity dates, etc.
        return true; // Placeholder
    }
    
    void install_new_certificates() {
        // Install validated certificates
    }
};
```

### 10. **Development vs Production Security**

```cpp
class ConfigurableSecurityClient {
private:
    HttpClient mClient;
    bool mDevelopmentMode;
    
public:
    void setup(bool development_mode = false) {
        mDevelopmentMode = development_mode;
        
        HttpClient::TlsConfig tls_config;
        
        if (development_mode) {
            // Development settings - more permissive
            tls_config.verify_certificates = false;     // Allow dev servers
            tls_config.verify_hostname = false;         // Allow localhost
            tls_config.allow_self_signed = true;        // Allow self-signed certs
            tls_config.require_tls_v12_or_higher = false; // Allow older TLS for testing
            
            FL_WARN("DEVELOPMENT MODE: TLS verification disabled!");
            FL_WARN("DO NOT USE IN PRODUCTION!");
            
        } else {
            // Production settings - maximum security
            tls_config.verify_certificates = true;
            tls_config.verify_hostname = true;
            tls_config.allow_self_signed = false;
            tls_config.require_tls_v12_or_higher = true;
            
            // Load production certificate bundle
            mClient.load_ca_bundle(HttpClient::CA_BUNDLE_MOZILLA);
            
            // Pin critical certificates
            pin_production_certificates();
            
            FL_WARN("PRODUCTION MODE: Full TLS verification enabled");
        }
        
        mClient.set_tls_config(tls_config);
    }
    
    void test_api_connection() {
        const char* api_url = mDevelopmentMode ? 
            "https://localhost:8443/api/test" :      // Dev server
            "https://api.production.com/test";       // Production server
            
        mClient.get_async(api_url, {
            {"Authorization", "Bearer " + get_api_token()},
            {"User-Agent", "FastLED-Device/1.0"}
        }, [this](const Response& response) {
            if (response.ok()) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("API test successful");
                FL_WARN("Security level: " << (mDevelopmentMode ? "DEV" : "PRODUCTION"));
                FL_WARN("TLS: " << tls_info.tls_version << " with " << tls_info.cipher_suite);
            } else {
                FL_WARN("API test failed: " << response.status_code());
                if (!mDevelopmentMode) {
                    FL_WARN("Check certificate configuration");
                }
            }
        });
    }
    
private:
    void pin_production_certificates() {
        // Pin certificates for critical production services
        mClient.pin_certificate("api.production.com", 
                                "a1b2c3d4e5f6...production_cert_sha256");
        mClient.pin_certificate("cdn.production.com", 
                                "f6e5d4c3b2a1...cdn_cert_sha256");
    }
    
    fl::string get_api_token() {
        return mDevelopmentMode ? "dev_token_123" : "prod_token_xyz";
    }
};
```

### 11. **Certificate Management & Updates**

```cpp
class CertificateManager {
private:
    HttpClient* mClient;
    fl::string mCertStoragePath;
    
public:
    CertificateManager(HttpClient* client, const char* storage_path = "/spiffs/certs.bin") 
        : mClient(client), mCertStoragePath(storage_path) {}
    
    void initialize() {
        // Try loading saved certificates first
        int loaded = mClient->load_certificates(mCertStoragePath.c_str());
        
        if (loaded == 0) {
            FL_WARN("No saved certificates found, loading defaults");
            load_default_certificates();
        } else {
            FL_WARN("Loaded " << loaded << " certificates from storage");
            
            // Check if certificates need updating (once per day)
            EVERY_N_HOURS(24) {
                check_for_certificate_updates();
            }
        }
        
        // Display certificate statistics
        print_certificate_stats();
    }
    
    void load_default_certificates() {
        // Load minimal set for common IoT services
        int count = mClient->load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << count << " default certificates");
        
        // Add custom root CA if needed
        add_custom_root_ca();
        
        // Save for next boot
        mClient->save_certificates(mCertStoragePath.c_str());
    }
    
    void add_custom_root_ca() {
        // Add your organization's root CA
        const char* custom_ca = R"(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKX1+... (your custom root CA)
...certificate content...
-----END CERTIFICATE-----
)";
        
        if (mClient->add_root_certificate(custom_ca, "Company Root CA")) {
            FL_WARN("Added custom root CA");
        } else {
            FL_WARN("Failed to add custom root CA");
        }
    }
    
    void check_for_certificate_updates() {
        FL_WARN("Checking for certificate updates...");
        
        // Use HTTPS to download certificate updates (secured by existing certs)
        mClient->get_async("https://ca-updates.yourservice.com/latest", {
            {"User-Agent", "FastLED-CertUpdater/1.0"},
            {"Accept", "application/json"}
        }, [this](const Response& response) {
            if (response.ok()) {
                // Parse JSON response to check for updates
                if (parse_update_info(response.text())) {
                    download_certificate_updates();
                }
            } else {
                FL_WARN("Certificate update check failed: " << response.status_code());
            }
        });
    }
    
    void download_certificate_updates() {
        FL_WARN("Downloading certificate updates...");
        
        fl::vector<uint8_t> cert_data;
        
        mClient->get_stream_async("https://ca-updates.yourservice.com/bundle.pem", {
            {"Authorization", "Bearer " + get_update_token()}
        },
        [&cert_data](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Accumulate certificate data
            for (const uint8_t& byte : chunk) {
                cert_data.push_back(byte);
            }
            
            if (is_final) {
                FL_WARN("Downloaded " << cert_data.size() << " bytes of certificate data");
            }
            return true;
        },
        [this, &cert_data](int status, const char* msg) {
            if (status == 200) {
                process_certificate_update(cert_data);
            } else {
                FL_WARN("Certificate download failed: " << status << " - " << msg);
            }
        });
    }
    
    void print_certificate_stats() {
        auto stats = mClient->get_certificate_stats();
        FL_WARN("Certificate Statistics:");
        FL_WARN("  Certificates: " << stats.certificate_count);
        FL_WARN("  Total size: " << stats.total_certificate_size << " bytes");
        FL_WARN("  Pinned certs: " << stats.pinned_certificate_count);
        FL_WARN("  Capacity: " << stats.max_certificate_capacity);
    }
    
private:
    bool parse_update_info(const char* json_response) {
        // Parse JSON to check version, availability, etc.
        // Return true if update is needed
        return false; // Placeholder
    }
    
    fl::string get_update_token() {
        // Return authentication token for certificate updates
        return "cert_update_token_123";
    }
    
    void process_certificate_update(const fl::vector<uint8_t>& cert_data) {
        // Validate signature, parse certificates, update store
        FL_WARN("Processing certificate update...");
        
        // For demo, just save the new certificates
        // In practice, you'd validate signatures, check dates, etc.
        if (validate_certificate_signature(cert_data)) {
            backup_current_certificates();
            install_new_certificates(cert_data);
            mClient->save_certificates(mCertStoragePath.c_str());
            FL_WARN("Certificates updated successfully");
        } else {
            FL_WARN("Certificate validation failed - update rejected");
        }
    }
    
    bool validate_certificate_signature(const fl::vector<uint8_t>& cert_data) {
        // Verify certificate bundle signature
        return true; // Placeholder
    }
    
    void backup_current_certificates() {
        // Backup current certificates before updating
        fl::string backup_path = mCertStoragePath + ".backup";
        mClient->save_certificates(backup_path.c_str());
    }
    
    void install_new_certificates(const fl::vector<uint8_t>& cert_data) {
        // Clear existing and install new certificates
        mClient->clear_certificates();
        
        // Parse and add each certificate from the bundle
        // (In practice, you'd parse the PEM format)
        
        FL_WARN("New certificates installed");
    }
};
```

## TLS Implementation Details

### **Platform-Specific TLS Support**

#### **ESP32: mbedTLS Integration**

```cpp
#ifdef ESP32
// Internal implementation uses ESP-IDF mbedTLS
class ESP32TlsTransport : public TlsTransport {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt ca_chain;
    
public:
    bool connect(const char* hostname, int port) override {
        // Use ESP32 mbedTLS for TLS connections
        mbedtls_ssl_init(&ssl_ctx);
        mbedtls_ssl_config_init(&ssl_config);
        mbedtls_x509_crt_init(&ca_chain);
        
        // Configure for TLS client
        mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        
        // Set CA certificates
        mbedtls_ssl_conf_ca_chain(&ssl_config, &ca_chain, nullptr);
        
        // Enable hostname verification
        mbedtls_ssl_conf_authmode(&ssl_config, MBEDTLS_SSL_VERIFY_REQUIRED);
        
        return perform_handshake(hostname, port);
    }
};
#endif
```

#### **WebAssembly: Browser TLS**

```cpp
#ifdef __EMSCRIPTEN__
// Use browser's native TLS implementation
class EmscriptenTlsTransport : public TlsTransport {
public:
    bool connect(const char* hostname, int port) override {
        // Browser handles TLS automatically for HTTPS URLs
        // Certificate validation done by browser's certificate store
        
        EM_ASM({
            // JavaScript code to handle TLS connection
            const url = UTF8ToString($0) + ":" + $1;
            console.log("Connecting to: " + url);
            
            // Browser's fetch() API handles TLS automatically
        }, hostname, port);
        
        return true; // Browser handles all TLS details
    }
    
    // Certificate pinning via browser APIs
    void pin_certificate(const char* hostname, const char* sha256) override {
        EM_ASM({
            const host = UTF8ToString($0);
            const fingerprint = UTF8ToString($1);
            
            // Use browser's SubResource Integrity or other mechanisms
            console.log("Pinning certificate for " + host + ": " + fingerprint);
        }, hostname, sha256);
    }
};
#endif
```

#### **Native Platforms: OpenSSL/mbedTLS**

```cpp
#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// Use system OpenSSL or bundled mbedTLS
class NativeTlsTransport : public TlsTransport {
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
    X509_STORE* cert_store = nullptr;
    
public:
    bool connect(const char* hostname, int port) override {
        // Initialize OpenSSL
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        
        // Load CA certificates
        SSL_CTX_set_cert_store(ssl_ctx, cert_store);
        
        // Enable hostname verification
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
        
        return perform_ssl_handshake(hostname, port);
    }
};
#endif
```

### **Memory Management for Certificates**

#### **Compressed Certificate Storage**

```cpp
class CompressedCertificateStore {
private:
    struct CertificateEntry {
        fl::string name;
        fl::vector<uint8_t> compressed_cert;
        fl::string sha256_fingerprint;
        uint32_t expiry_timestamp;
    };
    
    fl::vector<CertificateEntry> mCertificates;
    fl::size mMaxStorageSize = 32768; // 32KB default
    
public:
    bool add_certificate(const char* pem_cert, const char* name) {
        // Parse PEM certificate
        auto cert_der = parse_pem_to_der(pem_cert);
        
        // Compress using simple RLE or zlib if available
        auto compressed = compress_certificate(cert_der);
        
        // Check if we have space
        if (get_total_size() + compressed.size() > mMaxStorageSize) {
            FL_WARN("Certificate store full, removing oldest");
            remove_oldest_certificate();
        }
        
        CertificateEntry entry;
        entry.name = name ? name : "Unknown";
        entry.compressed_cert = fl::move(compressed);
        entry.sha256_fingerprint = calculate_sha256(cert_der);
        entry.expiry_timestamp = extract_expiry_date(cert_der);
        
        mCertificates.push_back(fl::move(entry));
        return true;
    }
    
    fl::vector<uint8_t> get_certificate(const fl::string& fingerprint) {
        for (const auto& entry : mCertificates) {
            if (entry.sha256_fingerprint == fingerprint) {
                return decompress_certificate(entry.compressed_cert);
            }
        }
        return fl::vector<uint8_t>();
    }
    
private:
    fl::vector<uint8_t> compress_certificate(const fl::vector<uint8_t>& cert_der) {
        // Simple compression for certificates (they compress well)
        return cert_der; // Placeholder - use RLE or zlib
    }
    
    fl::vector<uint8_t> decompress_certificate(const fl::vector<uint8_t>& compressed) {
        return compressed; // Placeholder
    }
    
    fl::size get_total_size() const {
        fl::size total = 0;
        for (const auto& cert : mCertificates) {
            total += cert.compressed_cert.size();
        }
        return total;
    }
    
    void remove_oldest_certificate() {
        if (!mCertificates.empty()) {
            mCertificates.erase(mCertificates.begin());
        }
    }
};
```

### **TLS Error Handling**

```cpp
enum class TlsError {
    SUCCESS = 0,
    CERTIFICATE_VERIFICATION_FAILED,
    HOSTNAME_VERIFICATION_FAILED,
    CIPHER_NEGOTIATION_FAILED,
    HANDSHAKE_TIMEOUT,
    CERTIFICATE_EXPIRED,
    CERTIFICATE_NOT_TRUSTED,
    TLS_VERSION_NOT_SUPPORTED,
    MEMORY_ALLOCATION_FAILED,
    NETWORK_CONNECTION_FAILED
};

const char* tls_error_to_string(TlsError error) {
    switch (error) {
        case TlsError::SUCCESS: return "Success";
        case TlsError::CERTIFICATE_VERIFICATION_FAILED: return "Certificate verification failed";
        case TlsError::HOSTNAME_VERIFICATION_FAILED: return "Hostname verification failed";
        case TlsError::CIPHER_NEGOTIATION_FAILED: return "Cipher negotiation failed";
        case TlsError::HANDSHAKE_TIMEOUT: return "TLS handshake timeout";
        case TlsError::CERTIFICATE_EXPIRED: return "Certificate expired";
        case TlsError::CERTIFICATE_NOT_TRUSTED: return "Certificate not trusted";
        case TlsError::TLS_VERSION_NOT_SUPPORTED: return "TLS version not supported";
        case TlsError::MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case TlsError::NETWORK_CONNECTION_FAILED: return "Network connection failed";
        default: return "Unknown TLS error";
    }
}

// Enhanced error reporting in callbacks
client.get_async("https://api.example.com/data", {},
    [](const Response& response) {
        if (!response.ok()) {
            auto tls_info = client.get_last_tls_info();
            if (tls_info.is_secure && !tls_info.certificate_verified) {
                FL_WARN("TLS Error: Certificate verification failed");
                FL_WARN("Server cert: " << tls_info.server_certificate_sha256);
                FL_WARN("Consider adding root CA or pinning certificate");
            }
        }
    }
);
```

This comprehensive TLS design provides **enterprise-grade security** while maintaining FastLED's **simple, embedded-friendly approach**! 🔒

### **API Design Philosophy: Progressive Complexity**

**🎯 Start Simple, Add Complexity Only When Needed:**

The FastLED HTTP client follows a **progressive complexity** design where the simplest use cases require the least code:

**Level 1 - Minimal (Just URL):**
```cpp
client.get("http://localhost:8899/health");                    // Uses 5s timeout, no headers
client.post("http://localhost:8899/upload", sensor_data);     // Uses 5s timeout, no headers
```

**Level 2 - Custom Timeout:**
```cpp
client.get("http://slow-api.com/data", 15000);                // 15s timeout, no headers
client.post("http://slow-api.com/upload", data, 20000);       // 20s timeout, no headers
```

**Level 3 - Add Headers (Common Case):**
```cpp
client.get("http://api.com/data", 5000, {                     // 5s timeout, with headers
    {"Authorization", "Bearer token"}
});
client.post("http://api.com/upload", data, 10000, {           // 10s timeout, with headers
    {"Content-Type", "application/json"}
});
```

**Benefits of This Approach:**
- **✅ Zero barrier to entry** - health checks and simple APIs work with minimal code
- **✅ Sensible defaults** - 5 second timeout works for most local/fast APIs
- **✅ Incremental complexity** - add timeouts/headers only when actually needed
- **✅ Familiar C++ pattern** - default parameters are a well-understood C++ idiom

### **Why fl::string Shared Buffer Semantics Are Perfect for Networking**

**fl::string Benefits from Codebase Analysis:**
- **Copy-on-write semantics**: Copying `fl::string` only copies a shared_ptr (8-16 bytes)
- **Small string optimization**: Strings ≤64 bytes stored inline (no heap allocation)  
- **Shared ownership**: Multiple strings can share the same buffer efficiently
- **Automatic memory management**: RAII cleanup via shared_ptr
- **Thread-safe sharing**: Safe to share between threads with copy-on-write

**Strategic API Design Decisions:**

**✅ Use `const fl::string&` for:**
- **Return values** that might be stored/shared (response text, error messages)
- **Error messages** in callbacks (efficient sharing across multiple futures)
- **HTTP headers** that might be cached or logged
- **Large content** that benefits from shared ownership
- **Any data the receiver might store**

**✅ Keep `const char*` for:**
- **URLs** (usually string literals, consumed immediately)
- **HTTP methods** ("GET", "POST" - compile-time constants)
- **Immediate consumption** parameters (no storage needed)
- **Legacy C-style access** when raw char* is required

**Memory Efficiency Examples:**

```cpp
// Shared error messages across multiple futures - only ONE copy in memory!
fl::string network_timeout_error("Connection timeout after 30 seconds");

auto future1 = fl::make_error_future<Response>(network_timeout_error);
auto future2 = fl::make_error_future<Response>(network_timeout_error); 
auto future3 = fl::make_error_future<Response>(network_timeout_error);
// All three futures share the same error string buffer via copy-on-write!

// Response text sharing
void handle_response(const Response& response) {
    // Efficient sharing - no string copy!
    fl::string cached_response = response.text(); // Cheap shared_ptr copy
    
    // Can store, log, display, etc. without memory waste
    log_response(cached_response);
    display_on_screen(cached_response);
    cache_for_later(cached_response);
    // All share the same underlying buffer!
}

// Error message propagation in callbacks
client.get_stream_async("http://api.example.com/data", {},
    [](fl::span<const fl::u8> chunk, bool is_final) { return true; },
    [](int status, const fl::string& error_msg) {
        // Can efficiently store/share error message
        last_error = error_msg;        // Cheap copy
        log_error(error_msg);          // Can share same buffer
        display_error(error_msg);      // No extra memory used
        // Single string buffer shared across all uses!
    }
);
```

**Embedded System Benefits:**

1. **Memory Conservation**: Shared buffers reduce memory fragmentation
2. **Cache Efficiency**: Multiple references to same data improve cache locality
3. **Copy Performance**: Copy-on-write makes string passing nearly free
4. **RAII Safety**: Automatic cleanup prevents memory leaks
5. **Thread Safety**: Safe sharing between threads without manual synchronization

### **API Design Pattern Summary**

The FastLED networking API uses consistent naming patterns to make the right choice obvious:

**📊 Complete API Overview:**

| **Method Pattern** | **Returns** | **Use Case** | **Example** |
|-------------------|-------------|--------------|-------------|
| `get(url, timeout=5000, headers={})` | `fl::future<Response>` | Single HTTP request, check result later | Weather data, API calls |
| `get_async(url, callback, timeout=5000, headers={})` | `void` | Single HTTP request, immediate callback | Fire-and-forget requests |
| `get_stream_async(url, chunk_cb, done_cb)` | `void` | Large data, real-time streaming | File downloads, sensor feeds |
| `post(url, data, timeout=5000, headers={})` | `fl::future<Response>` | Single upload, check result later | Sensor data upload |
| `post_async(url, data, callback, timeout=5000, headers={})` | `void` | Single upload, immediate callback | Quick data submission |
| `post_stream_async(url, provider, done_cb)` | `void` | Large uploads, chunked sending | Log files, large datasets |

**🎯 When to Use Each Pattern:**

**✅ Use `get()` / `post()` (Futures) When:**
- You need to check the result later in `loop()`
- You want to store/cache the response
- You need error handling flexibility
- You're doing multiple concurrent requests

**✅ Use `get_async()` / `post_async()` (Callbacks) When:**
- You want immediate response processing
- The response is simple (success/failure)
- You don't need to store the response
- Fire-and-forget pattern is acceptable

**✅ Use `get_stream_async()` / `post_stream_async()` (Streaming) When:**
- Data is too large for memory (>1KB on embedded)
- You need real-time processing (sensor feeds)
- Progress monitoring is important
- Memory efficiency is critical

**🔧 Practical Decision Guide:**

```cpp
// ✅ Perfect for futures - check result when convenient
auto health_future = client.get("http://localhost:8899/health");        // Simplest!
auto weather_future = client.get("http://weather.api.com/current", 10000, {
    {"Authorization", "Bearer token"}
});
// Later in loop(): if (weather_future.is_ready()) { ... }

// ✅ Perfect for callbacks - immediate simple processing  
client.get_async("http://api.com/ping", [](const Response& r) {
    FL_WARN("API status: " << (r.ok() ? "UP" : "DOWN"));
});                                                                      // Uses defaults!

// ✅ Perfect for streaming - large data with progress
client.get_stream_async("http://api.com/dataset.bin", {},
    [](fl::span<const fl::u8> chunk, bool final) { 
        process_chunk(chunk); 
        return true; 
    },
    [](int status, const fl::string& error) { 
        FL_WARN("Download: " << status); 
    }
);
```

**💡 Pro Tips:**
- **Start simple**: Most APIs work with just `client.get("http://localhost:8899/health")`
- **Add complexity gradually**: Add timeout/headers only when needed
- **Mix and match**: Use futures for some requests, callbacks for others
- **Error sharing**: `fl::string` errors can be stored/shared efficiently  
- **Memory efficiency**: Streaming APIs prevent memory exhaustion
- **Thread safety**: All patterns work safely with FastLED's threading model

## 🎯 **Key API Improvements Summary**

**✅ What Changed:**
- **Default timeout**: All methods now default to 5000ms (5 seconds)
- **Default headers**: All methods now default to empty headers (`{}`)
- **Removed overloads**: No separate initializer_list overloads (fl::span handles both)
- **Simplified signatures**: Most common case is now just URL + data (for POST)
- **Progressive complexity**: Add timeout/headers only when actually needed

**✅ Usage Comparison:**

| **Before (Verbose)** | **After (Simple)** |
|---------------------|-------------------|
| `get(url, span<headers>)` | `get(url)` |
| `get(url, {})` | `get(url)` |
| `get_async(url, {}, callback)` | `get_async(url, callback)` |
| `post(url, data, span<headers>)` | `post(url, data)` |
| Multiple overloads for headers | Single method handles all cases |

**✅ Benefits:**
- **✅ Zero learning curve** - health checks work with minimal code
- **✅ Faster prototyping** - test APIs without configuring timeouts/headers
- **✅ Fewer errors** - sensible defaults prevent common timeout issues
- **✅ Simplified API** - single method handles vectors, arrays, and initializer lists
- **✅ Backward compatible** - advanced usage still works exactly the same

**✅ Perfect For:**
- **Health checks**: `client.get("http://localhost:8899/health")`
- **Local APIs**: `client.post("http://192.168.1.100/data", sensor_data)`
- **Simple testing**: Quick API exploration without boilerplate
- **Development**: Focus on logic, not HTTP configuration

## 🔧 **fl::span Header Conversion Magic**

The `headers` parameter uses `fl::span<const fl::pair<fl::string, fl::string>>` which **automatically converts** from:

```cpp
// ✅ Initializer lists (most common)
client.get("http://api.com/data", 5000, {
    {"Authorization", "Bearer token"},
    {"Content-Type", "application/json"}
});

// ✅ fl::vector (when building headers dynamically)
fl::vector<fl::pair<fl::string, fl::string>> headers;
headers.push_back({"Authorization", get_token()});
client.get("http://api.com/data", 5000, headers);

// ✅ C-style arrays (when headers are static)
fl::pair<fl::string, fl::string> auth_headers[] = {
    {"Authorization", "Bearer static_token"}
};
client.get("http://api.com/data", 5000, auth_headers);

// ✅ Empty headers (uses default)
client.get("http://api.com/data"); // No headers needed
```

**Result**: **One method signature handles all use cases** - no overload explosion! 🎯

### **Response Object API Improvements**

The Response object has been optimized for FastLED's patterns and embedded use:

**✅ Binary Data Access:**
```cpp
// Modern fl::span interface - consistent with FastLED patterns
fl::span<const fl::u8> binary_data = response.binary();  // Returns span
fl::span<const fl::u8> same_data = response.data();      // Alias for consistency
fl::size data_size = response.size();                    // FastLED size type

// Easy span operations
for (const fl::u8& byte : response.binary()) {
    process_byte(byte);
}

// Direct memory access when needed
const fl::u8* raw_ptr = response.binary().data();
```

**✅ Text Content Access:**
```cpp
// Primary interface - fl::string with copy-on-write semantics
fl::string response_text = response.text();  // Cheap copy, shared buffer!

// Can store, cache, log efficiently
cache_response(response_text);    // Same buffer shared
log_response(response_text);      // No extra memory used
display_text(response_text);      // Efficient sharing

// Legacy C-style access when needed
const char* c_str = response.c_str();
fl::size len = response.text_length();
```

**🎯 Key Benefits:**

1. **fl::span<const fl::u8> binary()** instead of raw pointer:
   - **Type safety**: Carries size information automatically
   - **Range safety**: Prevents buffer overruns 
   - **Iterator support**: Works with range-based for loops
   - **FastLED consistent**: Uses fl::u8 type throughout codebase

2. **fl::string text()** instead of const reference:
   - **Copy-on-write**: Cheap to return by value (8-16 byte shared_ptr copy)
   - **Storage friendly**: Can store result without lifetime concerns
   - **Shared ownership**: Multiple users can share the same buffer
   - **Memory efficient**: Small strings (≤64 bytes) stored inline

3. **Consistent fl:: types** throughout:
   - **fl::u8** instead of uint8_t for consistency
   - **fl::size** instead of size_t for embedded compatibility
   - **fl::string** for shared buffer semantics

**Real-World Usage Benefits:**
```cpp
void handle_response(const Response& resp) {
    if (resp.ok()) {
        // Can store both text and binary efficiently
        fl::string text_data = resp.text();           // Shared buffer
        fl::span<const fl::u8> binary_data = resp.binary(); // Type-safe span
        
        // Store for later use - no copies, shared ownership
        cached_text = text_data;      // Cheap shared_ptr copy
        cached_binary = binary_data;  // Just pointer + size
        
        // Process safely with iterators
        for (const fl::u8& byte : binary_data) {
            analyze_byte(byte);
        }
    }
}
```
