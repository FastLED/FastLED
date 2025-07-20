# FastLED HttpServer API Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

| Component | Status | Notes |
|-----------|--------|-------|
| **HttpServer** | ✅ **DESIGNED** | ✅ Complete API design with transport abstraction |
| **Request** | ✅ **DESIGNED** | ✅ Memory-efficient design uses `fl::deque` for body storage |
| **Response** | ✅ **DESIGNED** | ✅ Streaming response support with PSRAM optimization |
| **RouteHandler** | ✅ **DESIGNED** | ✅ Function-based and class-based handler support |
| **ServerSocket** | ✅ **DESIGNED** | ✅ BSD-style server socket wrapper design complete |
| **TLS/HTTPS** | ✅ **DESIGNED** | ✅ Certificate management, TLS configuration |
| **Middleware** | ✅ **DESIGNED** | ✅ CORS, authentication, logging middleware |
| **Static Files** | ✅ **DESIGNED** | ✅ Efficient static file serving with caching |

**✅ INFRASTRUCTURE READY:**
- `fl::deque` - ✅ **Implemented** - Memory-efficient request/response storage
- `fl::string` - ✅ **Implemented** - Copy-on-write semantics  
- `fl::span` - ✅ **Implemented** - Safe data access
- `fl::vector`, `fl::shared_ptr`, `fl::mutex` - ✅ **Implemented**
- `fl::future<T>` - ✅ **Implemented** - From CONCURRENCY_FUTURE_API_DESIGN.md

**🎯 NEXT STEPS:**
1. Implement the `Request` class with `fl::deque` storage
2. Create the `ServerSocket` abstraction for listening
3. Implement basic HTTP server with route handling
4. Add TLS/HTTPS support for secure connections
5. Create middleware system for extensibility

**📝 RECENT DESIGN DECISIONS:**
- ✅ **Progressive complexity API** - simple defaults that scale to advanced features
- ✅ **Memory-efficient design** - `fl::deque` for request/response bodies with PSRAM support
- ✅ **Transport abstraction** - pluggable server transports (TCP, TLS, local-only)
- ✅ **Dual handler API** - both function callbacks and class-based handlers
- ✅ **Streaming support** - efficient handling of large uploads/downloads
- ✅ **FastLED integration** - works seamlessly with EngineEvents and FastLED patterns

## Overview

This document outlines the design for a FastLED HTTP server API that provides BSD-style server socket functionality under the `fl::` namespace. The API is designed to work across multiple platforms including native systems, ESP32 (via lwIP), and WebAssembly/Emscripten (via WebSocket/WebRTC).

## Design Goals

- **Consistent API**: Uniform interface across all supported platforms
- **FastLED Style**: Follow FastLED coding conventions and patterns  
- **Platform Abstraction**: Hide platform-specific implementation details
- **Type Safety**: Leverage C++ type system for compile-time safety
- **RAII**: Automatic resource management for sockets and connections
- **Header-Only**: Minimize build complexity where possible
- **Embedded-Friendly**: Work well on resource-constrained devices
- **Memory Efficient**: Use `fl::deque` to avoid request/response memory fragmentation
- **Progressive Complexity**: Simple defaults that scale to advanced features
- **Shared Ownership**: Use `fl::shared_ptr` for components that may be shared across the application

# HIGH-LEVEL HTTP SERVER API

## Overview

The high-level HTTP server API provides HTTP server functionality with both callback and class-based handler interfaces. It's designed to be non-blocking and integrate seamlessly with FastLED's event system.

## Core Components

### 1. Server Transport Abstraction

The FastLED HTTP server uses a pluggable transport system similar to the HTTP client, allowing fine-grained control over networking behavior including IPv4/IPv6 preferences, connection limits, and TLS configuration.

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
    fl::size max_concurrent_connections = 10;     ///< Maximum simultaneous connections
    fl::size max_pending_connections = 5;         ///< Maximum pending connections (listen backlog)
    fl::u32 connection_timeout_ms = 30000;        ///< Keep connection alive for 30s
    fl::u32 request_timeout_ms = 10000;           ///< Timeout for receiving complete request
    fl::u32 response_timeout_ms = 30000;          ///< Timeout for sending response
    fl::size max_request_size = 1048576;          ///< Maximum request size (1MB default)
    fl::size max_header_size = 8192;              ///< Maximum header size (8KB default)
};

/// Base server transport interface for accepting connections
class ServerTransport {
public:
    virtual ~ServerTransport() = default;
    
    /// Start listening on specified port
    /// @param port the port number to listen on
    /// @param bind_address the IP address to bind to (nullptr for all interfaces)
    /// @returns true if listening started successfully
    virtual bool listen(int port, const char* bind_address = nullptr) = 0;
    
    /// Stop listening and close all connections
    virtual void stop() = 0;
    
    /// Check if server is currently listening
    virtual bool is_listening() const = 0;
    
    /// Accept pending connections (non-blocking)
    /// @returns vector of new connections, empty if none available
    virtual fl::vector<fl::shared_ptr<Socket>> accept_connections() = 0;
    
    /// Get server statistics
    virtual fl::size get_active_connections() const = 0;
    virtual fl::size get_total_connections_accepted() const = 0;
    virtual fl::size get_total_requests_handled() const = 0;
    
    /// Check if transport supports the given features
    virtual bool supports_tls() const = 0;
    virtual bool supports_http2() const = 0;
    
    /// Get bound port (useful when port 0 is used for auto-assignment)
    virtual int get_bound_port() const = 0;
    virtual fl::string get_bound_address() const = 0;
    
protected:
    // Platform-specific server socket creation
    virtual fl::shared_ptr<ServerSocket> create_server_socket() const = 0;
    virtual bool configure_socket_options(ServerSocket* socket) const = 0;
};

/// Standard TCP server transport with IPv4/IPv6 support
class TcpServerTransport : public ServerTransport {
public:
    /// Server transport configuration options
    struct Options {
        IpVersion ip_version = IpVersion::AUTO;      ///< IPv4/IPv6 preference
        ServerTransportType transport_type = ServerTransportType::TCP_ONLY;
        ServerLimits limits;                         ///< Connection and resource limits
        fl::optional<fl::string> bind_address;       ///< Specific IP to bind to
        bool enable_keepalive = true;                ///< Enable HTTP keep-alive
        bool enable_nodelay = true;                  ///< Disable Nagle's algorithm (TCP_NODELAY)
        fl::size socket_buffer_size = 8192;          ///< Socket send/receive buffer size
        
        // Advanced socket options
        bool enable_so_reuseaddr = true;             ///< Allow address reuse
        bool enable_so_reuseport = false;            ///< Allow port reuse (Linux/BSD)
        fl::optional<fl::u32> so_rcvtimeo_ms;        ///< Socket receive timeout
        fl::optional<fl::u32> so_sndtimeo_ms;        ///< Socket send timeout
        
        // TLS-specific options (when transport_type includes TLS)
        fl::string certificate_path;                 ///< Path to server certificate
        fl::string private_key_path;                 ///< Path to private key
        fl::string ca_bundle_path;                   ///< Path to client CA bundle (for client auth)
        bool require_client_certificates = false;   ///< Require client certificate authentication
        fl::vector<fl::string> supported_protocols;  ///< ALPN protocols ("http/1.1", "h2")
        
        /// Generate hash for configuration comparison
        fl::size hash() const {
            fl::size h = 0;
            h ^= fl::hash<int>{}(static_cast<int>(ip_version));
            h ^= fl::hash<int>{}(static_cast<int>(transport_type));
            h ^= fl::hash<fl::size>{}(limits.max_concurrent_connections);
            h ^= fl::hash<fl::u32>{}(limits.connection_timeout_ms);
            h ^= fl::hash<bool>{}(enable_keepalive);
            h ^= fl::hash<bool>{}(enable_nodelay);
            h ^= fl::hash<fl::size>{}(socket_buffer_size);
            if (bind_address) {
                h ^= fl::hash<fl::string>{}(*bind_address);
            }
            h ^= fl::hash<fl::string>{}(certificate_path);
            h ^= fl::hash<fl::string>{}(private_key_path);
            return h;
        }
        
        /// Compare options for equality
        bool operator==(const Options& other) const {
            return ip_version == other.ip_version &&
                   transport_type == other.transport_type &&
                   limits.max_concurrent_connections == other.limits.max_concurrent_connections &&
                   limits.connection_timeout_ms == other.limits.connection_timeout_ms &&
                   enable_keepalive == other.enable_keepalive &&
                   enable_nodelay == other.enable_nodelay &&
                   socket_buffer_size == other.socket_buffer_size &&
                   bind_address == other.bind_address &&
                   certificate_path == other.certificate_path &&
                   private_key_path == other.private_key_path;
        }
    };
    
    /// Create server transport with specific options
    explicit TcpServerTransport(const Options& options = {}) : mOptions(options) {
        initialize_server_socket();
    }
    
    /// Destructor
    ~TcpServerTransport() override {
        stop();
    }
    
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
    const Options& get_options() const { return mOptions; }
    
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
    
    // Server socket management
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
    
    /// Initialize server socket (called once during construction)
    void initialize_server_socket();
    
    /// Connection management
    void cleanup_closed_connections();
    bool accept_single_connection();
    void configure_client_socket(Socket* socket) const;
    void set_socket_timeouts(Socket* socket) const;
    void set_socket_options(Socket* socket) const;
    void configure_tls_if_enabled(Socket* socket) const;
};

/// Specialized server transports for specific use cases

/// Local-only server transport (localhost, 127.0.0.1, ::1) - development/debugging
class LocalServerTransport : public TcpServerTransport {
public:
    LocalServerTransport() : TcpServerTransport(create_local_options()) {}
    
    bool listen(int port, const char* bind_address = nullptr) override {
        // Force localhost binding
        const char* local_bind = bind_address ? bind_address : "127.0.0.1";
        return TcpServerTransport::listen(port, local_bind);
    }
    
private:
    static Options create_local_options() {
        Options opts;
        opts.ip_version = IpVersion::AUTO;
        opts.transport_type = ServerTransportType::TCP_ONLY;
        opts.bind_address = "127.0.0.1";  // Localhost only
        opts.limits.max_concurrent_connections = 20;  // Allow more local connections
        opts.limits.connection_timeout_ms = 60000;    // Longer timeout for dev
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
        opts.transport_type = ServerTransportType::TLS_ONLY;  // HTTPS only
        opts.certificate_path = cert_path;
        opts.private_key_path = key_path;
        opts.limits.max_concurrent_connections = 50;  // Production capacity
        opts.limits.connection_timeout_ms = 30000;    // Standard timeout
        opts.supported_protocols = {"http/1.1", "h2"}; // HTTP/2 support
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
        opts.transport_type = ServerTransportType::MIXED;  // Both HTTP and HTTPS
        opts.limits.max_concurrent_connections = 10;       // Dev limits
        opts.limits.connection_timeout_ms = 60000;         // Longer for debugging
        opts.limits.request_timeout_ms = 30000;           // Longer for debugging
        return opts;
    }
};

} // namespace fl
```

### 2. HTTP Server with Route Handling

```cpp
namespace fl {

/// HTTP request method enumeration
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS,
    TRACE,
    CONNECT
};

/// Convert string to HttpMethod
HttpMethod string_to_http_method(const fl::string& method);
HttpMethod string_to_http_method(const char* method);

/// Convert HttpMethod to string
const char* http_method_to_string(HttpMethod method);

/// HTTP request object with memory-efficient fl::deque storage
class Request {
public:
    /// Request metadata
    HttpMethod method() const { return mMethod; }
    const fl::string& url() const { return mUrl; }
    const fl::string& path() const { return mPath; }
    const fl::string& query_string() const { return mQueryString; }
    const fl::string& protocol_version() const { return mProtocolVersion; }
    
    /// Request headers
    const fl::string& header(const fl::string& name) const;
    const fl::string& header(const char* name) const;
    bool has_header(const fl::string& name) const;
    bool has_header(const char* name) const;
    
    /// Get all headers as key-value pairs
    fl::span<const fl::pair<fl::string, fl::string>> headers() const;
    
    /// Request body size and properties
    fl::size content_length() const;
    const fl::string& content_type() const;
    fl::size body_size() const { return mBody.size(); }
    bool has_body() const { return !mBody.empty(); }
    
    /// Memory management info
    bool uses_psram() const { return mUsesPsram; }
    fl::size memory_footprint() const;
    
    // ========== PRIMARY INTERFACES ==========
    
    /// Body text access (copy-on-write fl::string for efficient sharing)
    /// @note Creates fl::string from request body - efficient for text content
    /// @note Uses copy-on-write semantics - multiple copies share same buffer
    fl::string body_text() const;
    
    /// Legacy C-style body access
    /// @note Returns pointer to internal buffer - valid until Request destroyed
    const char* body_c_str() const;
    fl::size body_text_length() const;
    
    /// Iterator-based access (most memory efficient)
    /// @note Direct iteration over request body - zero allocation
    using const_iterator = RequestBody::const_iterator;
    const_iterator begin() const { return mBody.begin(); }
    const_iterator end() const { return mBody.end(); }
    const_iterator cbegin() const { return mBody.cbegin(); }
    const_iterator cend() const { return mBody.cend(); }
    
    /// Chunk-based processing (ideal for large request bodies)
    /// @param chunk_size size of each chunk to process
    /// @param processor function called for each chunk
    /// @returns true if all chunks processed, false if processor returned false
    bool process_body_chunks(fl::size chunk_size, 
                            fl::function<bool(fl::span<const fl::u8>)> processor) const;
    
    // ========== QUERY PARAMETER ACCESS ==========
    
    /// Parse and access query parameters
    /// @note Lazy parsing - only parsed when first accessed
    const fl::string& query_param(const fl::string& name) const;
    const fl::string& query_param(const char* name) const;
    bool has_query_param(const fl::string& name) const;
    bool has_query_param(const char* name) const;
    
    /// Get all query parameters
    fl::span<const fl::pair<fl::string, fl::string>> query_params() const;
    
    // ========== PATH PARAMETER ACCESS ==========
    
    /// Access path parameters (set by route matching)
    /// @note Path parameters are extracted from URL patterns like "/user/{id}/posts/{post_id}"
    const fl::string& path_param(const fl::string& name) const;
    const fl::string& path_param(const char* name) const;
    bool has_path_param(const fl::string& name) const;
    bool has_path_param(const char* name) const;
    
    /// Get all path parameters
    fl::span<const fl::pair<fl::string, fl::string>> path_params() const;
    
    // ========== FORM DATA ACCESS ==========
    
    /// Access form data (application/x-www-form-urlencoded or multipart/form-data)
    /// @note Lazy parsing - only parsed when first accessed
    const fl::string& form_data(const fl::string& name) const;
    const fl::string& form_data(const char* name) const;
    bool has_form_data(const fl::string& name) const;
    bool has_form_data(const char* name) const;
    
    /// Get all form data
    fl::span<const fl::pair<fl::string, fl::string>> form_data_all() const;
    
    // ========== JSON ACCESS ==========
    
    /// Check if request contains JSON
    bool is_json() const;
    
    /// Parse request body as JSON (if JSON library is available)
    /// @note Returns empty optional if parsing fails or no JSON library
    /// @note Implementation depends on available JSON library (ArduinoJson, etc.)
    fl::optional<JsonDocument> json() const;
    
    // ========== UTILITY METHODS ==========
    
    /// Request validation
    bool is_valid() const;
    bool is_multipart() const;
    bool is_chunked() const;
    bool expects_continue() const;  // Expect: 100-continue header
    
    /// Connection info
    fl::string remote_address() const { return mRemoteAddress; }
    int remote_port() const { return mRemotePort; }
    bool is_secure() const { return mIsSecure; }  // HTTPS connection
    
    /// Storage optimization info
    bool is_body_inlined() const { return mBody.is_inlined(); }
    bool is_body_deque_storage() const { return mBody.is_deque(); }
    
    // ========== CONSTRUCTION (Internal Use) ==========
    
    /// Create empty request
    Request() = default;
    
    /// Create request (used internally by HttpServer)
    static Request create(HttpMethod method,
                         const fl::string& url,
                         const fl::string& protocol_version,
                         fl::vector<fl::pair<fl::string, fl::string>> headers,
                         RequestBody body,
                         const fl::string& remote_address,
                         int remote_port,
                         bool is_secure);
    
private:
    // Request metadata
    HttpMethod mMethod = HttpMethod::GET;
    fl::string mUrl;
    fl::string mPath;
    fl::string mQueryString;
    fl::string mProtocolVersion;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::string mRemoteAddress;
    int mRemotePort = 0;
    bool mIsSecure = false;
    
    // Request body - hybrid storage with small buffer optimization
    RequestBody mBody;
    bool mUsesPsram = false;
    
    // Parsed data (lazy-loaded)
    mutable fl::optional<fl::vector<fl::pair<fl::string, fl::string>>> mQueryParams;
    mutable fl::optional<fl::vector<fl::pair<fl::string, fl::string>>> mFormData;
    mutable fl::vector<fl::pair<fl::string, fl::string>> mPathParams;
    
    // Header lookup optimization
    mutable fl::unordered_map<fl::string, fl::string> mHeaderMap;
    mutable bool mHeaderMapBuilt = false;
    
    // Internal helpers
    void build_header_map() const;
    void parse_query_string() const;
    void parse_form_data() const;
    void parse_url_components();
    
    // Friends for internal construction
    friend class HttpServer;
    friend class RequestParser;
};

/// HTTP response builder with streaming support
class ResponseBuilder {
public:
    /// Response status
    ResponseBuilder& status(int status_code);
    ResponseBuilder& status(int status_code, const fl::string& status_text);
    ResponseBuilder& status(int status_code, const char* status_text);
    
    /// Response headers
    ResponseBuilder& header(const fl::string& name, const fl::string& value);
    ResponseBuilder& header(const char* name, const char* value);
    ResponseBuilder& header(const char* name, const fl::string& value);
    ResponseBuilder& content_type(const fl::string& type);
    ResponseBuilder& content_type(const char* type);
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
    
    /// Build the response (internal use)
    Response build() const;
    
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
    fl::string mBodyContent;
    fl::optional<fl::string> mFilePath;
    fl::optional<fl::function<bool(fl::span<fl::u8>)>> mStreamProvider;
    fl::optional<fl::function<fl::optional<fl::string>()>> mChunkProvider;
    bool mIsStreaming = false;
    bool mIsChunked = false;
};

/// Route handler function type
using RouteHandler = fl::function<Response(const Request&)>;

/// Async route handler function type (for non-blocking operations)
using AsyncRouteHandler = fl::function<void(const Request&, fl::function<void(Response)>)>;

/// Route pattern with method and path matching
struct Route {
    HttpMethod method;
    fl::string pattern;           // URL pattern like "/api/users/{id}"
    RouteHandler handler;         // Synchronous handler
    AsyncRouteHandler async_handler; // Asynchronous handler (optional)
    bool is_async = false;        // Whether to use async handler
    
    /// Create synchronous route
    Route(HttpMethod m, const fl::string& p, RouteHandler h) 
        : method(m), pattern(p), handler(fl::move(h)), is_async(false) {}
    
    /// Create asynchronous route
    Route(HttpMethod m, const fl::string& p, AsyncRouteHandler h) 
        : method(m), pattern(p), async_handler(fl::move(h)), is_async(true) {}
};

/// Main HTTP server class
class HttpServer : public EngineEvents::Listener {
public:
    /// Create HTTP server with default TCP transport
    HttpServer() : HttpServer(fl::make_shared<TcpServerTransport>()) {}
    
    /// Create HTTP server with custom transport
    explicit HttpServer(fl::shared_ptr<ServerTransport> transport) 
        : mTransport(fl::move(transport)) {}
    
    /// Destructor
    ~HttpServer();
    
    /// Factory methods for common server configurations
    
    /// Create local development server (localhost only, HTTP)
    /// @code
    /// auto server = HttpServer::create_local_server();
    /// server->listen(8080);  // Binds to 127.0.0.1:8080
    /// @endcode
    static fl::shared_ptr<HttpServer> create_local_server() {
        return fl::make_shared<HttpServer>(fl::make_shared<LocalServerTransport>());
    }
    
    /// Create HTTPS server with certificates
    /// @code
    /// auto server = HttpServer::create_https_server("cert.pem", "key.pem");
    /// server->listen(8443);  // HTTPS server on port 8443
    /// @endcode
    static fl::shared_ptr<HttpServer> create_https_server(const fl::string& cert_path, 
                                                          const fl::string& key_path) {
        return fl::make_shared<HttpServer>(
            fl::make_shared<HttpsServerTransport>(cert_path, key_path));
    }
    
    /// Create development server with mixed HTTP/HTTPS support
    /// @code
    /// auto server = HttpServer::create_development_server();
    /// server->listen(8080);  // Supports both HTTP and HTTPS
    /// @endcode
    static fl::shared_ptr<HttpServer> create_development_server() {
        return fl::make_shared<HttpServer>(fl::make_shared<DevelopmentServerTransport>());
    }
    
    /// Create server with custom transport configuration
    /// @code
    /// TcpServerTransport::Options opts;
    /// opts.limits.max_concurrent_connections = 100;
    /// opts.limits.connection_timeout_ms = 60000;
    /// 
    /// auto transport = fl::make_shared<TcpServerTransport>(opts);
    /// auto server = HttpServer::create_with_transport(transport);
    /// @endcode
    static fl::shared_ptr<HttpServer> create_with_transport(fl::shared_ptr<ServerTransport> transport) {
        return fl::make_shared<HttpServer>(fl::move(transport));
    }
    
    // ========== SERVER LIFECYCLE ==========
    
    /// Start listening on specified port
    /// @param port port number to listen on (0 for auto-assignment)
    /// @param bind_address IP address to bind to (nullptr for all interfaces)
    /// @returns true if server started successfully
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
    /// @code
    /// server.get("/health", [](const Request& req) {
    ///     return Response::ok("Server is healthy");
    /// });
    /// 
    /// server.post("/api/data", [](const Request& req) {
    ///     return Response::json("{\"status\": \"received\"}");
    /// });
    /// @endcode
    void get(const fl::string& pattern, RouteHandler handler);
    void post(const fl::string& pattern, RouteHandler handler);
    void put(const fl::string& pattern, RouteHandler handler);
    void delete_(const fl::string& pattern, RouteHandler handler);  // delete is keyword
    void patch(const fl::string& pattern, RouteHandler handler);
    void head(const fl::string& pattern, RouteHandler handler);
    void options(const fl::string& pattern, RouteHandler handler);
    
    /// Async route registration (for non-blocking handlers)
    /// @code
    /// server.get_async("/slow-api", [](const Request& req, auto callback) {
    ///     // Do async work...
    ///     setTimeout([callback]() {
    ///         callback(Response::ok("Async response"));
    ///     }, 1000);
    /// });
    /// @endcode
    void get_async(const fl::string& pattern, AsyncRouteHandler handler);
    void post_async(const fl::string& pattern, AsyncRouteHandler handler);
    void put_async(const fl::string& pattern, AsyncRouteHandler handler);
    void delete_async(const fl::string& pattern, AsyncRouteHandler handler);
    void patch_async(const fl::string& pattern, AsyncRouteHandler handler);
    void head_async(const fl::string& pattern, AsyncRouteHandler handler);
    void options_async(const fl::string& pattern, AsyncRouteHandler handler);
    
    /// Generic route registration
    /// @code
    /// server.route(HttpMethod::GET, "/custom", handler);
    /// server.route_async(HttpMethod::POST, "/async-custom", async_handler);
    /// @endcode
    void route(HttpMethod method, const fl::string& pattern, RouteHandler handler);
    void route_async(HttpMethod method, const fl::string& pattern, AsyncRouteHandler handler);
    
    // ========== MIDDLEWARE SUPPORT ==========
    
    /// Middleware function type
    using Middleware = fl::function<bool(const Request&, ResponseBuilder&)>;
    
    /// Add middleware that runs before all routes
    /// @code
    /// server.use([](const Request& req, ResponseBuilder& res) {
    ///     if (!req.has_header("Authorization")) {
    ///         res.status(401).body("Unauthorized");
    ///         return false;  // Stop processing
    ///     }
    ///     return true;  // Continue to next middleware/route
    /// });
    /// @endcode
    void use(Middleware middleware);
    
    /// Add middleware for specific path prefix
    /// @code
    /// server.use("/api", [](const Request& req, ResponseBuilder& res) {
    ///     res.cors_allow_origin("*");
    ///     return true;  // Continue processing
    /// });
    /// @endcode
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
    /// @code
    /// server.on_error([](const Request& req, int status, const fl::string& message) {
    ///     return Response::json(fl::string("{\"error\":\"") + message + "\"}").status(status);
    /// });
    /// @endcode
    void on_error(ErrorHandler handler);
    
    /// Set 404 handler
    /// @code
    /// server.on_not_found([](const Request& req) {
    ///     return Response::html("<h1>Page Not Found</h1>").status(404);
    /// });
    /// @endcode
    void on_not_found(RouteHandler handler);
    
    // ========== WEBSOCKET SUPPORT ==========
    
    /// WebSocket upgrade handler
    using WebSocketHandler = fl::function<void(const Request&, WebSocketConnection*)>;
    
    /// Add WebSocket endpoint
    /// @code
    /// server.websocket("/ws", [](const Request& req, WebSocketConnection* ws) {
    ///     ws->on_message([](const fl::string& message) {
    ///         FL_WARN("Received: " << message);
    ///     });
    /// });
    /// @endcode
    void websocket(const fl::string& pattern, WebSocketHandler handler);
    
    // ========== SERVER STATISTICS ==========
    
    /// Get server statistics
    /// @code
    /// auto stats = server.get_stats();
    /// FL_WARN("Active connections: " << stats.active_connections);
    /// FL_WARN("Total requests: " << stats.total_requests);
    /// @endcode
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
    fl::shared_ptr<ServerTransport> mTransport;
    fl::vector<Route> mRoutes;
    fl::vector<fl::pair<fl::string, Middleware>> mMiddlewares;  // path_prefix, middleware
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
    
    // Request processing
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
    
    // Friends
    friend class NetworkEventPump;
};

} // namespace fl
```

### 3. Usage Examples

#### **1. Simplest HTTP Server (Health Check Endpoint)**

```cpp
#include "fl/networking/http_server.h"

using namespace fl;

void setup() {
    // Create local development server (simplest setup)
    auto server = HttpServer::create_local_server();
    
    // Add health check endpoint (simplest route)
    server->get("/health", [](const Request& req) {
        return Response::ok("Server is healthy");
    });
    
    // Start listening on port 8080
    if (server->listen(8080)) {
        FL_WARN("Server listening on http://localhost:8080");
    } else {
        FL_WARN("Failed to start server");
    }
    
    // Server automatically processes requests via EngineEvents
}

void loop() {
    // Server handles requests automatically in background
    FastLED.show();
}
```

#### **2. IoT Device Control Server**

```cpp
class IoTControlServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    fl::vector<fl::u8> mSensorData;
    CRGB mLeds[100];
    
public:
    void initialize() {
        // Create local server for device control
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
        
        // Restart endpoint
        mServer->post("/restart", [](const Request& req) {
            FL_WARN("Restart requested");
            // Schedule restart after response is sent
            return Response::ok("Restarting device...");
        });
        
        // Start server
        if (mServer->listen(8080)) {
            FL_WARN("IoT Control Server listening on http://localhost:8080");
        }
    }
    
    void update_sensor_data() {
        // Update sensor readings (called periodically)
        mSensorData.clear();
        mSensorData.push_back(static_cast<fl::u8>(get_temperature()));
        mSensorData.push_back(static_cast<fl::u8>(get_humidity()));
        mSensorData.push_back(static_cast<fl::u8>(get_light_level()));
    }
    
private:
    fl::string get_device_status_json() const {
        return fl::string("{") +
            "\"uptime\": " + fl::to_string(millis()) + ", " +
            "\"free_memory\": " + fl::to_string(get_free_memory()) + ", " +
            "\"cpu_usage\": " + fl::to_string(get_cpu_usage()) + ", " +
            "\"wifi_connected\": " + (wifi_connected() ? "true" : "false") +
            "}";
    }
    
    void set_leds_from_json(const JsonDocument& json) {
        // Parse JSON and update LEDs
        // Implementation depends on your JSON library
    }
    
    void set_led_color(int index, CRGB color) {
        if (index >= 0 && index < 100) {
            mLeds[index] = color;
            FastLED.show();
        }
    }
    
    CRGB parse_color(const fl::string& color_str) {
        // Parse color string (hex, rgb, name, etc.)
        if (color_str == "red") return CRGB::Red;
        if (color_str == "green") return CRGB::Green;
        if (color_str == "blue") return CRGB::Blue;
        // Parse hex colors, etc.
        return CRGB::Black;
    }
    
    // Sensor reading functions
    float get_temperature() const { return 25.5f; }
    float get_humidity() const { return 60.2f; }
    float get_light_level() const { return 75.0f; }
    fl::size get_free_memory() const { return 50000; }
    float get_cpu_usage() const { return 15.5f; }
    bool wifi_connected() const { return true; }
};

// Usage
IoTControlServer control_server;

void setup() {
    FastLED.addLeds<WS2812B, 2, GRB>(control_server.mLeds, 100);
    control_server.initialize();
}

void loop() {
    // Update sensor data every 5 seconds
    EVERY_N_SECONDS(5) {
        control_server.update_sensor_data();
    }
    
    FastLED.show();
}
```

#### **3. API Server with Middleware**

```cpp
class APIServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    fl::string mApiKey = "fastled_api_key_123";
    
public:
    void initialize() {
        // Create production HTTP server
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
        setup_admin_routes();
        
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
        
        // Start server
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
            
            // Parse user data from request body
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
                "\"data\": [1, 2, 3, 4, 5]" +  // Placeholder data
                "}";
            
            return Response::json(data_json);
        });
        
        // Upload data (async processing)
        mServer->post_async("/api/data/upload", [](const Request& req, auto callback) {
            // Simulate async processing
            FL_WARN("Processing upload of " << req.body_size() << " bytes");
            
            // In real implementation, you'd process data asynchronously
            // For demo, just respond immediately
            callback(Response::json("{\"status\": \"upload_processed\"}"));
        });
        
        // Stream large dataset
        mServer->get("/api/data/stream", [](const Request& req) {
            // Create streaming response
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
    
    void setup_admin_routes() {
        // Server statistics
        mServer->get("/api/admin/stats", [this](const Request& req) {
            auto stats = mServer->get_stats();
            fl::string stats_json = fl::string("{") +
                "\"active_connections\": " + fl::to_string(stats.active_connections) + ", " +
                "\"total_requests\": " + fl::to_string(stats.total_requests_handled) + ", " +
                "\"uptime_ms\": " + fl::to_string(stats.server_uptime_ms) + ", " +
                "\"average_request_duration_ms\": " + fl::to_string(stats.average_request_duration_ms) +
                "}";
            return Response::json(stats_json);
        });
        
        // Server configuration
        mServer->get("/api/admin/config", [this](const Request& req) {
            auto config = mServer->get_config();
            fl::string config_json = fl::string("{") +
                "\"max_request_size\": " + fl::to_string(config.max_request_size) + ", " +
                "\"request_timeout_ms\": " + fl::to_string(config.request_timeout_ms) + ", " +
                "\"enable_compression\": " + (config.enable_compression ? "true" : "false") +
                "}";
            return Response::json(config_json);
        });
        
        // Restart server endpoint
        mServer->post("/api/admin/restart", [](const Request& req) {
            FL_WARN("Server restart requested");
            
            // Schedule restart after response is sent
            return Response::json("{\"status\": \"restart_scheduled\"}");
        });
    }
};

// Usage
APIServer api_server;

void setup() {
    api_server.initialize();
}

void loop() {
    FastLED.show();
}
```

#### **4. Static File Server with Web Interface**

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
                // Handle file upload
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
        // Apply the pattern to LEDs
    }
    
    void save_pattern_file(const Request& req) {
        // Save uploaded pattern file
        FL_WARN("Saving pattern file");
    }
};
```

#### **5. HTTPS Server with Authentication**

```cpp
class SecureAPIServer {
private:
    fl::shared_ptr<HttpServer> mServer;
    fl::unordered_map<fl::string, fl::string> mUsers;  // username -> password_hash
    fl::unordered_map<fl::string, fl::u32> mSessions;  // session_token -> expiry_time
    
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
        
        // Authentication routes (no auth required)
        setup_auth_routes();
        
        // Protected API routes (require authentication)
        mServer->use("/api", [this](const Request& req, ResponseBuilder& res) {
            return authenticate_request(req, res);
        });
        setup_protected_routes();
        
        // Admin routes (require admin role)
        mServer->use("/api/admin", [this](const Request& req, ResponseBuilder& res) {
            return authorize_admin(req, res);
        });
        setup_admin_routes();
        
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
                       .header("Set-Cookie", "session=" + session_token + "; HttpOnly; Secure; SameSite=Strict");
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
        
        // Public status endpoint
        mServer->get("/status", [](const Request& req) {
            return Response::json("{\"server\": \"FastLED HTTPS API\", \"status\": \"online\"}");
        });
    }
    
    void setup_protected_routes() {
        // User profile
        mServer->get("/api/profile", [](const Request& req) {
            // User info from authenticated session
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
            
            // Process secure upload
            save_secure_data(req.body_text());
            return Response::json("{\"status\": \"uploaded_securely\"}");
        });
    }
    
    void setup_admin_routes() {
        // System configuration (admin only)
        mServer->get("/api/admin/system", [](const Request& req) {
            fl::string system_info = fl::string("{") +
                "\"system_time\": " + fl::to_string(millis()) + ", " +
                "\"free_memory\": " + fl::to_string(get_free_memory()) + ", " +
                "\"active_sessions\": " + fl::to_string(get_active_session_count()) +
                "}";
            return Response::json(system_info);
        });
        
        // User management (admin only)
        mServer->get("/api/admin/users", [this](const Request& req) {
            fl::string users_json = "{\"users\": [";
            bool first = true;
            for (const auto& user : mUsers) {
                if (!first) users_json += ", ";
                users_json += "\"" + user.first + "\"";
                first = false;
            }
            users_json += "]}";
            return Response::json(users_json);
        });
        
        // Security logs (admin only)
        mServer->get("/api/admin/security-log", [](const Request& req) {
            return Response::json("{\"log_entries\": [\"No security events\"]}");
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
        
        return true;  // Continue processing
    }
    
    bool authorize_admin(const Request& req, ResponseBuilder& res) {
        // Additional admin authorization logic
        // For demo, just check if user is "admin"
        fl::string auth_header = req.header("Authorization");
        if (auth_header.find("admin_token") == fl::string::npos) {
            res.status(403).json("{\"error\": \"Admin access required\"}");
            return false;
        }
        return true;
    }
    
    bool verify_credentials(const fl::string& username, const fl::string& password) {
        auto it = mUsers.find(username);
        if (it == mUsers.end()) return false;
        
        return it->second == hash_password(password);
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
        // Generate secure random token - simplified for demo
        return "token_" + fl::to_string(millis()) + "_" + fl::to_string(random(10000));
    }
    
    void apply_secure_led_pattern(const fl::string& pattern) {
        FL_WARN("Applying secure LED pattern: " << pattern);
    }
    
    void save_secure_data(const fl::string& data) {
        FL_WARN("Saving secure data: " << data.substr(0, 50) << "...");
    }
    
    fl::size get_free_memory() const { return 50000; }
    fl::size get_active_session_count() const { return mSessions.size(); }
};
```

## Key Design Benefits

### **🎯 Progressive Complexity**

The HttpServer API follows the same progressive complexity approach as the HTTP client:

**Level 1 - Minimal (Health Check):**
```cpp
auto server = HttpServer::create_local_server();
server->get("/health", [](const Request& req) {
    return Response::ok("Server is healthy");
});
server->listen(8080);
```

**Level 2 - Add Routes:**
```cpp
server->post("/data", [](const Request& req) {
    return Response::json("{\"received\": " + fl::to_string(req.body_size()) + "}");
});
```

**Level 3 - Add Middleware:**
```cpp
server->use_cors();
server->use("/api", auth_middleware);
```

**Level 4 - Advanced Features:**
```cpp
server->websocket("/ws", websocket_handler);
server->use_static_files("/", "/www");
```

### **✅ Memory Efficiency**

- **fl::deque storage** for request/response bodies with PSRAM support
- **Copy-on-write semantics** for headers and text content
- **Streaming support** for large uploads/downloads
- **Small buffer optimization** for typical HTTP requests

### **✅ FastLED Integration**

- **EngineEvents integration** - automatic request processing
- **Non-blocking operation** - doesn't interfere with LED updates  
- **FastLED naming conventions** - consistent `fl::` namespace
- **Embedded-friendly** - works on resource-constrained devices

### **✅ Shared Ownership Benefits**

- **Transport sharing** - Multiple servers can share the same transport configuration
- **Connection pooling** - Connections can be efficiently shared and reused
- **Component reuse** - Server instances can be easily shared between application components
- **Memory efficiency** - Reference counting prevents premature destruction of shared resources
- **Thread safety** - fl::shared_ptr provides safe sharing between threads

### **✅ Enterprise Features**

- **TLS/HTTPS support** with certificate management
- **Middleware system** for authentication, CORS, logging
- **Route parameters** and query string parsing
- **WebSocket support** for real-time communication
- **Static file serving** with caching
- **Comprehensive error handling**
- **Request/response streaming** for large data
- **Connection pooling** and keep-alive support

The HttpServer design provides **enterprise-grade server functionality** while maintaining FastLED's **simple, embedded-friendly approach**! 🚀

### **🔧 Transport Factory Integration**

Similar to the HTTP client, the server supports transport factories for easy configuration:

```cpp
// Server transport factory
class ServerTransportFactory {
public:
    static ServerTransportFactory& instance() {
        static ServerTransportFactory factory;
        return factory;
    }
    
    /// Create HTTP-only server transport
    fl::shared_ptr<TcpServerTransport> create_http_server(int port = 80) {
        TcpServerTransport::Options opts;
        opts.transport_type = ServerTransportType::TCP_ONLY;
        opts.limits.max_concurrent_connections = 50;
        return get_or_create_transport<TcpServerTransport>(opts);
    }
    
    /// Create HTTPS-only server transport
    fl::shared_ptr<HttpsServerTransport> create_https_server(
        const fl::string& cert_path, const fl::string& key_path, int port = 443) {
        return fl::make_shared<HttpsServerTransport>(cert_path, key_path);
    }
    
    /// Create development server (HTTP + HTTPS)
    fl::shared_ptr<DevelopmentServerTransport> create_development_server() {
        return get_or_create_transport<DevelopmentServerTransport>();
    }
    
    /// Create local-only server (localhost binding)
    fl::shared_ptr<LocalServerTransport> create_local_server() {
        return get_or_create_transport<LocalServerTransport>();
    }
};

// Usage with factory - demonstrates shared ownership benefits
void setup_multiple_servers() {
    auto& factory = ServerTransportFactory::instance();
    
    // HTTP server on port 8080
    auto http_transport = factory.create_http_server(8080);
    auto http_server = HttpServer::create_with_transport(http_transport);
    
    // HTTPS server on port 8443
    auto https_transport = factory.create_https_server("cert.pem", "key.pem", 8443);
    auto https_server = HttpServer::create_with_transport(https_transport);
    
    // Local development server
    auto local_transport = factory.create_local_server();
    auto local_server = HttpServer::create_with_transport(local_transport);
    
    // Demonstrate shared ownership - multiple servers can share the same transport
    auto shared_transport = factory.create_development_server();
    auto api_server = HttpServer::create_with_transport(shared_transport);
    auto admin_server = HttpServer::create_with_transport(shared_transport);  // Shares same transport
    
    // Both api_server and admin_server use the same underlying transport configuration
    // Transport is automatically cleaned up when both servers are destroyed
}
```

This comprehensive HttpServer design complements the HTTP client perfectly, providing a complete networking solution for FastLED applications! 🌐 
