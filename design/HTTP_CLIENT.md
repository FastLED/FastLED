# FastLED HTTP Client Design

## ⚠️ IMPLEMENTATION STATUS

**🔄 PARTIALLY IMPLEMENTED - Mixed Implementation State**

| Component | Status | Implementation Status |
|-----------|--------|----------------------|
| **Core Types** | ✅ **COMPLETE** | `Request`, `Response`, `HttpHeaders`, `HttpStatusCode` - Fully implemented |
| **fl::future<T>** | ✅ **COMPLETE** | Complete event-driven futures with blocking/non-blocking support |
| **Transport Interface** | ✅ **COMPLETE** | `Transport` base class and factory pattern implemented |
| **TCP/TLS Transports** | 🔄 **STUB ONLY** | Headers + stub implementations returning fake responses |
| **Socket System** | 🔄 **INTERFACE ONLY** | Socket interface defined, platform implementations vary |
| **HttpClient Class** | ❌ **HEADER ONLY** | Complete API defined in header, no implementation (.cpp) |
| **RequestBuilder Class** | ❌ **HEADER ONLY** | Complete API defined in header, no implementation |
| **Simple HTTP Functions** | ❌ **NOT IMPLEMENTED** | `http_get()`, `http_post()` etc. declared but not implemented |
| **Connection Pooling** | 🔄 **PARTIAL** | `SimpleConnectionPool` class defined, implementation incomplete |

**✅ INFRASTRUCTURE READY:**
- `fl::future<T>` - ✅ **COMPLETE** - Full event-driven futures with timeout support
- `fl::string`, `fl::span`, `fl::vector`, `fl::shared_ptr`, `fl::mutex` - ✅ **COMPLETE**
- HTTP types (`Request`, `Response`, `HttpHeaders`) - ✅ **COMPLETE**
- Transport abstraction layer - ✅ **COMPLETE**

**🔄 PARTIALLY IMPLEMENTED:**
- TCP/TLS transport stubs - Return fake responses for testing
- Socket interface - Defined but platform implementations incomplete
- Connection pool framework - Structure exists but needs implementation

**❌ MISSING IMPLEMENTATIONS:**
- `HttpClient` class implementation (only header exists)
- `RequestBuilder` class implementation (only header exists)  
- Simple HTTP convenience functions
- Real HTTP protocol parsing/generation
- Actual network socket implementations
- Full connection pooling logic

**🎯 NEXT STEPS:**
1. Implement `HttpClient` class methods in `http_client.cpp`
2. Implement `RequestBuilder` class methods
3. Replace transport stubs with real HTTP protocol handling
4. Complete socket implementations for target platforms
5. Implement simple HTTP convenience functions

**📝 KEY ARCHITECTURAL DECISION:**
The implementation uses **`fl::future<T>` throughout** instead of `fl::optional<T>` as originally designed. This provides better asynchronous support for embedded systems.

## Design Goals

- **Progressive Complexity**: Simple defaults that scale to advanced features
- **Memory Efficient**: Use `fl::deque` with PSRAM support for request/response bodies  
- **Transport Abstraction**: ✅ **IMPLEMENTED** - Pluggable backends for different networking stacks
- **FastLED Integration**: Non-blocking operation compatible with LED updates
- **Authentication Support**: Multiple authentication methods built-in
- **Streaming Support**: Handle large uploads/downloads efficiently

## Core API

### **1. Simple HTTP Functions (Level 1) - ❌ NOT IMPLEMENTED**

**🚨 API CHANGE:** All functions return `fl::future<Response>` instead of `fl::optional<Response>`

```cpp
namespace fl {

/// Simple HTTP GET request
/// @param url the URL to request
/// @returns future that resolves to response  
fl::future<Response> http_get(const fl::string& url);

/// Simple HTTP POST request with data
/// @param url the URL to post to
/// @param data the data to send
/// @param content_type the content type (default: application/octet-stream)
/// @returns future that resolves to response
fl::future<Response> http_post(const fl::string& url, 
                               fl::span<const fl::u8> data,
                               const fl::string& content_type = "application/octet-stream");

/// Simple HTTP POST request with text
/// @param url the URL to post to  
/// @param text the text data to send
/// @param content_type the content type (default: text/plain)
/// @returns future that resolves to response
fl::future<Response> http_post(const fl::string& url,
                               const fl::string& text,
                               const fl::string& content_type = "text/plain");

/// Simple HTTP POST request with JSON
/// @param url the URL to post to
/// @param json the JSON data to send
/// @returns future that resolves to response
fl::future<Response> http_post_json(const fl::string& url, const fl::string& json);

/// Simple HTTP PUT request
fl::future<Response> http_put(const fl::string& url, 
                              fl::span<const fl::u8> data,
                              const fl::string& content_type = "application/octet-stream");

/// Simple HTTP DELETE request
fl::future<Response> http_delete(const fl::string& url);

} // namespace fl
```

### **2. HTTP Client Class (Level 2) - ❌ HEADER ONLY**

**🚨 API CHANGE:** All methods return `fl::future<T>` instead of `fl::optional<T>`

```cpp
namespace fl {

/// HTTP Client with configuration and session management
class HttpClient {
public:
    /// Client configuration options
    struct Config {
        fl::u32 timeout_ms = 10000;                          ///< Request timeout
        fl::u32 connect_timeout_ms = 5000;                   ///< Connection timeout
        fl::size max_redirects = 5;                          ///< Maximum redirect follows
        bool follow_redirects = true;                        ///< Whether to follow redirects
        fl::string user_agent = "FastLED/1.0";              ///< User agent string
        fl::vector<fl::pair<fl::string, fl::string>> default_headers; ///< Default headers
        bool verify_ssl = true;                              ///< Verify SSL certificates
        fl::string ca_bundle_path;                           ///< CA bundle for SSL verification
        fl::size max_response_size = 10485760;               ///< Max response size (10MB)
        fl::size buffer_size = 8192;                         ///< Internal buffer size
        bool enable_compression = true;                      ///< Accept gzip compression
        bool enable_keepalive = true;                        ///< Use HTTP keep-alive
        fl::u32 keepalive_timeout_ms = 30000;                ///< Keep-alive timeout
    };
    
    /// Create HTTP client with default configuration
    HttpClient() = default;
    
    /// Create HTTP client with custom configuration
    explicit HttpClient(const Config& config);
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::shared_ptr<Transport> transport, const Config& config = {});
    
    /// Destructor
    ~HttpClient();
    
    // ========== SIMPLE REQUEST METHODS ==========
    
    /// HTTP GET request
    /// @param url the URL to request
    /// @returns future that resolves to response
    fl::future<Response> get(const fl::string& url);
    
    /// HTTP POST request with data
    fl::future<Response> post(const fl::string& url, fl::span<const fl::u8> data,
                              const fl::string& content_type = "application/octet-stream");
    
    /// HTTP POST request with text
    fl::future<Response> post(const fl::string& url, const fl::string& text,
                              const fl::string& content_type = "text/plain");
    
    /// HTTP PUT request
    fl::future<Response> put(const fl::string& url, fl::span<const fl::u8> data,
                             const fl::string& content_type = "application/octet-stream");
    
    /// HTTP DELETE request  
    fl::future<Response> delete_(const fl::string& url);  // delete is keyword
    
    /// HTTP HEAD request
    fl::future<Response> head(const fl::string& url);
    
    /// HTTP OPTIONS request
    fl::future<Response> options(const fl::string& url);
    
    /// HTTP PATCH request
    fl::future<Response> patch(const fl::string& url, fl::span<const fl::u8> data,
                               const fl::string& content_type = "application/octet-stream");
    
    // ========== ADVANCED REQUEST METHODS ==========
    
    /// Send custom request
    fl::future<Response> send(const Request& request);
    
    /// Send request asynchronously (returns future immediately)
    fl::future<Response> send_async(const Request& request);
    
    /// Download file to local storage
    fl::future<bool> download_file(const fl::string& url, const fl::string& local_path);
    
    /// Upload file from local storage
    fl::future<Response> upload_file(const fl::string& url, const fl::string& file_path,
                                     const fl::string& field_name = "file");
    
    /// Stream download with custom processor
    fl::future<bool> stream_download(const fl::string& url, 
                                    fl::function<bool(fl::span<const fl::u8>)> data_processor);
    
    /// Stream upload with custom provider
    fl::future<Response> stream_upload(const fl::string& url,
                                      fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider,
                                      const fl::string& content_type = "application/octet-stream");
    
    // ========== CONFIGURATION METHODS ==========
    // All configuration methods are declared in header
    
    // ========== FACTORY METHODS ==========
    
    /// Create client with specific transport type
    static fl::shared_ptr<HttpClient> create_with_tcp_transport();
    static fl::shared_ptr<HttpClient> create_with_tls_transport();
    static fl::shared_ptr<HttpClient> create_with_transport(fl::shared_ptr<Transport> transport);
    
    /// Create clients for common use cases
    static fl::shared_ptr<HttpClient> create_simple_client();
    static fl::shared_ptr<HttpClient> create_secure_client(bool verify_ssl = true);
    static fl::shared_ptr<HttpClient> create_api_client(const fl::string& base_url, const fl::string& api_key);
    
    // ... (full configuration API exists in header)
};

} // namespace fl
```

### **3. Request Builder (Level 3) - ❌ HEADER ONLY**

```cpp
namespace fl {

/// Request builder for constructing complex HTTP requests
class RequestBuilder {
public:
    /// Create request builder
    RequestBuilder(const fl::string& method, const fl::string& url, HttpClient* client = nullptr);
    
    // ========== BUILDER PATTERN METHODS ==========
    // All methods declared in header with full fluent API
    
    /// Build and send request
    Request build() const;
    fl::future<Response> send();
    fl::future<Response> send_async();
    
    /// Streaming operations
    fl::future<bool> download_to_file(const fl::string& file_path);
    fl::future<bool> stream_download(fl::function<bool(fl::span<const fl::u8>)> data_processor);
    fl::future<Response> stream_upload(fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider);
    
    // ... (full builder API exists in header)
};

} // namespace fl
```

## Transport Abstraction - ✅ IMPLEMENTED

### **1. Transport Interface - ✅ COMPLETE**

```cpp
namespace fl {

/// HTTP transport interface for different networking backends
class Transport {
public:
    virtual ~Transport() = default;
    
    /// Send HTTP request and receive response
    virtual fl::future<Response> send_request(const Request& request) = 0;
    
    /// Send request asynchronously (all requests are async now)
    virtual fl::future<Response> send_request_async(const Request& request) = 0;
    
    /// Check if transport supports the given URL scheme
    virtual bool supports_scheme(const fl::string& scheme) const = 0;
    
    /// Transport capabilities
    virtual bool supports_streaming() const = 0;
    virtual bool supports_keepalive() const = 0;
    virtual bool supports_compression() const = 0;
    virtual bool supports_ssl() const = 0;
    
    /// Connection management
    virtual fl::size get_active_connections() const = 0;
    virtual void close_all_connections() = 0;
    
    /// Transport info
    virtual fl::string get_transport_name() const = 0;
    virtual fl::string get_transport_version() const = 0;
    
    /// Stream operations
    virtual fl::future<bool> stream_download(const Request& request,
                                            fl::function<bool(fl::span<const fl::u8>)> data_processor) = 0;
    virtual fl::future<Response> stream_upload(const Request& request,
                                               fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider) = 0;
};

} // namespace fl
```

### **2. TCP Transport Implementation - 🔄 STUB ONLY**

```cpp
namespace fl {

/// TCP/HTTP transport - CURRENTLY STUB IMPLEMENTATION
class TcpTransport : public Transport {
public:
    fl::future<Response> send_request(const Request& request) override {
        // 🚨 STUB: Returns fake response
        Response response(HttpStatusCode::OK);
        response.set_body(fl::string("Stub HTTP response from TCP transport"));
        response.set_header("Content-Type", "text/plain");
        response.set_header("Server", "FastLED-TCP-Stub");
        
        return fl::make_ready_future(response);
    }
    
    // ... other methods are stub implementations
};

} // namespace fl
```

### **3. TLS Transport Implementation - 🔄 STUB ONLY**

```cpp
namespace fl {

/// TLS/HTTPS transport - CURRENTLY STUB IMPLEMENTATION  
class TlsTransport : public Transport {
public:
    fl::future<Response> send_request(const Request& request) override {
        // 🚨 STUB: Returns fake HTTPS response
        Response response(HttpStatusCode::OK);
        response.set_body(fl::string("Stub HTTPS response from TLS transport"));
        response.set_header("Content-Type", "text/plain");
        response.set_header("Server", "FastLED-TLS-Stub");
        
        return fl::make_ready_future(response);
    }
    
    // ... other methods are stub implementations
};

} // namespace fl
```

### **4. Transport Factory - ✅ COMPLETE**

```cpp
namespace fl {

/// Transport factory for creating transport instances
class TransportFactory {
public:
    /// Create transport based on URL scheme
    static fl::shared_ptr<Transport> create_for_scheme(const fl::string& scheme);
    
    /// Create transport with specific options
    static fl::shared_ptr<Transport> create_tcp_transport();
    static fl::shared_ptr<Transport> create_tls_transport();
    
    /// Register custom transport implementation
    using TransportCreator = fl::function<fl::shared_ptr<Transport>()>;
    static void register_transport(const fl::string& scheme, TransportCreator creator);
    
    /// Check if scheme is supported
    static bool is_scheme_supported(const fl::string& scheme);
    
    /// Get list of supported schemes
    static fl::vector<fl::string> get_supported_schemes();
};

} // namespace fl
```

## Usage Examples

### **1. Current Testing Usage (With Stubs)**

```cpp
#include "fl/networking/http_client.h"

void test_stub_responses() {
    // Create transport directly (HttpClient not yet implemented)
    auto tcp_transport = fl::TransportFactory::create_tcp_transport();
    auto tls_transport = fl::TransportFactory::create_tls_transport();
    
    // Create basic requests
    fl::Request http_request(fl::HttpMethod::GET, "http://example.com");
    fl::Request https_request(fl::HttpMethod::GET, "https://example.com");
    
    // Send requests (currently returns stub responses)
    auto http_future = tcp_transport->send_request(http_request);
    auto https_future = tls_transport->send_request(https_request);
    
    // Get results (immediate since stubs return ready futures)
    auto http_result = http_future.try_get_result();
    auto https_result = https_future.try_get_result();
    
    if (http_result.is<fl::response>()) {
        auto response = *http_result.ptr<fl::response>();
        FL_WARN("HTTP Response: " << response.get_body_text());
        // Output: "Stub HTTP response from TCP transport"
    }
    
    if (https_result.is<fl::response>()) {
        auto response = *https_result.ptr<fl::response>();
        FL_WARN("HTTPS Response: " << response.get_body_text());
        // Output: "Stub HTTPS response from TLS transport"
    }
}
```

### **2. Future Usage (When Implementation Complete)**

```cpp
#include "fl/networking/http_client.h"

void fetch_weather_data() {
    // Simple GET request (when implemented)
    auto weather_future = fl::http_get("http://api.weather.com/current");
    
    // Check result in main loop
    auto result = weather_future.try_get_result();
    if (result.is<fl::response>()) {
        auto response = *result.ptr<fl::response>();
        if (response.is_success()) {
            fl::string weather_json = response.get_body_text();
            FL_WARN("Weather data: " << weather_json);
            update_weather_display(weather_json);
        }
    } else if (result.is<fl::FutureError>()) {
        FL_WARN("Weather request failed: " << result.ptr<fl::FutureError>()->message);
    }
    // else: still pending, check again later
}

void send_sensor_data() {
    // Simple POST request with JSON (when implemented)
    fl::string sensor_json = "{\"temperature\": 25.5, \"humidity\": 60.2}";
    auto upload_future = fl::http_post_json("http://api.iot.com/sensors", sensor_json);
    
    // Check result
    auto result = upload_future.try_get_result();
    if (result.is<fl::response>()) {
        auto response = *result.ptr<fl::response>();
        if (response.get_status_code() == fl::HttpStatusCode::CREATED) {
            FL_WARN("Sensor data sent successfully");
        }
    } else if (result.is<fl::FutureError>()) {
        FL_WARN("Failed to send sensor data: " << result.ptr<fl::FutureError>()->message);
    }
}
```

### **3. Configured HTTP Client (When Implemented)**

```cpp
#include "fl/networking/http_client.h"

class WeatherService {
private:
    fl::shared_ptr<fl::HttpClient> mClient;
    
public:
    void initialize() {
        // Create and configure HTTP client (when implemented)
        mClient = fl::HttpClient::create_simple_client();
        mClient->set_timeout(15000);
        mClient->set_user_agent("FastLED-Weather/1.0");
        mClient->set_header("Accept", "application/json");
        mClient->set_api_key("your_api_key_here", "X-API-Key");
        
        // Enable keep-alive for efficiency
        mClient->set_enable_keepalive(true);
        mClient->set_keepalive_timeout(60000);
    }
    
    void update_weather() {
        auto weather_future = mClient->get("http://api.weather.com/current?location=Seattle");
        
        auto result = weather_future.try_get_result();
        if (result.is<fl::response>()) {
            auto response = *result.ptr<fl::response>();
            if (response.is_success()) {
                process_weather_data(response.get_body_text());
            }
        } else if (result.is<fl::FutureError>()) {
            FL_WARN("Weather update failed: " << result.ptr<fl::FutureError>()->message);
        }
    }
    
private:
    void process_weather_data(const fl::string& json) {
        // Parse weather JSON and update LED display
        FL_WARN("Processing weather data: " << json);
    }
};
```

## Integration with FastLED

### **Event-Driven Pattern with fl::future**

The implemented `fl::future<T>` system perfectly integrates with FastLED's event loop:

```cpp
class NetworkManager {
private:
    fl::future<fl::response> mWeatherRequest;
    fl::future<fl::response> mSensorUpload;
    
public:
    void setup() {
        // Start async requests (when implemented)
        mWeatherRequest = fl::http_get("http://api.weather.com/current");
        
        fl::string sensor_data = get_sensor_readings();
        mSensorUpload = fl::http_post("http://api.iot.com/upload", sensor_data);
    }
    
    void loop() {
        // Check weather request non-blockingly
        auto weather_result = mWeatherRequest.try_get_result();
        if (weather_result.is<fl::response>()) {
            auto response = *weather_result.ptr<fl::response>();
            update_weather_leds(response.get_body_text());
            mWeatherRequest.clear(); // Mark as processed
        } else if (weather_result.is<fl::FutureError>()) {
            handle_weather_error(weather_result.ptr<fl::FutureError>()->message);
            mWeatherRequest.clear();
        }
        
        // Check sensor upload non-blockingly  
        auto upload_result = mSensorUpload.try_get_result();
        if (upload_result.is<fl::response>()) {
            FL_WARN("Sensor upload complete");
            mSensorUpload.clear();
        } else if (upload_result.is<fl::FutureError>()) {
            FL_WARN("Sensor upload failed: " << upload_result.ptr<fl::FutureError>()->message);
            mSensorUpload.clear();
        }
        
        // LEDs update smoothly - never blocked by network I/O!
        FastLED.show();
    }
    
private:
    void update_weather_leds(const fl::string& weather_json) {
        // Update LED display based on weather
    }
    
    void handle_weather_error(const fl::string& error) {
        // Show error indication on LEDs
    }
    
    fl::string get_sensor_readings() {
        return "{\"temperature\": 25.5, \"humidity\": 60.2}";
    }
};
```

## Implementation Priority

**High Priority (Core Functionality):**
1. ✅ **Complete** - HTTP types and fl::future system
2. ❌ **Missing** - HttpClient class implementation
3. ❌ **Missing** - Basic HTTP protocol parsing/generation
4. 🔄 **Upgrade** - Replace transport stubs with real networking

**Medium Priority (Advanced Features):**
1. ❌ **Missing** - RequestBuilder implementation  
2. ❌ **Missing** - Connection pooling completion
3. ❌ **Missing** - Authentication and SSL/TLS features

**Low Priority (Convenience):**
1. ❌ **Missing** - Simple HTTP convenience functions
2. ❌ **Missing** - Streaming upload/download
3. ❌ **Missing** - File upload/download helpers

The HTTP client design provides a solid foundation with futures-based async support. The transport abstraction is complete and the type system is fully implemented, making it ready for the core HttpClient implementation. 
