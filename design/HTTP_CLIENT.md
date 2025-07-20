# FastLED HTTP Client Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

This document defines the HTTP client API for FastLED, providing both simple one-line requests and advanced configurable clients with transport abstraction, authentication, and streaming support.

## Design Goals

- **Progressive Complexity**: Simple defaults that scale to advanced features
- **Memory Efficient**: Use `fl::deque` with PSRAM support for request/response bodies
- **Transport Abstraction**: Pluggable backends for different networking stacks
- **FastLED Integration**: Non-blocking operation compatible with LED updates
- **Authentication Support**: Multiple authentication methods built-in
- **Streaming Support**: Handle large uploads/downloads efficiently

## Core API

### **1. Simple HTTP Functions (Level 1)**

For basic HTTP requests with minimal setup:

```cpp
namespace fl {

/// Simple HTTP GET request
/// @param url the URL to request
/// @returns response or empty optional on error
fl::optional<Response> http_get(const fl::string& url);

/// Simple HTTP POST request with data
/// @param url the URL to post to
/// @param data the data to send
/// @param content_type the content type (default: application/octet-stream)
/// @returns response or empty optional on error
fl::optional<Response> http_post(const fl::string& url, 
                                 fl::span<const fl::u8> data,
                                 const fl::string& content_type = "application/octet-stream");

/// Simple HTTP POST request with text
/// @param url the URL to post to  
/// @param text the text data to send
/// @param content_type the content type (default: text/plain)
/// @returns response or empty optional on error
fl::optional<Response> http_post(const fl::string& url,
                                 const fl::string& text,
                                 const fl::string& content_type = "text/plain");

/// Simple HTTP POST request with JSON
/// @param url the URL to post to
/// @param json the JSON data to send
/// @returns response or empty optional on error
fl::optional<Response> http_post_json(const fl::string& url, const fl::string& json);

/// Simple HTTP PUT request
fl::optional<Response> http_put(const fl::string& url, 
                                fl::span<const fl::u8> data,
                                const fl::string& content_type = "application/octet-stream");

/// Simple HTTP DELETE request
fl::optional<Response> http_delete(const fl::string& url);

} // namespace fl
```

### **2. HTTP Client Class (Level 2)**

For configurable HTTP clients with session management:

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
    explicit HttpClient(fl::shared_ptr<const Transport> transport, const Config& config = {});
    
    /// Destructor
    ~HttpClient();
    
    // ========== SIMPLE REQUEST METHODS ==========
    
    /// HTTP GET request
    /// @param url the URL to request
    /// @returns response or empty optional on error
    fl::optional<Response> get(const fl::string& url);
    
    /// HTTP POST request with data
    fl::optional<Response> post(const fl::string& url, fl::span<const fl::u8> data,
                               const fl::string& content_type = "application/octet-stream");
    
    /// HTTP POST request with text
    fl::optional<Response> post(const fl::string& url, const fl::string& text,
                               const fl::string& content_type = "text/plain");
    
    /// HTTP PUT request
    fl::optional<Response> put(const fl::string& url, fl::span<const fl::u8> data,
                              const fl::string& content_type = "application/octet-stream");
    
    /// HTTP DELETE request  
    fl::optional<Response> delete_(const fl::string& url);  // delete is keyword
    
    /// HTTP HEAD request
    fl::optional<Response> head(const fl::string& url);
    
    /// HTTP OPTIONS request
    fl::optional<Response> options(const fl::string& url);
    
    /// HTTP PATCH request
    fl::optional<Response> patch(const fl::string& url, fl::span<const fl::u8> data,
                                const fl::string& content_type = "application/octet-stream");
    
    // ========== ADVANCED REQUEST METHODS ==========
    
    /// Send custom request
    fl::optional<Response> send(const Request& request);
    
    /// Send request asynchronously  
    void send_async(const Request& request, fl::function<void(fl::optional<Response>)> callback);
    
    /// Download file to local storage
    bool download_file(const fl::string& url, const fl::string& local_path);
    
    /// Upload file from local storage
    fl::optional<Response> upload_file(const fl::string& url, const fl::string& file_path,
                                      const fl::string& field_name = "file");
    
    /// Stream download with custom processor
    bool stream_download(const fl::string& url, 
                        fl::function<bool(fl::span<const fl::u8>)> data_processor);
    
    /// Stream upload with custom provider
    fl::optional<Response> stream_upload(const fl::string& url,
                                        fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider,
                                        const fl::string& content_type = "application/octet-stream");
    
    // ========== CONFIGURATION ==========
    
    /// Set request timeout
    void set_timeout(fl::u32 timeout_ms);
    fl::u32 get_timeout() const;
    
    /// Set connection timeout
    void set_connect_timeout(fl::u32 timeout_ms);
    fl::u32 get_connect_timeout() const;
    
    /// Configure redirect handling
    void set_follow_redirects(bool follow);
    void set_max_redirects(fl::size max_redirects);
    bool get_follow_redirects() const;
    fl::size get_max_redirects() const;
    
    /// Set user agent
    void set_user_agent(const fl::string& user_agent);
    const fl::string& get_user_agent() const;
    
    /// Header management
    void set_header(const fl::string& name, const fl::string& value);
    void set_headers(const fl::vector<fl::pair<fl::string, fl::string>>& headers);
    void remove_header(const fl::string& name);
    void clear_headers();
    fl::span<const fl::pair<fl::string, fl::string>> get_headers() const;
    
    /// SSL/TLS configuration
    void set_verify_ssl(bool verify);
    void set_ca_bundle_path(const fl::string& path);
    void set_client_certificate(const fl::string& cert_path, const fl::string& key_path);
    bool get_verify_ssl() const;
    
    /// Response size limits
    void set_max_response_size(fl::size max_size);
    fl::size get_max_response_size() const;
    
    /// Buffer configuration
    void set_buffer_size(fl::size buffer_size);
    fl::size get_buffer_size() const;
    
    /// Compression settings
    void set_enable_compression(bool enable);
    bool get_enable_compression() const;
    
    /// Keep-alive settings
    void set_enable_keepalive(bool enable);
    void set_keepalive_timeout(fl::u32 timeout_ms);
    bool get_enable_keepalive() const;
    fl::u32 get_keepalive_timeout() const;
    
    // ========== AUTHENTICATION ==========
    
    /// Set basic authentication
    void set_basic_auth(const fl::string& username, const fl::string& password);
    
    /// Set bearer token authentication
    void set_bearer_token(const fl::string& token);
    
    /// Set API key authentication
    void set_api_key(const fl::string& key, const fl::string& header_name = "X-API-Key");
    
    /// Set custom authorization header
    void set_authorization(const fl::string& auth_header);
    
    /// Clear authentication
    void clear_auth();
    
    // ========== SESSION MANAGEMENT ==========
    
    /// Cookie management
    void set_cookie(const fl::string& name, const fl::string& value);
    void set_cookies(const fl::vector<fl::pair<fl::string, fl::string>>& cookies);
    void clear_cookies();
    fl::span<const fl::pair<fl::string, fl::string>> get_cookies() const;
    
    /// Session persistence
    void enable_cookie_jar(bool enable = true);
    bool is_cookie_jar_enabled() const;
    
    // ========== REQUEST BUILDING ==========
    
    /// Create request builder for advanced requests
    RequestBuilder request(const fl::string& method, const fl::string& url);
    RequestBuilder get_request(const fl::string& url);
    RequestBuilder post_request(const fl::string& url);
    RequestBuilder put_request(const fl::string& url);
    RequestBuilder delete_request(const fl::string& url);
    RequestBuilder head_request(const fl::string& url);
    RequestBuilder options_request(const fl::string& url);
    RequestBuilder patch_request(const fl::string& url);
    
    // ========== STATISTICS ==========
    
    /// Client statistics
    struct Stats {
        fl::size total_requests;
        fl::size successful_requests;
        fl::size failed_requests;
        fl::size redirects_followed;
        fl::size bytes_sent;
        fl::size bytes_received;
        fl::u32 average_response_time_ms;
        fl::u32 last_request_time_ms;
    };
    Stats get_stats() const;
    void reset_stats();
    
    /// Connection statistics
    fl::size get_active_connections() const;
    fl::size get_total_connections() const;
    
    // ========== ERROR HANDLING ==========
    
    /// Get last error information
    enum class ErrorCode {
        SUCCESS,
        NETWORK_ERROR,
        TIMEOUT,
        SSL_ERROR,
        INVALID_URL,
        INVALID_RESPONSE,
        TOO_MANY_REDIRECTS,
        RESPONSE_TOO_LARGE,
        UNKNOWN_ERROR
    };
    
    ErrorCode get_last_error() const;
    fl::string get_last_error_message() const;
    
    /// Error handling callback
    using ErrorHandler = fl::function<void(ErrorCode, const fl::string&)>;
    void set_error_handler(ErrorHandler handler);
    
    // ========== FACTORY METHODS ==========
    
    /// Create client with specific transport type
    static fl::shared_ptr<HttpClient> create_with_tcp_transport(const TcpTransport::Options& options = {});
    static fl::shared_ptr<HttpClient> create_with_tls_transport(const TlsTransport::Options& options);
    static fl::shared_ptr<HttpClient> create_with_transport(fl::shared_ptr<const Transport> transport);
    
    /// Create clients for common use cases
    static fl::shared_ptr<HttpClient> create_simple_client();
    static fl::shared_ptr<HttpClient> create_secure_client(bool verify_ssl = true);
    static fl::shared_ptr<HttpClient> create_api_client(const fl::string& base_url, const fl::string& api_key);
    
private:
    fl::shared_ptr<const Transport> mTransport;
    Config mConfig;
    fl::vector<fl::pair<fl::string, fl::string>> mDefaultHeaders;
    fl::vector<fl::pair<fl::string, fl::string>> mCookies;
    bool mCookieJarEnabled = false;
    
    // Authentication state
    fl::optional<fl::string> mAuthHeader;
    
    // Statistics
    mutable fl::mutex mStatsMutex;
    Stats mStats;
    
    // Error handling
    ErrorCode mLastError = ErrorCode::SUCCESS;
    fl::string mLastErrorMessage;
    ErrorHandler mErrorHandler;
    
    // Internal implementation
    fl::optional<Response> send_internal(const Request& request);
    Request build_request(const fl::string& method, const fl::string& url) const;
    void apply_config_to_request(Request& request) const;
    void apply_authentication(Request& request) const;
    void apply_cookies(Request& request) const;
    void process_response_cookies(const Response& response);
    fl::optional<Response> handle_redirects(const Request& request, const Response& response);
    void update_stats(const Request& request, const fl::optional<Response>& response, fl::u32 duration_ms);
    void set_error(ErrorCode error, const fl::string& message);
    
    // Friends
    friend class RequestBuilder;
};

} // namespace fl
```

### **3. Request Builder (Level 3)**

For advanced request construction:

```cpp
namespace fl {

/// Request builder for constructing complex HTTP requests
class RequestBuilder {
public:
    /// Create request builder
    RequestBuilder(const fl::string& method, const fl::string& url, HttpClient* client = nullptr);
    
    /// HTTP method and URL
    RequestBuilder& method(const fl::string& method);
    RequestBuilder& url(const fl::string& url);
    
    /// Headers
    RequestBuilder& header(const fl::string& name, const fl::string& value);
    RequestBuilder& headers(const fl::vector<fl::pair<fl::string, fl::string>>& headers);
    RequestBuilder& user_agent(const fl::string& agent);
    RequestBuilder& content_type(const fl::string& type);
    RequestBuilder& accept(const fl::string& types);
    RequestBuilder& authorization(const fl::string& auth);
    
    /// Query parameters
    RequestBuilder& query(const fl::string& name, const fl::string& value);
    RequestBuilder& query(const fl::vector<fl::pair<fl::string, fl::string>>& params);
    
    /// Request body
    RequestBuilder& body(fl::span<const fl::u8> data);
    RequestBuilder& body(const fl::string& text);
    RequestBuilder& json(const fl::string& json_data);
    RequestBuilder& form_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields);
    RequestBuilder& multipart_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields);
    RequestBuilder& file_upload(const fl::string& field_name, const fl::string& file_path);
    
    /// Authentication shortcuts
    RequestBuilder& basic_auth(const fl::string& username, const fl::string& password);
    RequestBuilder& bearer_token(const fl::string& token);
    RequestBuilder& api_key(const fl::string& key, const fl::string& header_name = "X-API-Key");
    
    /// Request options
    RequestBuilder& timeout(fl::u32 timeout_ms);
    RequestBuilder& follow_redirects(bool follow = true);
    RequestBuilder& max_redirects(fl::size max_redirects);
    RequestBuilder& verify_ssl(bool verify = true);
    
    /// Build and send request
    Request build() const;
    fl::optional<Response> send();
    void send_async(fl::function<void(fl::optional<Response>)> callback);
    
    /// Streaming operations
    bool download_to_file(const fl::string& file_path);
    bool stream_download(fl::function<bool(fl::span<const fl::u8>)> data_processor);
    fl::optional<Response> stream_upload(fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider);
    
private:
    HttpClient* mClient;
    fl::string mMethod;
    fl::string mUrl;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::vector<fl::pair<fl::string, fl::string>> mQueryParams;
    fl::vector<fl::u8> mBody;
    fl::optional<fl::string> mBodyContentType;
    fl::u32 mTimeout = 0;  // 0 = use client default
    fl::optional<bool> mFollowRedirects;
    fl::optional<fl::size> mMaxRedirects;
    fl::optional<bool> mVerifySSL;
    
    // Internal helpers
    fl::string build_query_string() const;
    fl::string build_form_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields) const;
    fl::string build_multipart_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields) const;
    fl::string encode_basic_auth(const fl::string& username, const fl::string& password) const;
    fl::string url_encode(const fl::string& str) const;
};

} // namespace fl
```

## Transport Abstraction

### **1. Transport Interface**

```cpp
namespace fl {

/// HTTP transport interface for different networking backends
class Transport {
public:
    virtual ~Transport() = default;
    
    /// Send HTTP request and receive response
    virtual fl::optional<Response> send_request(const Request& request) = 0;
    
    /// Send request asynchronously
    virtual void send_request_async(const Request& request, 
                                   fl::function<void(fl::optional<Response>)> callback) = 0;
    
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
};

} // namespace fl
```

### **2. TCP Transport Implementation**

```cpp
namespace fl {

/// TCP/HTTP transport using socket abstraction
class TcpTransport : public Transport {
public:
    /// TCP transport configuration
    struct Options {
        IpVersion ip_version = IpVersion::AUTO;
        fl::u32 connect_timeout_ms = 5000;
        fl::u32 read_timeout_ms = 10000;
        fl::u32 write_timeout_ms = 10000;
        fl::size buffer_size = 8192;
        bool enable_keepalive = true;
        fl::u32 keepalive_timeout_ms = 30000;
        bool enable_nodelay = true;
        fl::size max_connections_per_host = 5;
        fl::size max_total_connections = 50;
        bool enable_connection_pooling = true;
        
        /// Generate hash for transport caching
        fl::size hash() const;
        bool operator==(const Options& other) const;
    };
    
    explicit TcpTransport(const Options& options = {});
    ~TcpTransport() override;
    
    // Transport interface
    fl::optional<Response> send_request(const Request& request) override;
    void send_request_async(const Request& request, 
                           fl::function<void(fl::optional<Response>)> callback) override;
    
    bool supports_scheme(const fl::string& scheme) const override;
    bool supports_streaming() const override;
    bool supports_keepalive() const override;
    bool supports_compression() const override;
    bool supports_ssl() const override;
    
    fl::size get_active_connections() const override;
    void close_all_connections() override;
    
    fl::string get_transport_name() const override;
    fl::string get_transport_version() const override;
    
    /// TCP-specific configuration
    const Options& get_options() const { return mOptions; }
    
private:
    const Options mOptions;
    fl::unique_ptr<ConnectionPool> mConnectionPool;
    
    // HTTP protocol implementation
    fl::optional<Response> send_http_request(fl::shared_ptr<Socket> socket, const Request& request);
    bool send_http_headers(Socket* socket, const Request& request);
    bool send_http_body(Socket* socket, const Request& request);
    fl::optional<Response> receive_http_response(Socket* socket);
    Response parse_http_response(const fl::string& response_text);
    
    // Connection management
    fl::shared_ptr<Socket> get_connection(const fl::string& host, int port);
    void return_connection(fl::shared_ptr<Socket> socket, const fl::string& host, int port);
    
    // URL parsing
    struct UrlComponents {
        fl::string scheme;
        fl::string host;
        int port;
        fl::string path;
        fl::string query;
        fl::string fragment;
    };
    UrlComponents parse_url(const fl::string& url);
    
    // HTTP utility functions
    fl::string build_http_request_line(const Request& request, const UrlComponents& url);
    fl::string build_http_headers(const Request& request, const UrlComponents& url);
    fl::string format_header_value(const fl::string& name, const fl::string& value);
};

} // namespace fl
```

### **3. TLS Transport Implementation**

```cpp
namespace fl {

/// TLS/HTTPS transport with SSL/TLS support
class TlsTransport : public Transport {
public:
    /// TLS transport configuration
    struct Options {
        TcpTransport::Options tcp_options;               ///< Base TCP options
        bool verify_certificate = true;                 ///< Verify server certificates
        fl::string ca_bundle_path;                       ///< CA bundle file path
        fl::string client_cert_path;                     ///< Client certificate path
        fl::string client_key_path;                      ///< Client private key path
        fl::string client_key_password;                  ///< Client key password
        fl::vector<fl::string> supported_protocols;      ///< ALPN protocols
        fl::string cipher_suites;                        ///< Allowed cipher suites
        bool enable_sni = true;                          ///< Server Name Indication
        fl::u32 handshake_timeout_ms = 10000;           ///< TLS handshake timeout
        
        /// Generate hash for transport caching
        fl::size hash() const;
        bool operator==(const Options& other) const;
    };
    
    explicit TlsTransport(const Options& options);
    ~TlsTransport() override;
    
    // Transport interface
    fl::optional<Response> send_request(const Request& request) override;
    void send_request_async(const Request& request, 
                           fl::function<void(fl::optional<Response>)> callback) override;
    
    bool supports_scheme(const fl::string& scheme) const override;
    bool supports_streaming() const override;
    bool supports_keepalive() const override;
    bool supports_compression() const override;
    bool supports_ssl() const override;
    
    fl::size get_active_connections() const override;
    void close_all_connections() override;
    
    fl::string get_transport_name() const override;
    fl::string get_transport_version() const override;
    
    /// TLS-specific configuration
    const Options& get_options() const { return mOptions; }
    
    /// Certificate information
    struct CertificateInfo {
        fl::string subject;
        fl::string issuer;
        fl::string serial_number;
        fl::string not_before;
        fl::string not_after;
        fl::vector<fl::string> san_dns_names;
        fl::vector<fl::string> san_ip_addresses;
        bool is_valid;
        bool is_expired;
        bool is_self_signed;
    };
    fl::optional<CertificateInfo> get_peer_certificate_info(const fl::string& host, int port);
    
private:
    const Options mOptions;
    fl::unique_ptr<TcpTransport> mTcpTransport;
    
    // TLS implementation (platform-specific)
    struct TlsContext;
    fl::unique_ptr<TlsContext> mTlsContext;
    
    // TLS connection management
    fl::shared_ptr<Socket> create_tls_connection(const fl::string& host, int port);
    bool perform_tls_handshake(Socket* socket, const fl::string& host);
    bool verify_peer_certificate(const fl::string& host);
    
    // Platform-specific TLS implementation
    void initialize_tls_context();
    void cleanup_tls_context();
    bool load_ca_bundle();
    bool load_client_certificate();
    
    // Certificate validation
    bool validate_certificate_chain(const fl::string& host);
    bool check_certificate_hostname(const fl::string& host);
    bool check_certificate_expiry();
};

} // namespace fl
```

## Usage Examples

### **1. Simple API Requests**

```cpp
#include "fl/networking/http_client.h"

void fetch_weather_data() {
    // Simple GET request
    auto response = fl::http_get("http://api.weather.com/current");
    
    if (response && response->status_code() == 200) {
        fl::string weather_json = response->body_text();
        FL_WARN("Weather data: " << weather_json);
        
        // Parse and use weather data
        update_weather_display(weather_json);
    } else {
        FL_WARN("Failed to fetch weather data");
    }
}

void send_sensor_data() {
    // Simple POST request with JSON
    fl::string sensor_json = "{\"temperature\": 25.5, \"humidity\": 60.2}";
    auto response = fl::http_post_json("http://api.iot.com/sensors", sensor_json);
    
    if (response && response->status_code() == 201) {
        FL_WARN("Sensor data sent successfully");
    } else {
        FL_WARN("Failed to send sensor data");
    }
}
```

### **2. Configured HTTP Client**

```cpp
#include "fl/networking/http_client.h"

class WeatherService {
private:
    fl::HttpClient mClient;
    
public:
    void initialize() {
        // Configure HTTP client
        mClient.set_timeout(15000);
        mClient.set_user_agent("FastLED-Weather/1.0");
        mClient.set_header("Accept", "application/json");
        mClient.set_api_key("your_api_key_here", "X-API-Key");
        
        // Enable keep-alive for efficiency
        mClient.set_enable_keepalive(true);
        mClient.set_keepalive_timeout(60000);
    }
    
    void update_weather() {
        auto response = mClient.get("http://api.weather.com/current?location=Seattle");
        
        if (response && response->status_code() == 200) {
            process_weather_data(response->body_text());
        } else {
            FL_WARN("Weather update failed: " << mClient.get_last_error_message());
        }
    }
    
    void send_device_status() {
        fl::string status_json = build_device_status_json();
        auto response = mClient.post("http://api.iot.com/devices/status", 
                                    status_json, "application/json");
        
        if (response && response->status_code() == 200) {
            FL_WARN("Device status sent successfully");
        }
    }
    
private:
    fl::string build_device_status_json() {
        return fl::string("{") +
            "\"device_id\": \"fastled_001\", " +
            "\"uptime\": " + fl::to_string(millis()) + ", " +
            "\"led_count\": 100, " +
            "\"current_pattern\": \"rainbow\"" +
            "}";
    }
    
    void process_weather_data(const fl::string& json) {
        // Parse weather JSON and update LED display
        // Implementation depends on JSON library availability
        FL_WARN("Processing weather data: " << json);
    }
};
```

### **3. Advanced Request Building**

```cpp
#include "fl/networking/http_client.h"

class APIService {
private:
    fl::shared_ptr<fl::HttpClient> mClient;
    fl::string mBaseUrl;
    fl::string mAuthToken;
    
public:
    void initialize(const fl::string& base_url, const fl::string& auth_token) {
        mBaseUrl = base_url;
        mAuthToken = auth_token;
        
        // Create secure HTTPS client
        mClient = fl::HttpClient::create_secure_client(true);
        mClient->set_timeout(30000);
        mClient->set_user_agent("FastLED-IoT/1.0");
        mClient->set_bearer_token(auth_token);
    }
    
    void upload_led_pattern() {
        // Build complex multipart request
        auto response = mClient->post_request(mBaseUrl + "/patterns")
            .header("Content-Type", "multipart/form-data")
            .multipart_data({
                {"pattern_name", "rainbow_wave"},
                {"duration_ms", "5000"},
                {"led_count", "100"}
            })
            .file_upload("pattern_data", "/spiffs/patterns/rainbow_wave.json")
            .timeout(60000)  // Long timeout for file upload
            .send();
        
        if (response && response->status_code() == 201) {
            FL_WARN("Pattern uploaded successfully");
        } else {
            FL_WARN("Pattern upload failed");
        }
    }
    
    void stream_sensor_data() {
        // Stream large sensor data file
        bool success = mClient->post_request(mBaseUrl + "/sensors/bulk")
            .header("Content-Type", "application/octet-stream")
            .stream_upload([this]() -> fl::optional<fl::span<const fl::u8>> {
                return read_next_sensor_chunk();
            });
        
        if (success) {
            FL_WARN("Sensor data streamed successfully");
        }
    }
    
    void download_firmware_update() {
        // Stream download large firmware file
        bool success = mClient->get_request(mBaseUrl + "/firmware/latest")
            .stream_download([](fl::span<const fl::u8> chunk) -> bool {
                return write_firmware_chunk(chunk);
            });
        
        if (success) {
            FL_WARN("Firmware download completed");
        }
    }
    
private:
    fl::optional<fl::span<const fl::u8>> read_next_sensor_chunk() {
        // Read next chunk of sensor data
        // Return empty optional when done
        static int chunk_count = 0;
        chunk_count++;
        
        if (chunk_count <= 10) {
            static fl::string data = "sensor_data_chunk_" + fl::to_string(chunk_count);
            return fl::span<const fl::u8>(
                reinterpret_cast<const fl::u8*>(data.data()), 
                data.size()
            );
        }
        
        return fl::nullopt;  // End of data
    }
    
    static bool write_firmware_chunk(fl::span<const fl::u8> chunk) {
        // Write firmware chunk to flash
        FL_WARN("Writing firmware chunk: " << chunk.size() << " bytes");
        return true;  // Continue download
    }
};
```

### **4. Async HTTP Client**

```cpp
#include "fl/networking/http_client.h"

class AsyncWeatherService {
private:
    fl::shared_ptr<fl::HttpClient> mClient;
    fl::vector<fl::string> mCityList;
    fl::size mCurrentCityIndex = 0;
    
public:
    void initialize() {
        mClient = fl::HttpClient::create_simple_client();
        mClient->set_timeout(10000);
        
        mCityList = {"Seattle", "Portland", "San Francisco", "Los Angeles"};
    }
    
    void start_weather_updates() {
        update_next_city();
    }
    
private:
    void update_next_city() {
        if (mCurrentCityIndex >= mCityList.size()) {
            mCurrentCityIndex = 0;  // Start over
        }
        
        fl::string city = mCityList[mCurrentCityIndex];
        fl::string url = "http://api.weather.com/current?location=" + city;
        
        // Send async request
        mClient->get_request(url)
            .send_async([this, city](fl::optional<fl::Response> response) {
                this->handle_weather_response(city, response);
            });
        
        mCurrentCityIndex++;
    }
    
    void handle_weather_response(const fl::string& city, fl::optional<fl::Response> response) {
        if (response && response->status_code() == 200) {
            fl::string weather_data = response->body_text();
            FL_WARN("Weather for " << city << ": " << weather_data);
            
            // Update LED display for this city
            update_city_weather_display(city, weather_data);
        } else {
            FL_WARN("Failed to get weather for " << city);
        }
        
        // Continue with next city after 5 seconds
        schedule_next_update();
    }
    
    void schedule_next_update() {
        // In real implementation, use a timer or EngineEvents
        // For demo, just continue immediately
        EVERY_N_SECONDS(5) {
            update_next_city();
        }
    }
    
    void update_city_weather_display(const fl::string& city, const fl::string& weather_data) {
        // Parse weather and update LEDs
        // Implementation specific to weather data format
        FL_WARN("Updating display for " << city);
    }
};
```

### **5. HTTPS Client with Authentication**

```cpp
#include "fl/networking/http_client.h"

class SecureAPIClient {
private:
    fl::shared_ptr<fl::HttpClient> mClient;
    fl::string mApiEndpoint;
    fl::string mClientCertPath;
    fl::string mClientKeyPath;
    
public:
    void initialize(const fl::string& api_endpoint, 
                   const fl::string& cert_path, 
                   const fl::string& key_path) {
        mApiEndpoint = api_endpoint;
        mClientCertPath = cert_path;
        mClientKeyPath = key_path;
        
        // Create TLS transport with client certificates
        fl::TlsTransport::Options tls_options;
        tls_options.verify_certificate = true;
        tls_options.client_cert_path = cert_path;
        tls_options.client_key_path = key_path;
        tls_options.ca_bundle_path = "/spiffs/ca-bundle.pem";
        
        auto transport = fl::make_shared<fl::TlsTransport>(tls_options);
        mClient = fl::make_shared<fl::HttpClient>(transport);
        
        // Configure secure client
        mClient->set_timeout(20000);
        mClient->set_user_agent("FastLED-Secure/1.0");
        mClient->set_header("Accept", "application/json");
    }
    
    void authenticate() {
        // Authenticate with client certificate
        auto response = mClient->post_request(mApiEndpoint + "/auth/certificate")
            .header("X-Device-ID", "fastled_device_001")
            .json("{\"device_type\": \"led_controller\"}")
            .send();
        
        if (response && response->status_code() == 200) {
            // Extract and store session token
            fl::string auth_response = response->body_text();
            extract_session_token(auth_response);
            FL_WARN("Authentication successful");
        } else {
            FL_WARN("Authentication failed");
        }
    }
    
    void send_encrypted_data() {
        fl::string encrypted_data = encrypt_sensor_data();
        
        auto response = mClient->post_request(mApiEndpoint + "/data/secure")
            .header("Content-Type", "application/encrypted")
            .header("X-Encryption-Method", "AES-256-GCM")
            .body(encrypted_data)
            .timeout(30000)
            .send();
        
        if (response && response->status_code() == 200) {
            FL_WARN("Encrypted data sent successfully");
        } else {
            FL_WARN("Failed to send encrypted data");
        }
    }
    
    void download_secure_firmware() {
        auto response = mClient->get_request(mApiEndpoint + "/firmware/secure/latest")
            .header("X-Firmware-Version", "1.0.0")
            .timeout(120000)  // 2 minute timeout for large download
            .send();
        
        if (response && response->status_code() == 200) {
            // Verify firmware signature
            if (verify_firmware_signature(response->body_text())) {
                install_firmware_update(response->body_text());
            } else {
                FL_WARN("Firmware signature verification failed");
            }
        }
    }
    
private:
    void extract_session_token(const fl::string& auth_response) {
        // Parse JSON response and extract session token
        // Set as bearer token for future requests
        fl::string token = parse_json_token(auth_response);
        mClient->set_bearer_token(token);
    }
    
    fl::string encrypt_sensor_data() {
        // Encrypt sensor data before transmission
        fl::string raw_data = get_sensor_data();
        return encrypt_aes_256_gcm(raw_data);
    }
    
    fl::string get_sensor_data() {
        return "{\"temperature\": 25.5, \"humidity\": 60.2, \"timestamp\": " + 
               fl::to_string(millis()) + "}";
    }
    
    fl::string parse_json_token(const fl::string& json) {
        // Simple JSON token extraction (production would use proper JSON parser)
        return "extracted_token_here";
    }
    
    fl::string encrypt_aes_256_gcm(const fl::string& data) {
        // Encrypt data (implementation depends on crypto library)
        return "encrypted_" + data;
    }
    
    bool verify_firmware_signature(const fl::string& firmware_data) {
        // Verify firmware digital signature
        return true;  // Simplified for demo
    }
    
    void install_firmware_update(const fl::string& firmware_data) {
        FL_WARN("Installing firmware update");
        // Install firmware (platform-specific implementation)
    }
};
```

## Integration with FastLED

### **EngineEvents Integration**

HTTP client operations integrate seamlessly with FastLED's event system:

```cpp
class NetworkManager : public EngineEvents::Listener {
private:
    fl::shared_ptr<fl::HttpClient> mClient;
    
public:
    void initialize() {
        mClient = fl::HttpClient::create_simple_client();
        EngineEvents::addListener(this);
    }
    
    void onEndFrame() override {
        // Process network operations during FastLED.show()
        // This ensures non-blocking operation
        process_pending_requests();
        cleanup_completed_requests();
    }
    
private:
    void process_pending_requests() {
        // Handle async HTTP operations
        // Update connection pools
        // Process callbacks
    }
    
    void cleanup_completed_requests() {
        // Clean up completed async requests
        // Free unused connections
    }
};
```

The HTTP client design provides comprehensive client functionality while maintaining FastLED's principles of simplicity, memory efficiency, and embedded-system compatibility. 
