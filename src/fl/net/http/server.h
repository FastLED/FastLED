/// @file net/http/server.h
/// @brief Minimal HTTP server for FastLED examples
///
/// Provides a simple HTTP/1.0 server API using fl::function for route handlers.
/// Designed for educational purposes and host-based testing (POSIX/Windows).
///
/// Example usage:
/// @code
///   using namespace fl::net::http;
///
///   Server server;
///
///   server.get("/", [](const Request& req) {
///       return Response::ok("Hello from FastLED!\n");
///   });
///
///   server.post("/color", [](const Request& req) {
///       fl::Json body = fl::Json::parse(req.body());
///       int r = body["r"] | 0;
///       // ... process color
///       return Response::ok("Color updated\n");
///   });
///
///   if (server.start(8080)) {
///       // In loop():
///       server.update();
///   }
/// @endcode

#pragma once

#include "fl/int.h"
#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/map.h"
#include "fl/json.h"
#include "fl/async.h"
#include "fl/engine_events.h"

namespace fl {
namespace net {
namespace http {

/// HTTP request object (immutable, passed by const reference)
class Request {
public:
    Request() = default;

    /// Get HTTP method (e.g., "GET", "POST", "PUT", "DELETE")
    const string& method() const { return mMethod; }

    /// Get request path (e.g., "/", "/api/status")
    const string& path() const { return mPath; }

    /// Get request body (for POST/PUT requests)
    const string& body() const { return mBody; }

    /// Get HTTP version (e.g., "HTTP/1.1")
    const string& http_version() const { return mHttpVersion; }

    /// Get header value by name (case-insensitive)
    /// @param name Header name (e.g., "Content-Type")
    /// @return Header value if present, otherwise nullopt
    optional<string> header(const string& name) const;

    /// Get query parameter value by name
    /// @param param Parameter name (e.g., "id" for "/?id=123")
    /// @return Parameter value if present, otherwise nullopt
    optional<string> query(const string& param) const;

    /// Check if request is GET
    bool is_get() const { return mMethod == "GET"; }

    /// Check if request is POST
    bool is_post() const { return mMethod == "POST"; }

    /// Check if request has a body
    bool has_body() const { return !mBody.empty(); }

private:
    friend class Server;

    string mMethod;
    string mPath;
    string mHttpVersion;
    string mBody;
    map<string, string> mHeaders;
    map<string, string> mQuery;
};

/// HTTP response builder (fluent interface)
class Response {
public:
    Response();

    /// Set HTTP status code
    /// @param code Status code (e.g., 200, 404, 500)
    /// @return Reference to this for chaining
    Response& status(int code);

    /// Add HTTP header
    /// @param name Header name
    /// @param value Header value
    /// @return Reference to this for chaining
    Response& header(const string& name, const string& value);

    /// Set response body
    /// @param content Body content
    /// @return Reference to this for chaining
    Response& body(const string& content);

    /// Set JSON response body with automatic Content-Type header
    /// @param data JSON object to serialize
    /// @return Reference to this for chaining
    Response& json(const Json& data);

    /// Factory method for 200 OK response
    /// @param body Optional body content
    /// @return Response with status 200
    static Response ok(const string& body = "");

    /// Factory method for 404 Not Found response
    /// @return Response with status 404
    static Response not_found();

    /// Factory method for 400 Bad Request response
    /// @param message Error message
    /// @return Response with status 400
    static Response bad_request(const string& message);

    /// Factory method for 500 Internal Server Error response
    /// @param message Error message
    /// @return Response with status 500
    static Response internal_error(const string& message);

private:
    friend class Server;

    int mStatusCode = 200;
    string mBody;
    map<string, string> mHeaders;

    string to_string() const;
};

/// Route handler function signature
/// Takes const reference to Request, returns Response by value
using RouteHandler = function<Response(const Request&)>;

/// HTTP Server class
///
/// Minimal HTTP/1.0 server with non-blocking I/O.
/// Designed for educational purposes and host-based testing.
///
/// Platform Support:
/// - POSIX (Linux, macOS)
/// - Windows (via Winsock)
/// - NOT supported on embedded platforms (no networking stack)
///
/// @note Server automatically integrates with FastLED's async system.
/// When the server is running, it will automatically process requests
/// during FastLED.show(), delay(), and async_run() calls.
/// The server also listens for engine shutdown events and cleans up automatically.
class Server : public EngineEvents::Listener {
public:
    /// Constructor
    Server();

    /// Destructor (stops server if running)
    ~Server();

    // Non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /// Start server on specified port
    /// @param port Port to listen on (default: 8080)
    /// @return true if started successfully, false otherwise
    bool start(int port = 8080);

    /// Stop server and close all connections
    void stop();

    /// Register route handler using fl::function
    /// @param method HTTP method ("GET", "POST", "PUT", "DELETE")
    /// @param path URL path ("/", "/api/status", etc.)
    /// @param handler Route handler function (fl::function)
    void route(const string& method, const string& path, RouteHandler handler);

    /// Convenience method for GET routes
    /// @param path URL path
    /// @param handler Route handler function
    void get(const string& path, RouteHandler handler);

    /// Convenience method for POST routes
    /// @param path URL path
    /// @param handler Route handler function
    void post(const string& path, RouteHandler handler);

    /// Convenience method for PUT routes
    /// @param path URL path
    /// @param handler Route handler function
    void put(const string& path, RouteHandler handler);

    /// Convenience method for DELETE routes
    /// @param path URL path
    /// @param handler Route handler function
    void del(const string& path, RouteHandler handler);

    /// Update server (process pending requests non-blocking)
    /// @note This is called automatically by the async system when the server is running.
    /// You can still call it manually in loop() if needed for explicit control.
    /// @return Number of requests processed this update
    size_t update();

    /// Check if server is running
    bool is_running() const { return mRunning; }

    /// Get server port
    int port() const { return mPort; }

    /// Get last error message
    string last_error() const { return mLastError; }

private:
    // EngineEvents::Listener implementation
    void onExit() override;

    // Forward declaration for async integration helper
    class ServerAsyncRunner;
    struct RouteEntry {
        string method;
        string path;
        RouteHandler handler;
    };

    struct ClientConnection {
        int fd = -1;
        u32 connect_time = 0;
        string buffer;
    };

    int mPort = 0;
    int mListenSocket = -1;
    bool mRunning = false;
    string mLastError;

    vector<RouteEntry> mRoutes;
    vector<ClientConnection> mClientSockets;

    // Async system integration
    ServerAsyncRunner* mAsyncRunner = nullptr;

    bool setup_listen_socket(int port);
    void accept_connections();
    size_t process_requests();
    optional<Request> read_request(ClientConnection& client);
    bool send_response(int client_fd, const Response& response);
    optional<RouteHandler> find_handler(const string& method, const string& path) const;
    void close_client(size_t index);
    void cleanup_stale_connections();
};

} // namespace http
} // namespace net

// Convenience type aliases for common use
using HttpServer = net::http::Server;
using http_request = net::http::Request;
using http_response = net::http::Response;

} // namespace fl
