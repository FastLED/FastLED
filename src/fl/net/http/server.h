#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/types.h"
#include "fl/net/server_socket.h"
#include "fl/net/socket.h"
#include "fl/function.h"
#include "fl/shared_ptr.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "fl/string.h"
#include "fl/mutex.h"
#include "fl/future.h"
#include "fl/optional.h"

namespace fl {

/// Forward declarations
class HttpServer;
class ResponseBuilder;

/// Route handler function types
using RouteHandler = fl::function<Response(const Request&)>;
using AsyncRouteHandler = fl::function<void(const Request&, fl::function<void(Response)>)>;

/// Middleware function type - returns true to continue, false to stop processing
using Middleware = fl::function<bool(const Request&, ResponseBuilder&)>;

/// Error handler function type
using ErrorHandler = fl::function<Response(const Request&, int status_code, const fl::string& message)>;

// ========== Simple HTTP Server Functions (Level 1) ==========

/// Simple HTTP server factory functions
fl::shared_ptr<HttpServer> create_local_server();
fl::shared_ptr<HttpServer> create_development_server();

/// Quick server setup for common use cases
void serve_health_check(int port = 8080);
void serve_device_control(int port = 8080);

// ========== HTTP Response Builder ==========

/// HTTP response builder for constructing responses
class ResponseBuilder {
public:
    ResponseBuilder();
    
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
    
    /// Response body
    ResponseBuilder& body(const fl::string& content);
    ResponseBuilder& body(const char* content);
    ResponseBuilder& body(fl::span<const fl::u8> data);
    ResponseBuilder& json(const fl::string& json_content);
    ResponseBuilder& html(const fl::string& html_content);
    ResponseBuilder& text(const fl::string& text_content);
    
    /// Build the response
    Response build() &&;     // Move semantics for efficiency
    
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
    fl::vector<fl::u8> mBody;
};

// ========== Route Implementation ==========

/// Internal route representation
struct Route {
    HttpMethod method;
    fl::string pattern;
    RouteHandler handler;
    AsyncRouteHandler async_handler;
    bool is_async;
    
    // Default constructor for use in containers
    Route() : method(HttpMethod::GET), is_async(false) {}
    
    Route(HttpMethod m, const fl::string& p, RouteHandler h)
        : method(m), pattern(p), handler(h), is_async(false) {}
    
    Route(HttpMethod m, const fl::string& p, AsyncRouteHandler h)
        : method(m), pattern(p), async_handler(h), is_async(true) {}
};

/// Internal middleware representation
struct MiddlewareEntry {
    fl::string path_prefix;
    Middleware middleware;
    bool has_prefix;
    
    // Default constructor for use in containers
    MiddlewareEntry() : has_prefix(false) {}
    
    MiddlewareEntry(Middleware m) : middleware(m), has_prefix(false) {}
    MiddlewareEntry(const fl::string& prefix, Middleware m) 
        : path_prefix(prefix), middleware(m), has_prefix(true) {}
};

// ========== HTTP Server Class ==========

/// HTTP Server - uses existing ServerSocket without virtual methods
class HttpServer {
public:
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
    };
    
    /// Create HTTP server with default configuration
    HttpServer();
    
    /// Create HTTP server with custom configuration
    explicit HttpServer(const Config& config);
    
    /// Destructor
    ~HttpServer();
    
    // ========== FACTORY METHODS ==========
    
    /// Create local development server (localhost only, HTTP)
    static fl::shared_ptr<HttpServer> create_local_server();
    
    /// Create development server with extended timeouts
    static fl::shared_ptr<HttpServer> create_development_server();
    
    // ========== SERVER LIFECYCLE ==========
    
    /// Start listening on specified port
    bool listen(int port, const fl::string& bind_address = "0.0.0.0");
    
    /// Stop the server and close all connections
    void stop();
    
    /// Check if server is currently listening
    bool is_listening() const;
    
    /// Get bound port (useful when port 0 is used)
    int get_port() const;
    fl::string get_address() const;
    
    // ========== ROUTE REGISTRATION ==========
    
    /// Simple route registration
    void get(const fl::string& pattern, RouteHandler handler);
    void post(const fl::string& pattern, RouteHandler handler);
    void put(const fl::string& pattern, RouteHandler handler);
    void delete_(const fl::string& pattern, RouteHandler handler);  // delete is keyword
    void patch(const fl::string& pattern, RouteHandler handler);
    void head(const fl::string& pattern, RouteHandler handler);
    void options(const fl::string& pattern, RouteHandler handler);
    
    /// Async route registration
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
    
    /// Set custom error handler
    void on_error(ErrorHandler handler);
    
    /// Set 404 handler
    void on_not_found(RouteHandler handler);
    
    // ========== SERVER STATISTICS ==========
    
    /// Get server statistics
    ServerStats get_stats() const;
    
    // ========== CONFIGURATION ==========
    
    /// Set server configuration
    void configure(const Config& config);
    const Config& get_config() const;
    
    // ========== REQUEST PROCESSING ==========
    
    /// Process any pending network activity (called automatically by EngineEvents)
    void process_requests();
    
private:
    // Server state
    fl::shared_ptr<ServerSocket> mServerSocket;
    fl::vector<fl::shared_ptr<Socket>> mActiveConnections;
    bool mIsListening = false;
    Config mConfig;
    
    // Route and middleware management
    fl::vector<Route> mRoutes;
    fl::vector<MiddlewareEntry> mMiddlewares;
    ErrorHandler mErrorHandler;
    RouteHandler mNotFoundHandler;
    
    // Statistics
    mutable fl::mutex mStatsMutex;
    fl::size mTotalRequestsHandled = 0;
    fl::size mMiddlewareExecutions = 0;
    fl::size mRouteMatches = 0;
    fl::size mNotFoundResponses = 0;
    fl::size mErrorResponses = 0;
    fl::u32 mServerStartTime = 0;
    
    // Internal request processing
    void accept_new_connections();
    void handle_existing_connections();
    void handle_single_connection(fl::shared_ptr<Socket> connection);
    Request parse_http_request(Socket* socket);
    Response process_request(const Request& request);
    void send_http_response(Socket* socket, const Response& response);
    
    // Route matching and middleware
    Route* find_matching_route(const Request& request);
    bool execute_middlewares(const Request& request, ResponseBuilder& response_builder);
    bool matches_pattern(const fl::string& pattern, const fl::string& path, 
                        fl::vector<fl::pair<fl::string, fl::string>>& path_params);
    
    // HTTP parsing helpers
    fl::string read_http_line(Socket* socket);
    fl::string read_http_headers(Socket* socket, fl::vector<fl::pair<fl::string, fl::string>>& headers);
    fl::vector<fl::u8> read_http_body(Socket* socket, fl::size content_length);
    
    // Response helpers
    fl::string build_http_response_string(const Response& response);
    Response handle_error(const Request& request, int status_code, const fl::string& message);
    
    // Connection management
    void cleanup_closed_connections();
    void close_connection(fl::shared_ptr<Socket> connection);
    
    // Built-in handlers
    void initialize_default_handlers();
    Response handle_default_not_found(const Request& request);
    Response handle_default_error(const Request& request, int status_code, const fl::string& message);
    
    // Statistics
    void update_stats();
    void record_request_processed();
    void record_middleware_execution();
    void record_route_match();
    void record_not_found();
    void record_error();
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
