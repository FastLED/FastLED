# FastLED HTTP Server Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

This document defines the HTTP server API for FastLED, providing route-based request handling, middleware system, WebSocket support, and static file serving with security. Designed for IoT device control, web interfaces, and API endpoints.

## Design Goals

- **Progressive Complexity**: Simple defaults that scale to advanced features
- **Memory Efficient**: Use `fl::deque` with PSRAM support for request/response bodies
- **Transport Abstraction**: Pluggable server transports (TCP, TLS, local-only)
- **FastLED Integration**: Non-blocking operation compatible with LED updates
- **Dual Handler API**: Both function callbacks and class-based handlers
- **Streaming Support**: Efficient handling of large uploads/downloads
- **Security Built-in**: Input validation, authentication, TLS support

## Core Server API

### **1. Simple HTTP Server (Level 1)**

For basic HTTP endpoints with minimal setup:

```cpp
namespace fl {

/// Simple HTTP server factory functions
fl::shared_ptr<HttpServer> create_local_server();
fl::shared_ptr<HttpServer> create_https_server(const fl::string& cert_path, const fl::string& key_path);
fl::shared_ptr<HttpServer> create_development_server();

/// Quick server setup for common use cases
void serve_health_check(int port = 8080);
void serve_device_control(int port = 8080);
void serve_static_files(const fl::string& directory, int port = 80);

} // namespace fl
```

### **2. HTTP Server Class (Level 2)**

Main HTTP server with route handling and middleware:

```cpp
namespace fl {

/// HTTP request method enumeration
enum class HttpMethod {
    GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, TRACE, CONNECT
};

/// Convert between HttpMethod and strings
HttpMethod string_to_http_method(const fl::string& method);
const char* http_method_to_string(HttpMethod method);

/// Route handler function types
using RouteHandler = fl::function<Response(const Request&)>;
using AsyncRouteHandler = fl::function<void(const Request&, fl::function<void(Response)>)>;

/// Main HTTP server class
class HttpServer : public EngineEvents::Listener {
public:
    /// Create HTTP server with default TCP transport
    HttpServer();
    
    /// Create HTTP server with shared const transport
    explicit HttpServer(fl::shared_ptr<const ServerTransport> transport);
    
    /// Destructor
    ~HttpServer();
    
    // ========== FACTORY METHODS ==========
    
    /// Create local development server (localhost only, HTTP)
    static fl::shared_ptr<HttpServer> create_local_server();
    
    /// Create HTTPS server with certificates
    static fl::shared_ptr<HttpServer> create_https_server(const fl::string& cert_path, 
                                                          const fl::string& key_path);
    
    /// Create development server with mixed HTTP/HTTPS support
    static fl::shared_ptr<HttpServer> create_development_server();
    
    /// Create server with custom transport configuration
    static fl::shared_ptr<HttpServer> create_with_transport(fl::shared_ptr<const ServerTransport> transport);
    
    // ========== SERVER LIFECYCLE ==========
    
    /// Start listening on specified port
    bool listen(int port, const char* bind_address = nullptr);
    
    /// Stop the server and close all connections
    void stop();
    
    /// Check if server is currently listening
    bool is_listening() const;
    
    /// Get bound port (useful when port 0 is used)
    int get_port() const;
    fl::string get_address() const;
    
    // ========== ROUTE REGISTRATION (PROGRESSIVE COMPLEXITY) ==========
    
    /// Simple route registration (most common usage)
    void get(const fl::string& pattern, RouteHandler handler);
    void post(const fl::string& pattern, RouteHandler handler);
    void put(const fl::string& pattern, RouteHandler handler);
    void delete_(const fl::string& pattern, RouteHandler handler);  // delete is keyword
    void patch(const fl::string& pattern, RouteHandler handler);
    void head(const fl::string& pattern, RouteHandler handler);
    void options(const fl::string& pattern, RouteHandler handler);
    
    /// Async route registration (for non-blocking handlers)
    void get_async(const fl::string& pattern, AsyncRouteHandler handler);
    void post_async(const fl::string& pattern, AsyncRouteHandler handler);
    void put_async(const fl::string& pattern, AsyncRouteHandler handler);
    void delete_async(const fl::string& pattern, AsyncRouteHandler handler);
    void patch_async(const fl::string& pattern, AsyncRouteHandler handler);
    void head_async(const fl::string& pattern, AsyncRouteHandler handler);
    void options_async(const fl::string& pattern, AsyncRouteHandler handler);
    
    /// Generic route registration
    void route(HttpMethod method, const fl::string& pattern, RouteHandler handler);
    void route_async(HttpMethod method, const fl::string& pattern, AsyncRouteHandler handler);
    
    // ========== MIDDLEWARE SUPPORT ==========
    
    /// Middleware function type
    using Middleware = fl::function<bool(const Request&, ResponseBuilder&)>;
    
    /// Add middleware that runs before all routes
    void use(Middleware middleware);
    
    /// Add middleware for specific path prefix
    void use(const fl::string& path_prefix, Middleware middleware);
    
    /// Built-in middleware
    void use_cors(const fl::string& origin = "*", 
                  const fl::string& methods = "GET, POST, PUT, DELETE, OPTIONS",
                  const fl::string& headers = "Content-Type, Authorization");
    void use_request_logging();
    void use_json_parser();
    void use_form_parser();
    void use_static_files(const fl::string& mount_path, const fl::string& file_directory);
    
    // ========== ERROR HANDLING ==========
    
    /// Error handler function type
    using ErrorHandler = fl::function<Response(const Request&, int status_code, const fl::string& message)>;
    
    /// Set custom error handler
    void on_error(ErrorHandler handler);
    
    /// Set 404 handler
    void on_not_found(RouteHandler handler);
    
    // ========== WEBSOCKET SUPPORT ==========
    
    /// WebSocket upgrade handler
    using WebSocketHandler = fl::function<void(const Request&, WebSocketConnection*)>;
    
    /// Add WebSocket endpoint
    void websocket(const fl::string& pattern, WebSocketHandler handler);
    
    // ========== SERVER STATISTICS ==========
    
    /// Server statistics
    struct ServerStats {
        fl::size active_connections;
        fl::size total_connections_accepted;
        fl::size total_requests_handled;
        fl::size current_request_queue_size;
        fl::size middleware_executions;
        fl::size route_matches;
        fl::size not_found_responses;
        fl::size error_responses;
        fl::u32 average_request_duration_ms;
        fl::u32 server_uptime_ms;
        fl::string transport_type;
    };
    ServerStats get_stats() const;
    
    /// Get transport-specific statistics
    TcpServerTransport::ServerStats get_transport_stats() const;
    
    // ========== CONFIGURATION ==========
    
    /// Server configuration options
    struct Config {
        fl::size max_request_size = 1048576;     ///< Maximum request size (1MB)
        fl::size max_header_size = 8192;         ///< Maximum header size (8KB)
        fl::u32 request_timeout_ms = 10000;      ///< Request processing timeout
        fl::u32 keep_alive_timeout_ms = 30000;   ///< Keep-alive timeout
        bool enable_compression = false;         ///< Enable gzip compression
        bool enable_request_logging = false;     ///< Enable request logging
        fl::string static_file_directory;        ///< Directory for static files
        fl::string default_content_type = "text/plain"; ///< Default content type
    };
    
    /// Set server configuration
    void configure(const Config& config);
    const Config& get_config() const;
    
private:
    fl::shared_ptr<const ServerTransport> mTransport;
    fl::vector<Route> mRoutes;
    fl::vector<fl::pair<fl::string, Middleware>> mMiddlewares;
    ErrorHandler mErrorHandler;
    RouteHandler mNotFoundHandler;
    Config mConfig;
    
    // Statistics
    mutable fl::mutex mStatsMutex;
    fl::size mTotalRequestsHandled = 0;
    fl::size mMiddlewareExecutions = 0;
    fl::size mRouteMatches = 0;
    fl::size mNotFoundResponses = 0;
    fl::size mErrorResponses = 0;
    fl::u32 mServerStartTime = 0;
    
    // EngineEvents integration
    void onEndFrame() override;
    
    // Request processing (internal)
    void process_pending_connections();
    void handle_request(fl::shared_ptr<Socket> connection);
    Request parse_request(Socket* connection);
    Response dispatch_request(const Request& request);
    Route* find_matching_route(const Request& request);
    bool execute_middlewares(const Request& request, ResponseBuilder& response);
    void send_response(Socket* connection, const Response& response);
    Response handle_error(const Request& request, int status_code, const fl::string& message);
    
    // Route matching
    bool matches_pattern(const fl::string& pattern, const fl::string& path, 
                        fl::vector<fl::pair<fl::string, fl::string>>& path_params);
    
    // Internal helpers
    void initialize_default_handlers();
    void register_route(HttpMethod method, const fl::string& pattern, RouteHandler handler);
    void register_async_route(HttpMethod method, const fl::string& pattern, AsyncRouteHandler handler);
};

} // namespace fl
```

## Request/Response Implementation

### **1. HTTP Request Class**

```cpp
namespace fl {

/// HTTP request object with memory-efficient fl::deque storage
class Request {
public:
    /// Request metadata
    HttpMethod method() const;
    const fl::string& url() const;
    const fl::string& path() const;
    const fl::string& query_string() const;
    const fl::string& protocol_version() const;
    
    /// Request headers
    const fl::string& header(const fl::string& name) const;
    const fl::string& header(const char* name) const;
    bool has_header(const fl::string& name) const;
    bool has_header(const char* name) const;
    fl::span<const fl::pair<fl::string, fl::string>> headers() const;
    
    /// Request body access
    fl::string body_text() const;                    ///< Get body as text
    const char* body_c_str() const;                  ///< Legacy C-style access
    fl::size body_size() const;                      ///< Body size in bytes
    bool has_body() const;                           ///< Check if body exists
    
    /// Iterator-based access (most memory efficient)
    using const_iterator = RequestBody::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
    
    /// Chunk-based processing (ideal for large request bodies)
    bool process_body_chunks(fl::size chunk_size, 
                            fl::function<bool(fl::span<const fl::u8>)> processor) const;
    
    /// Query parameter access
    const fl::string& query_param(const fl::string& name) const;
    const fl::string& query_param(const char* name) const;
    bool has_query_param(const fl::string& name) const;
    fl::span<const fl::pair<fl::string, fl::string>> query_params() const;
    
    /// Path parameter access (set by route matching)
    const fl::string& path_param(const fl::string& name) const;
    const fl::string& path_param(const char* name) const;
    bool has_path_param(const fl::string& name) const;
    fl::span<const fl::pair<fl::string, fl::string>> path_params() const;
    
    /// Form data access
    const fl::string& form_data(const fl::string& name) const;
    const fl::string& form_data(const char* name) const;
    bool has_form_data(const fl::string& name) const;
    fl::span<const fl::pair<fl::string, fl::string>> form_data_all() const;
    
    /// JSON access
    bool is_json() const;
    fl::optional<JsonDocument> json() const;
    
    /// Utility methods
    bool is_valid() const;
    bool is_multipart() const;
    bool is_chunked() const;
    bool expects_continue() const;
    
    /// Connection info
    fl::string remote_address() const;
    int remote_port() const;
    bool is_secure() const;
    
    /// Memory info
    bool uses_psram() const;
    fl::size memory_footprint() const;
    
private:
    // Implementation details
    HttpMethod mMethod = HttpMethod::GET;
    fl::string mUrl, mPath, mQueryString, mProtocolVersion;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::string mRemoteAddress;
    int mRemotePort = 0;
    bool mIsSecure = false;
    
    RequestBody mBody;  // Hybrid storage with small buffer optimization
    
    // Parsed data (lazy-loaded)
    mutable fl::optional<fl::vector<fl::pair<fl::string, fl::string>>> mQueryParams;
    mutable fl::optional<fl::vector<fl::pair<fl::string, fl::string>>> mFormData;
    mutable fl::vector<fl::pair<fl::string, fl::string>> mPathParams;
    
    friend class HttpServer;
    friend class RequestParser;
};

} // namespace fl
```

### **2. HTTP Response Builder**

```cpp
namespace fl {

/// HTTP response builder with streaming support
class ResponseBuilder {
public:
    /// Response status
    ResponseBuilder& status(int status_code);
    ResponseBuilder& status(int status_code, const fl::string& status_text);
    
    /// Response headers
    ResponseBuilder& header(const fl::string& name, const fl::string& value);
    ResponseBuilder& header(const char* name, const char* value);
    ResponseBuilder& content_type(const fl::string& type);
    ResponseBuilder& content_length(fl::size length);
    
    /// Convenience headers
    ResponseBuilder& cors_allow_origin(const fl::string& origin = "*");
    ResponseBuilder& cors_allow_methods(const fl::string& methods = "GET, POST, PUT, DELETE, OPTIONS");
    ResponseBuilder& cors_allow_headers(const fl::string& headers = "Content-Type, Authorization");
    ResponseBuilder& cache_control(const fl::string& directives);
    ResponseBuilder& no_cache();
    ResponseBuilder& redirect(const fl::string& location, int status_code = 302);
    
    /// Response body (simple cases)
    ResponseBuilder& body(const fl::string& content);
    ResponseBuilder& body(const char* content);
    ResponseBuilder& body(fl::span<const fl::u8> data);
    ResponseBuilder& json(const fl::string& json_content);
    ResponseBuilder& html(const fl::string& html_content);
    ResponseBuilder& text(const fl::string& text_content);
    
    /// File response
    ResponseBuilder& file(const fl::string& file_path);
    ResponseBuilder& file(const fl::string& file_path, const fl::string& content_type);
    
    /// Streaming response
    ResponseBuilder& stream(fl::function<bool(fl::span<fl::u8>)> data_provider);
    ResponseBuilder& stream_chunked(fl::function<fl::optional<fl::string>()> chunk_provider);
    
    /// Build the response
    Response build() &&;     // Move semantics for efficiency
    Response build() const;  // Copy version for backwards compatibility
    
    /// Quick response builders (static methods)
    static Response ok(const fl::string& content = "");
    static Response json_response(const fl::string& json_content);
    static Response html_response(const fl::string& html_content);
    static Response text_response(const fl::string& text_content);
    static Response not_found(const fl::string& message = "Not Found");
    static Response bad_request(const fl::string& message = "Bad Request");
    static Response internal_error(const fl::string& message = "Internal Server Error");
    static Response method_not_allowed(const fl::string& message = "Method Not Allowed");
    static Response unauthorized(const fl::string& message = "Unauthorized");
    static Response forbidden(const fl::string& message = "Forbidden");
    static Response redirect_response(const fl::string& location, int status_code = 302);
    
private:
    int mStatusCode = 200;
    fl::string mStatusText = "OK";
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    ResponseBody mBody;  // Consistent fl::deque storage
    
    // Streaming options
    fl::optional<fl::string> mFilePath;
    fl::optional<fl::function<bool(fl::span<fl::u8>)>> mStreamProvider;
    fl::optional<fl::function<fl::optional<fl::string>()>> mChunkProvider;
    bool mIsStreaming = false;
    bool mIsChunked = false;
};

/// HTTP response with consistent fl::deque storage
class Response {
public:
    /// Response metadata
    int status_code() const;
    const fl::string& status_text() const;
    const fl::string& protocol_version() const;
    
    /// Response headers
    const fl::string& header(const fl::string& name) const;
    bool has_header(const fl::string& name) const;
    fl::span<const fl::pair<fl::string, fl::string>> headers() const;
    
    /// Response body
    fl::string body_text() const;
    const char* body_c_str() const;
    fl::size body_size() const;
    bool has_body() const;
    
    /// Iterator access
    using const_iterator = ResponseBody::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
    
    /// Direct body access
    const ResponseBody& body() const;
    
    /// Memory info
    bool uses_psram() const;
    fl::size memory_footprint() const;
    
    /// Quick response builders (static methods)
    static Response ok(const fl::string& content = "");
    static Response json(const fl::string& json_content);
    static Response html(const fl::string& html_content);
    static Response text(const fl::string& text_content);
    static Response not_found(const fl::string& message = "Not Found");
    static Response bad_request(const fl::string& message = "Bad Request");
    static Response internal_error(const fl::string& message = "Internal Server Error");
    static Response unauthorized(const fl::string& message = "Unauthorized");
    static Response redirect(const fl::string& location, int status_code = 302);
    
private:
    int mStatusCode = 200;
    fl::string mStatusText = "OK";
    fl::string mProtocolVersion = "HTTP/1.1";
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    ResponseBody mBody;  // Consistent fl::deque storage
    
    friend class HttpServer;
    friend class ResponseBuilder;
};

} // namespace fl
```

## Server Transport Abstraction

### **1. Server Transport Interface**

```cpp
namespace fl {

/// Server transport configuration
enum class ServerTransportType {
    TCP_ONLY,       ///< HTTP only (no TLS)
    TLS_ONLY,       ///< HTTPS only (require TLS)
    MIXED           ///< Support both HTTP and HTTPS
};

/// Server connection limits and behavior
struct ServerLimits {
    fl::size max_concurrent_connections = 10;
    fl::size max_pending_connections = 5;
    fl::u32 connection_timeout_ms = 30000;
    fl::u32 request_timeout_ms = 10000;
    fl::u32 response_timeout_ms = 30000;
    fl::size max_request_size = 1048576;  // 1MB
    fl::size max_header_size = 8192;      // 8KB
};

/// Base server transport interface
class ServerTransport {
public:
    virtual ~ServerTransport() = default;
    
    /// Start listening on specified port
    virtual bool listen(int port, const char* bind_address = nullptr) = 0;
    
    /// Stop listening and close all connections
    virtual void stop() = 0;
    
    /// Check if server is currently listening
    virtual bool is_listening() const = 0;
    
    /// Accept pending connections (non-blocking)
    virtual fl::vector<fl::shared_ptr<Socket>> accept_connections() = 0;
    
    /// Server statistics
    virtual fl::size get_active_connections() const = 0;
    virtual fl::size get_total_connections_accepted() const = 0;
    virtual fl::size get_total_requests_handled() const = 0;
    
    /// Transport capabilities
    virtual bool supports_tls() const = 0;
    virtual bool supports_http2() const = 0;
    
    /// Server info
    virtual int get_bound_port() const = 0;
    virtual fl::string get_bound_address() const = 0;
    
protected:
    virtual fl::shared_ptr<ServerSocket> create_server_socket() const = 0;
    virtual bool configure_socket_options(ServerSocket* socket) const = 0;
};

} // namespace fl
```

### **2. TCP Server Transport**

```cpp
namespace fl {

/// Standard TCP server transport with IPv4/IPv6 support
class TcpServerTransport : public ServerTransport {
public:
    /// Server transport configuration options
    struct Options {
        IpVersion ip_version = IpVersion::AUTO;
        ServerTransportType transport_type = ServerTransportType::TCP_ONLY;
        ServerLimits limits;
        fl::optional<fl::string> bind_address;
        bool enable_keepalive = true;
        bool enable_nodelay = true;
        fl::size socket_buffer_size = 8192;
        
        // Advanced socket options
        bool enable_so_reuseaddr = true;
        bool enable_so_reuseport = false;  // Linux/BSD only
        fl::optional<fl::u32> so_rcvtimeo_ms;
        fl::optional<fl::u32> so_sndtimeo_ms;
        
        // TLS-specific options
        fl::string certificate_path;
        fl::string private_key_path;
        fl::string ca_bundle_path;
        bool require_client_certificates = false;
        fl::vector<fl::string> supported_protocols;  // ALPN protocols
        
        fl::size hash() const;
        bool operator==(const Options& other) const;
    };
    
    explicit TcpServerTransport(const Options& options = {});
    ~TcpServerTransport() override;
    
    // ServerTransport interface implementation
    bool listen(int port, const char* bind_address = nullptr) override;
    void stop() override;
    bool is_listening() const override;
    fl::vector<fl::shared_ptr<Socket>> accept_connections() override;
    fl::size get_active_connections() const override;
    fl::size get_total_connections_accepted() const override;
    fl::size get_total_requests_handled() const override;
    bool supports_tls() const override;
    bool supports_http2() const override;
    int get_bound_port() const override;
    fl::string get_bound_address() const override;
    
    /// Get current transport options
    const Options& get_options() const;
    
    /// Server statistics
    struct ServerStats {
        fl::size active_connections;
        fl::size total_connections_accepted;
        fl::size total_requests_handled;
        fl::size current_request_queue_size;
        fl::size rejected_connections;
        fl::u32 average_request_duration_ms;
        fl::u32 server_uptime_ms;
    };
    ServerStats get_server_stats() const;

protected:
    fl::shared_ptr<ServerSocket> create_server_socket() const override;
    bool configure_socket_options(ServerSocket* socket) const override;

private:
    const Options mOptions;
    fl::shared_ptr<ServerSocket> mServerSocket;
    fl::vector<fl::shared_ptr<Socket>> mActiveConnections;
    bool mIsListening = false;
    int mBoundPort = 0;
    fl::string mBoundAddress;
    
    // Statistics tracking
    mutable fl::mutex mStatsMutex;
    fl::size mTotalConnectionsAccepted = 0;
    fl::size mTotalRequestsHandled = 0;
    fl::size mRejectedConnections = 0;
    fl::u32 mServerStartTime = 0;
    
    // Internal management
    void initialize_server_socket();
    void cleanup_closed_connections();
    bool accept_single_connection();
    void configure_client_socket(Socket* socket) const;
    void set_socket_timeouts(Socket* socket) const;
    void configure_tls_if_enabled(Socket* socket) const;
};

} // namespace fl
```

### **3. Specialized Server Transports**

```cpp
namespace fl {

/// Local-only server transport (localhost only) - development/debugging
class LocalServerTransport : public TcpServerTransport {
public:
    LocalServerTransport() : TcpServerTransport(create_local_options()) {}
    
    bool listen(int port, const char* bind_address = nullptr) override {
        const char* local_bind = bind_address ? bind_address : "127.0.0.1";
        return TcpServerTransport::listen(port, local_bind);
    }
    
private:
    static Options create_local_options() {
        Options opts;
        opts.ip_version = IpVersion::AUTO;
        opts.transport_type = ServerTransportType::TCP_ONLY;
        opts.bind_address = "127.0.0.1";
        opts.limits.max_concurrent_connections = 20;
        opts.limits.connection_timeout_ms = 60000;
        return opts;
    }
};

/// Production HTTPS server transport with strict security
class HttpsServerTransport : public TcpServerTransport {
public:
    explicit HttpsServerTransport(const fl::string& cert_path, const fl::string& key_path) 
        : TcpServerTransport(create_https_options(cert_path, key_path)) {}
    
private:
    static Options create_https_options(const fl::string& cert_path, const fl::string& key_path) {
        Options opts;
        opts.transport_type = ServerTransportType::TLS_ONLY;
        opts.certificate_path = cert_path;
        opts.private_key_path = key_path;
        opts.limits.max_concurrent_connections = 50;
        opts.limits.connection_timeout_ms = 30000;
        opts.supported_protocols = {"http/1.1", "h2"};
        return opts;
    }
};

/// Development server transport with mixed HTTP/HTTPS
class DevelopmentServerTransport : public TcpServerTransport {
public:
    DevelopmentServerTransport() : TcpServerTransport(create_development_options()) {}
    
private:
    static Options create_development_options() {
        Options opts;
        opts.transport_type = ServerTransportType::MIXED;
        opts.limits.max_concurrent_connections = 10;
        opts.limits.connection_timeout_ms = 60000;
        opts.limits.request_timeout_ms = 30000;
        return opts;
    }
};

} // namespace fl
```

## Usage Examples

### **1. Simple Health Check Server**

```cpp
#include "fl/networking/http_server.h"

void setup() {
    // Create local development server (simplest setup)
    auto server = HttpServer::create_local_server();
    
    // Add health check endpoint
    server->get("/health", [](const Request& req) {
        return Response::ok("Server is healthy");
    });
    
    // Start listening on port 8080
    if (server->listen(8080)) {
        FL_WARN("Server listening on http://localhost:8080");
    } else {
        FL_WARN("Failed to start server");
    }
}

void loop() {
    // Server handles requests automatically via EngineEvents
    FastLED.show();
}
```

### **2. IoT Device Control Server**

```cpp
class IoTControlServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    CRGB mLeds[100];
    
public:
    void initialize() {
        mServer = HttpServer::create_local_server();
        
        // Device status endpoint
        mServer->get("/status", [this](const Request& req) {
            return Response::json(get_device_status_json());
        });
        
        // Sensor data endpoint
        mServer->get("/sensors", [this](const Request& req) {
            fl::string sensor_json = fl::string("{") +
                "\"temperature\": " + fl::to_string(get_temperature()) + ", " +
                "\"humidity\": " + fl::to_string(get_humidity()) + ", " +
                "\"light\": " + fl::to_string(get_light_level()) +
                "}";
            return Response::json(sensor_json);
        });
        
        // LED control endpoint
        mServer->post("/leds", [this](const Request& req) {
            if (req.is_json()) {
                auto json_data = req.json();
                if (json_data) {
                    set_leds_from_json(*json_data);
                    return Response::ok("LEDs updated");
                }
            }
            return Response::bad_request("Invalid JSON");
        });
        
        // LED color endpoint with path parameters
        mServer->put("/leds/{index}/color/{color}", [this](const Request& req) {
            int index = fl::stoi(req.path_param("index"));
            fl::string color = req.path_param("color");
            
            if (index >= 0 && index < 100) {
                set_led_color(index, parse_color(color));
                return Response::ok("LED color updated");
            }
            return Response::bad_request("Invalid LED index");
        });
        
        // Start server
        if (mServer->listen(8080)) {
            FL_WARN("IoT Control Server listening on http://localhost:8080");
        }
    }
    
private:
    fl::string get_device_status_json() const {
        return fl::string("{") +
            "\"uptime\": " + fl::to_string(millis()) + ", " +
            "\"free_memory\": " + fl::to_string(get_free_memory()) + ", " +
            "\"led_count\": 100, " +
            "\"current_pattern\": \"rainbow\"" +
            "}";
    }
    
    void set_leds_from_json(const JsonDocument& json) {
        // Parse JSON and update LEDs
    }
    
    void set_led_color(int index, CRGB color) {
        if (index >= 0 && index < 100) {
            mLeds[index] = color;
            FastLED.show();
        }
    }
    
    CRGB parse_color(const fl::string& color_str) {
        if (color_str == "red") return CRGB::Red;
        if (color_str == "green") return CRGB::Green;
        if (color_str == "blue") return CRGB::Blue;
        return CRGB::Black;
    }
    
    float get_temperature() const { return 25.5f; }
    float get_humidity() const { return 60.2f; }
    float get_light_level() const { return 75.0f; }
    fl::size get_free_memory() const { return 50000; }
};
```

### **3. API Server with Middleware**

```cpp
class APIServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    fl::string mApiKey = "fastled_api_key_123";
    
public:
    void initialize() {
        mServer = HttpServer::create_local_server();
        
        // Enable CORS for web frontend
        mServer->use_cors("*", "GET, POST, PUT, DELETE, OPTIONS", "Content-Type, Authorization");
        
        // Enable request logging
        mServer->use_request_logging();
        
        // API key authentication middleware
        mServer->use("/api", [this](const Request& req, ResponseBuilder& res) {
            if (req.header("Authorization") != "Bearer " + mApiKey) {
                res.status(401).json("{\"error\": \"Invalid API key\"}");
                return false;  // Stop processing
            }
            return true;  // Continue to routes
        });
        
        // API endpoints
        setup_user_routes();
        setup_data_routes();
        
        // Error handling
        mServer->on_error([](const Request& req, int status, const fl::string& message) {
            fl::string error_json = fl::string("{\"error\": \"") + message + 
                                   "\", \"status\": " + fl::to_string(status) + "}";
            return Response::json(error_json).status(status);
        });
        
        // 404 handler
        mServer->on_not_found([](const Request& req) {
            return Response::json("{\"error\": \"Endpoint not found\"}")
                   .status(404);
        });
        
        if (mServer->listen(8080)) {
            FL_WARN("API Server listening on http://localhost:8080");
        }
    }
    
private:
    void setup_user_routes() {
        // Get user info
        mServer->get("/api/users/{id}", [](const Request& req) {
            fl::string user_id = req.path_param("id");
            fl::string user_json = fl::string("{") +
                "\"id\": \"" + user_id + "\", " +
                "\"name\": \"FastLED User\", " +
                "\"email\": \"user@fastled.com\"" +
                "}";
            return Response::json(user_json);
        });
        
        // Create user
        mServer->post("/api/users", [](const Request& req) {
            if (!req.is_json()) {
                return Response::bad_request("JSON required");
            }
            
            fl::string body = req.body_text();
            FL_WARN("Creating user: " << body);
            
            return Response::json("{\"id\": \"new_user_123\", \"status\": \"created\"}")
                   .status(201);
        });
        
        // Update user
        mServer->put("/api/users/{id}", [](const Request& req) {
            fl::string user_id = req.path_param("id");
            fl::string body = req.body_text();
            
            FL_WARN("Updating user " << user_id << ": " << body);
            
            return Response::json("{\"status\": \"updated\"}");
        });
        
        // Delete user
        mServer->delete_("/api/users/{id}", [](const Request& req) {
            fl::string user_id = req.path_param("id");
            
            FL_WARN("Deleting user: " << user_id);
            
            return Response::json("{\"status\": \"deleted\"}");
        });
    }
    
    void setup_data_routes() {
        // Get sensor data with query parameters
        mServer->get("/api/data", [](const Request& req) {
            fl::string sensor_type = req.query_param("sensor");
            fl::string limit_str = req.query_param("limit");
            int limit = limit_str.empty() ? 100 : fl::stoi(limit_str);
            
            fl::string data_json = fl::string("{") +
                "\"sensor\": \"" + sensor_type + "\", " +
                "\"limit\": " + fl::to_string(limit) + ", " +
                "\"data\": [1, 2, 3, 4, 5]" +
                "}";
            
            return Response::json(data_json);
        });
        
        // Upload data (async processing)
        mServer->post_async("/api/data/upload", [](const Request& req, auto callback) {
            FL_WARN("Processing upload of " << req.body_size() << " bytes");
            
            // Simulate async processing
            callback(Response::json("{\"status\": \"upload_processed\"}"));
        });
        
        // Stream large dataset
        mServer->get("/api/data/stream", [](const Request& req) {
            return Response::stream_chunked([]() -> fl::optional<fl::string> {
                static int chunk_count = 0;
                chunk_count++;
                
                if (chunk_count <= 5) {
                    return fl::optional<fl::string>(
                        fl::string("{\"chunk\": ") + fl::to_string(chunk_count) + 
                        ", \"data\": [1, 2, 3, 4, 5]}\n"
                    );
                } else {
                    return fl::nullopt;  // End of stream
                }
            });
        });
    }
};
```

### **4. Static File Server with Web Interface**

```cpp
class WebInterfaceServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    
public:
    void initialize() {
        mServer = HttpServer::create_local_server();
        
        // Enable static file serving
        mServer->use_static_files("/", "/spiffs/www");
        
        // API endpoints for web interface
        mServer->get("/api/device-info", [](const Request& req) {
            fl::string device_info = fl::string("{") +
                "\"device_name\": \"FastLED Controller\", " +
                "\"version\": \"1.0.0\", " +
                "\"led_count\": 100, " +
                "\"wifi_ssid\": \"" + get_wifi_ssid() + "\", " +
                "\"ip_address\": \"" + get_ip_address() + "\"" +
                "}";
            return Response::json(device_info);
        });
        
        // LED pattern control
        mServer->post("/api/pattern", [](const Request& req) {
            if (req.has_form_data("pattern")) {
                fl::string pattern = req.form_data("pattern");
                apply_led_pattern(pattern);
                return Response::json("{\"status\": \"pattern_applied\"}");
            }
            return Response::bad_request("Pattern parameter required");
        });
        
        // File upload for new patterns
        mServer->post("/api/upload-pattern", [](const Request& req) {
            if (req.is_multipart()) {
                save_pattern_file(req);
                return Response::json("{\"status\": \"pattern_uploaded\"}");
            }
            return Response::bad_request("Multipart form data required");
        });
        
        // WebSocket for real-time LED updates
        mServer->websocket("/ws/leds", [](const Request& req, WebSocketConnection* ws) {
            ws->on_message([](const fl::string& message) {
                FL_WARN("WebSocket message: " << message);
                // Parse and apply real-time LED updates
            });
            
            ws->on_close([]() {
                FL_WARN("WebSocket connection closed");
            });
        });
        
        // Serve main page
        mServer->get("/", [](const Request& req) {
            return Response::file("/spiffs/www/index.html", "text/html");
        });
        
        if (mServer->listen(80)) {
            FL_WARN("Web Interface available at http://" << get_ip_address());
        }
    }
    
private:
    fl::string get_wifi_ssid() const { return "FastLED_Network"; }
    fl::string get_ip_address() const { return "192.168.1.100"; }
    
    void apply_led_pattern(const fl::string& pattern) {
        FL_WARN("Applying LED pattern: " << pattern);
    }
    
    void save_pattern_file(const Request& req) {
        FL_WARN("Saving pattern file");
    }
};
```

### **5. HTTPS Server with Authentication**

```cpp
class SecureAPIServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    fl::unordered_map<fl::string, fl::string> mUsers;
    fl::unordered_map<fl::string, fl::u32> mSessions;
    
public:
    void initialize() {
        // Create HTTPS server with certificates
        mServer = HttpServer::create_https_server("/spiffs/server.crt", "/spiffs/server.key");
        
        // Initialize users
        mUsers["admin"] = hash_password("admin123");
        mUsers["user"] = hash_password("user123");
        
        // CORS for secure web frontend
        mServer->use_cors("https://localhost:3000", "GET, POST, PUT, DELETE, OPTIONS", 
                         "Content-Type, Authorization");
        
        // Request logging with security info
        mServer->use([](const Request& req, ResponseBuilder& res) {
            FL_WARN("Request: " << http_method_to_string(req.method()) << " " << req.path() 
                    << " from " << req.remote_address() 
                    << " (TLS: " << (req.is_secure() ? "YES" : "NO") << ")");
            return true;
        });
        
        // Authentication routes
        setup_auth_routes();
        
        // Protected API routes
        mServer->use("/api", [this](const Request& req, ResponseBuilder& res) {
            return authenticate_request(req, res);
        });
        setup_protected_routes();
        
        if (mServer->listen(8443)) {
            FL_WARN("Secure API Server listening on https://localhost:8443");
        }
    }
    
private:
    void setup_auth_routes() {
        // Login endpoint
        mServer->post("/login", [this](const Request& req) {
            if (!req.has_form_data("username") || !req.has_form_data("password")) {
                return Response::bad_request("Username and password required");
            }
            
            fl::string username = req.form_data("username");
            fl::string password = req.form_data("password");
            
            if (verify_credentials(username, password)) {
                fl::string session_token = generate_session_token();
                mSessions[session_token] = millis() + 3600000;  // 1 hour expiry
                
                fl::string response_json = fl::string("{") +
                    "\"status\": \"success\", " +
                    "\"session_token\": \"" + session_token + "\", " +
                    "\"expires_in\": 3600" +
                    "}";
                
                return Response::json(response_json)
                       .header("Set-Cookie", "session=" + session_token + "; HttpOnly; Secure");
            } else {
                return Response::unauthorized("Invalid credentials");
            }
        });
        
        // Logout endpoint
        mServer->post("/logout", [this](const Request& req) {
            fl::string auth_header = req.header("Authorization");
            if (auth_header.starts_with("Bearer ")) {
                fl::string token = auth_header.substr(7);
                mSessions.erase(token);
            }
            return Response::ok("Logged out");
        });
    }
    
    void setup_protected_routes() {
        // User profile
        mServer->get("/api/profile", [](const Request& req) {
            return Response::json("{\"username\": \"user\", \"role\": \"standard\"}");
        });
        
        // LED control (authenticated users only)
        mServer->post("/api/leds/pattern", [](const Request& req) {
            fl::string pattern = req.form_data("pattern");
            apply_secure_led_pattern(pattern);
            return Response::json("{\"status\": \"pattern_applied_securely\"}");
        });
        
        // Secure data upload
        mServer->post("/api/upload", [](const Request& req) {
            if (req.body_size() > 100000) {
                return Response::bad_request("File too large");
            }
            
            save_secure_data(req.body_text());
            return Response::json("{\"status\": \"uploaded_securely\"}");
        });
    }
    
    bool authenticate_request(const Request& req, ResponseBuilder& res) {
        fl::string auth_header = req.header("Authorization");
        if (!auth_header.starts_with("Bearer ")) {
            res.status(401).json("{\"error\": \"Authentication required\"}");
            return false;
        }
        
        fl::string token = auth_header.substr(7);
        auto it = mSessions.find(token);
        if (it == mSessions.end() || it->second < millis()) {
            res.status(401).json("{\"error\": \"Invalid or expired session\"}");
            return false;
        }
        
        return true;
    }
    
    bool verify_credentials(const fl::string& username, const fl::string& password) {
        auto it = mUsers.find(username);
        return it != mUsers.end() && it->second == hash_password(password);
    }
    
    fl::string hash_password(const fl::string& password) {
        // Simple hash for demo - use proper crypto in production
        fl::size hash = 0;
        for (char c : password) {
            hash = hash * 31 + c;
        }
        return fl::to_string(hash);
    }
    
    fl::string generate_session_token() {
        return "token_" + fl::to_string(millis()) + "_" + fl::to_string(random(10000));
    }
    
    void apply_secure_led_pattern(const fl::string& pattern) {
        FL_WARN("Applying secure LED pattern: " << pattern);
    }
    
    void save_secure_data(const fl::string& data) {
        FL_WARN("Saving secure data: " << data.substr(0, 50) << "...");
    }
};
```

## Integration with FastLED

### **EngineEvents Integration**

The HTTP server integrates seamlessly with FastLED's event system for non-blocking operation:

```cpp
void HttpServer::onEndFrame() {
    // Process network events during FastLED.show()
    process_pending_connections();
    
    // Handle incoming requests
    for (auto& connection : mActiveConnections) {
        if (connection->has_data_available()) {
            handle_request(connection);
        }
    }
    
    // Clean up closed connections
    cleanup_closed_connections();
    
    // Update statistics
    update_server_stats();
}
```

### **Memory Management**

The server uses FastLED's memory management primitives for efficient operation on embedded systems:

- **fl::deque with PSRAM allocator** for request/response bodies
- **Small buffer optimization** for typical HTTP requests
- **Copy-on-write semantics** for headers and shared data
- **Streaming support** to avoid memory spikes

### **Non-Blocking Operation**

All server operations are designed to be non-blocking:

- **Request processing** happens during `FastLED.show()`
- **Connection management** uses non-blocking sockets
- **File serving** uses chunked streaming
- **WebSocket messages** are queued and processed incrementally

The HTTP server design provides comprehensive server functionality while maintaining FastLED's core principles of simplicity, performance, and embedded-system compatibility. 
