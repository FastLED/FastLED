/// @file net/http/server.cpp.hpp
/// @brief HTTP server implementation

#pragma once

#include "fl/stl/asio/http/server.h"
#include "platforms/esp/is_esp.h"  // ok platform headers - for FL_IS_ESP32  // IWYU pragma: keep

// Common includes needed by both POSIX/Windows and ESP32 implementations
#include "fl/stl/cctype.h"
#include "fl/stl/cstring.h"
#include "fl/stl/malloc.h"
#include "fl/warn.h"

// ESP32 HTTP server header (must be before namespace declarations)
#if defined(FL_IS_ESP32) && !defined(FASTLED_HAS_NETWORKING)
// IWYU pragma: begin_keep
#include <esp_http_server.h>
// IWYU pragma: end_keep
#endif

#ifdef FASTLED_HAS_NETWORKING

#include "platforms/time_platform.h"

// Platform-specific socket includes
#ifdef FL_IS_WIN
    #include "platforms/win/socket_win.h"  // ok platform headers  // IWYU pragma: keep
    #define SOCKET_ERROR_WOULD_BLOCK WSAEWOULDBLOCK
    #define SOCKET_ERROR_IN_PROGRESS WSAEINPROGRESS
#else
    #include "platforms/posix/socket_posix.h"  // ok platform headers (includes all system socket headers)  // IWYU pragma: keep
    #define SOCKET_ERROR_WOULD_BLOCK EWOULDBLOCK
    #define SOCKET_ERROR_IN_PROGRESS EINPROGRESS
#endif

namespace fl {
namespace asio {
namespace http {

// ========== Async System Integration ==========

/// Internal async runner helper for Server
/// This bridges the Server::update() method (which returns size_t)
/// with the async_runner interface (which requires void update())
class Server::ServerAsyncRunner : public async_runner {
public:
    explicit ServerAsyncRunner(Server* server) : mServer(server) {}

    void update() override {
        if (mServer) {
            mServer->update();  // Call server's update() and discard return value
        }
    }

    bool has_active_tasks() const override {
        return mServer && mServer->is_running();
    }

    size_t active_task_count() const override {
        return (mServer && mServer->is_running()) ? mServer->mClientSockets.size() : 0;
    }

private:
    Server* mServer;
};

namespace {

// Connection timeout (30 seconds)
constexpr u32 CONNECTION_TIMEOUT_MS = 30000;

// Helper: Case-insensitive string comparison
bool iequals(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (fl::tolower(a[i]) != fl::tolower(b[i])) return false;
    }
    return true;
}

// Helper: Trim whitespace from string
string trim(const string& s) {
    size_t start = 0;
    while (start < s.size() && fl::isspace(s[start])) ++start;
    size_t end = s.size();
    while (end > start && fl::isspace(s[end - 1])) --end;
    return s.substr(start, end - start);
}

// Helper: Split string by delimiter
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    size_t start = 0;
    size_t end = s.find(delimiter);
    while (end != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

// Helper: Parse query string (e.g., "?id=123&name=test")
map<string, string> parse_query_string(const string& query) {
    map<string, string> params;
    if (query.empty() || query[0] != '?') return params;

    vector<string> pairs = split(query.substr(1), '&');
    for (const auto& pair : pairs) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != string::npos) {
            string key = pair.substr(0, eq_pos);
            string value = pair.substr(eq_pos + 1);
            params[key] = value;
        }
    }
    return params;
}

// Helper: Get HTTP status text
const char* status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

// Helper: Set socket to non-blocking mode
bool set_nonblocking(int fd) {
#ifdef FL_IS_WIN
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

} // anonymous namespace

//==============================================================================
// Request implementation
//==============================================================================

optional<string> Request::header(const string& name) const {
    for (auto it = mHeaders.begin(); it != mHeaders.end(); ++it) {
        if (iequals(it->first, name)) {
            return it->second;
        }
    }
    return nullopt;
}

optional<string> Request::query(const string& param) const {
    auto it = mQuery.find(param);
    if (it != mQuery.end()) {
        return it->second;
    }
    return nullopt;
}

//==============================================================================
// Response implementation
//==============================================================================

Response::Response() {
    mHeaders["Content-Type"] = "text/plain";
}

Response& Response::status(int code) {
    mStatusCode = code;
    return *this;
}

Response& Response::header(const string& name, const string& value) {
    mHeaders[name] = value;
    return *this;
}

Response& Response::body(const string& content) {
    mBody = content;
    return *this;
}

Response& Response::json(const class json& data) {
    mBody = data.to_string();
    mHeaders["Content-Type"] = "application/json";
    return *this;
}

Response Response::ok(const string& body) {
    Response resp;
    resp.status(200).body(body);
    return resp;
}

Response Response::not_found() {
    Response resp;
    resp.status(404).body("Not Found\n");
    return resp;
}

Response Response::bad_request(const string& message) {
    Response resp;
    resp.status(400).body(message + "\n");
    return resp;
}

Response Response::internal_error(const string& message) {
    Response resp;
    resp.status(500).body(message + "\n");
    return resp;
}

string Response::to_string() const {
    string result;

    // Status line
    result += "HTTP/1.0 ";
    result += fl::to_string(mStatusCode);
    result += " ";
    result += status_text(mStatusCode);
    result += "\r\n";

    // Headers
    for (auto it = mHeaders.begin(); it != mHeaders.end(); ++it) {
        result += it->first + ": " + it->second + "\r\n";
    }

    // Content-Length (auto-calculated)
    result += "Content-Length: " + fl::to_string(mBody.size()) + "\r\n";

    // End of headers
    result += "\r\n";

    // Body
    result += mBody;

    return result;
}

//==============================================================================
// HttpServer implementation
//==============================================================================

Server::Server() {
    // Register as engine event listener for automatic cleanup on exit
    EngineEvents::addListener(this);
}

Server::~Server() {
    // Unregister from engine events
    EngineEvents::removeListener(this);
    stop();
}

void Server::onExit() {
    // Automatically stop server on engine shutdown
    stop();
}

bool Server::start(int port) {
    if (mRunning) {
        mLastError = "Server already running";
        return false;
    }

    if (!setup_listen_socket(port)) {
        return false;
    }

    mPort = port;
    mRunning = true;
    mLastError.clear();

    // Register with async system for automatic updates
    if (!mAsyncRunner) {
        mAsyncRunner = fl::make_unique<ServerAsyncRunner>(this);
        AsyncManager::instance().register_runner(mAsyncRunner.get());
    }

    return true;
}

void Server::stop() {
    if (!mRunning) return;

    // Unregister from async system
    if (mAsyncRunner) {
        AsyncManager::instance().unregister_runner(mAsyncRunner.get());
        mAsyncRunner.reset();
    }

    // Close all client connections
    for (auto& client : mClientSockets) {
        if (client.fd != -1) {
            close(client.fd);
        }
    }
    mClientSockets.clear();

    // Close listen socket
    if (mListenSocket != -1) {
        close(mListenSocket);
        mListenSocket = -1;
    }

    mRunning = false;
}

void Server::route(const string& method, const string& path, RouteHandler handler) {
    mRoutes.push_back({method, path, handler});
}

void Server::get(const string& path, RouteHandler handler) {
    route("GET", path, handler);
}

void Server::post(const string& path, RouteHandler handler) {
    route("POST", path, handler);
}

void Server::put(const string& path, RouteHandler handler) {
    route("PUT", path, handler);
}

void Server::del(const string& path, RouteHandler handler) {
    route("DELETE", path, handler);
}

size_t Server::update() {
    if (!mRunning) return 0;

    accept_connections();
    cleanup_stale_connections();
    return process_requests();
}

bool Server::setup_listen_socket(int port) {
    // Create socket (initialization handled by socket wrapper)
    mListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (mListenSocket < 0) {
        mLastError = "Failed to create socket";
        return false;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)(&opt), sizeof(opt)) < 0) {
        close(mListenSocket);
        mListenSocket = -1;
        mLastError = "Failed to set SO_REUSEADDR";
        return false;
    }

    // Set non-blocking
    if (!set_nonblocking(mListenSocket)) {
        close(mListenSocket);
        mListenSocket = -1;
        mLastError = "Failed to set non-blocking mode";
        return false;
    }

    // Bind socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u16>(port));

    if (bind(mListenSocket, (sockaddr*)(&addr), sizeof(addr)) < 0) {
        close(mListenSocket);
        mListenSocket = -1;
        mLastError = "Failed to bind to port";
        return false;
    }

    // Listen
    if (listen(mListenSocket, 5) < 0) {
        close(mListenSocket);
        mListenSocket = -1;
        mLastError = "Failed to listen on socket";
        return false;
    }

    return true;
}

void Server::accept_connections() {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    // Accept all pending connections (non-blocking)
    while (true) {
        int client_fd = accept(mListenSocket,
                              (sockaddr*)(&client_addr),
                              &addr_len);

        if (client_fd < 0) {
#ifdef FL_IS_WIN
            if (WSAGetLastError() == SOCKET_ERROR_WOULD_BLOCK) break;
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;
#endif
            // Other error - log and continue
            break;
        }

        // Set client socket to non-blocking
        if (!set_nonblocking(client_fd)) {
            close(client_fd);
            continue;
        }

        // Add to client list
        ClientConnection client;
        client.fd = client_fd;
        client.connect_time = fl::platforms::millis();
        mClientSockets.push_back(client);
    }
}

size_t Server::process_requests() {
    size_t requests_processed = 0;

    // Process all clients (iterate backwards to allow removal)
    for (size_t i = mClientSockets.size(); i > 0; --i) {
        size_t index = i - 1;
        ClientConnection& client = mClientSockets[index];

        // Try to read request
        optional<Request> req = read_request(client);
        if (!req) {
            continue; // No complete request yet
        }

        // Find handler
        optional<RouteHandler> handler = find_handler(req->method(), req->path());

        Response resp;
        if (handler) {
            // Call handler
            resp = (*handler)(*req);
        } else {
            // No handler found - 404
            resp = Response::not_found();
        }

        // Send response
        send_response(client.fd, resp);

        // Close connection (HTTP/1.0 - no keep-alive)
        close_client(index);

        requests_processed++;
    }

    return requests_processed;
}

optional<Request> Server::read_request(ClientConnection& client) {
    // Read data from socket
    char buffer[4096];

#ifdef FL_IS_WIN
    int bytes = recv(client.fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (WSAGetLastError() != SOCKET_ERROR_WOULD_BLOCK) {
            return nullopt; // Connection closed or error
        }
        return nullopt; // No data yet
    }
#else
    ssize_t bytes = recv(client.fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (bytes <= 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            return nullopt; // Connection closed or error
        }
        return nullopt; // No data yet
    }
#endif

    client.buffer.append(buffer, static_cast<size_t>(bytes));

    // Check if we have complete HTTP request (ends with \r\n\r\n)
    size_t header_end = client.buffer.find("\r\n\r\n");
    if (header_end == string::npos) {
        return nullopt; // Headers not complete yet
    }

    // Parse request
    Request req;

    // Split into lines
    string header_section = client.buffer.substr(0, header_end);
    vector<string> lines = split(header_section, '\n');

    if (lines.empty()) {
        return nullopt;
    }

    // Parse request line: "GET /path HTTP/1.1\r"
    string request_line = trim(lines[0]);
    vector<string> parts = split(request_line, ' ');
    if (parts.size() < 3) {
        return nullopt;
    }

    req.mMethod = parts[0];

    // Split path and query string
    string full_path = parts[1];
    size_t query_pos = full_path.find('?');
    if (query_pos != string::npos) {
        req.mPath = full_path.substr(0, query_pos);
        req.mQuery = parse_query_string(full_path.substr(query_pos));
    } else {
        req.mPath = full_path;
    }

    req.mHttpVersion = parts[2];

    // Parse headers
    for (size_t i = 1; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos != string::npos) {
            string name = trim(line.substr(0, colon_pos));
            string value = trim(line.substr(colon_pos + 1));
            req.mHeaders[name] = value;
        }
    }

    // Read body if Content-Length present
    optional<string> content_length = req.header("Content-Length");
    if (content_length) {
        int body_len = atoi(content_length->c_str());
        if (body_len > 0) {
            size_t body_start = header_end + 4;
            size_t total_needed = body_start + static_cast<size_t>(body_len);

            if (client.buffer.size() < total_needed) {
                return nullopt; // Body not complete yet
            }

            req.mBody = client.buffer.substr(body_start, static_cast<size_t>(body_len));
        }
    }

    return req;
}

bool Server::send_response(int client_fd, const Response& response) {
    string data = response.to_string();
    const char* ptr = data.c_str();
    size_t remaining = data.size();

    while (remaining > 0) {
#ifdef FL_IS_WIN
        int sent = send(client_fd, ptr, static_cast<int>(remaining), 0);
#else
        ssize_t sent = send(client_fd, ptr, remaining, 0);
#endif

        if (sent <= 0) {
            return false;
        }

        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

optional<RouteHandler> Server::find_handler(const string& method, const string& path) const {
    for (const auto& entry : mRoutes) {
        if (entry.method == method && entry.path == path) {
            return entry.handler;
        }
    }
    return nullopt;
}

void Server::close_client(size_t index) {
    if (index >= mClientSockets.size()) return;

    close(mClientSockets[index].fd);
    mClientSockets.erase(mClientSockets.begin() + static_cast<ptrdiff_t>(index));
}

void Server::cleanup_stale_connections() {
    u32 now = fl::platforms::millis();

    for (size_t i = mClientSockets.size(); i > 0; --i) {
        size_t index = i - 1;
        const ClientConnection& client = mClientSockets[index];

        if (now - client.connect_time > CONNECTION_TIMEOUT_MS) {
            close_client(index);
        }
    }
}

} // namespace http
} // namespace asio
} // namespace fl

#elif defined(FL_IS_ESP32)

// ============================================================================
// ESP32 Implementation using esp_http_server.h
// ============================================================================

namespace fl {
namespace asio {
namespace http {

// Minimal definition for unique_ptr<ServerAsyncRunner> destruction
// (ESP32 uses esp_http_server tasks, not async_runner)
class Server::ServerAsyncRunner {};

// ========== ESP32 Helpers ==========

namespace {

// Map method string to httpd_method_t
httpd_method_t to_httpd_method(const string& method) {
    if (method == "GET") return HTTP_GET;
    if (method == "POST") return HTTP_POST;
    if (method == "PUT") return HTTP_PUT;
    if (method == "DELETE") return HTTP_DELETE;
    return HTTP_GET;
}

// Context passed to ESP-IDF URI handler callback
struct EspRouteContext {
    Server* server;
    size_t route_index;
};

// Forward declaration - implemented as Server static method for private access
esp_err_t esp_route_handler(httpd_req_t* req);

} // anonymous namespace

// Static method on Server - has access to private members of Request/Response.
// Signature uses void* to avoid ESP-IDF types in header; cast to httpd_req_t* here.
// This is called from the ESP-IDF HTTP server task.
int Server::handle_esp_request(void* raw_req) {
    auto* req = static_cast<httpd_req_t*>(raw_req);
    auto* ctx = static_cast<EspRouteContext*>(req->user_ctx);
    if (!ctx || !ctx->server) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No context");
        return ESP_FAIL;
    }

    // Build fl::asio::http::Request from httpd_req_t
    Request fl_req;
    fl_req.mPath = req->uri;
    switch (req->method) {
        case HTTP_GET:    fl_req.mMethod = "GET"; break;
        case HTTP_POST:   fl_req.mMethod = "POST"; break;
        case HTTP_PUT:    fl_req.mMethod = "PUT"; break;
        case HTTP_DELETE: fl_req.mMethod = "DELETE"; break;
        default:          fl_req.mMethod = "GET"; break;
    }
    fl_req.mHttpVersion = "HTTP/1.1";

    // Strip query string from path
    size_t q = fl_req.mPath.find('?');
    if (q != string::npos) {
        fl_req.mPath = fl_req.mPath.substr(0, q);
    }

    // Read POST body if present
    if (req->content_len > 0 && req->content_len <= 8192) {
        int total_len = req->content_len;
        char* buf = static_cast<char*>(fl::malloc(total_len + 1));
        if (buf) {
            int received = 0;
            while (received < total_len) {
                int ret = httpd_req_recv(req, buf + received, total_len - received);
                if (ret <= 0) {
                    break;
                }
                received += ret;
            }
            buf[received] = '\0';
            fl_req.mBody = string(buf, received);
            fl::free(buf);
        }
    }

    // Call the RouteHandler
    if (ctx->route_index >= ctx->server->mRoutes.size()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid route");
        return ESP_FAIL;
    }

    Response resp = ctx->server->mRoutes[ctx->route_index].handler(fl_req);

    // Send response using Response::to_string() which serializes to HTTP format
    // Use esp_http_server APIs to send the response parts
    char status_str[32];
    snprintf(status_str, sizeof(status_str), "%d", resp.mStatusCode);
    httpd_resp_set_status(req, status_str);

    for (auto it = resp.mHeaders.begin(); it != resp.mHeaders.end(); ++it) {
        httpd_resp_set_hdr(req, it->first.c_str(), it->second.c_str());
    }

    httpd_resp_send(req, resp.mBody.c_str(), resp.mBody.size());
    return ESP_OK;
}

namespace {

// ESP-IDF URI handler callback - delegates to Server::handle_esp_request
esp_err_t esp_route_handler(httpd_req_t* req) {
    return static_cast<esp_err_t>(Server::handle_esp_request(static_cast<void*>(req)));
}

} // anonymous namespace

// ========== Request implementation ==========

optional<string> Request::header(const string& name) const {
    for (auto it = mHeaders.begin(); it != mHeaders.end(); ++it) {
        if (it->first == name) {
            return it->second;
        }
    }
    return nullopt;
}

optional<string> Request::query(const string& param) const {
    auto it = mQuery.find(param);
    if (it != mQuery.end()) {
        return it->second;
    }
    return nullopt;
}

// ========== Response implementation ==========

Response::Response() {
    mHeaders["Content-Type"] = "text/plain";
}

Response& Response::status(int code) {
    mStatusCode = code;
    return *this;
}

Response& Response::header(const string& name, const string& value) {
    mHeaders[name] = value;
    return *this;
}

Response& Response::body(const string& content) {
    mBody = content;
    return *this;
}

Response& Response::json(const class json& data) {
    mBody = data.to_string();
    mHeaders["Content-Type"] = "application/json";
    return *this;
}

Response Response::ok(const string& body) {
    Response resp;
    resp.status(200).body(body);
    return resp;
}

Response Response::not_found() {
    Response resp;
    resp.status(404).body("Not Found\n");
    return resp;
}

Response Response::bad_request(const string& message) {
    Response resp;
    resp.status(400).body(message + "\n");
    return resp;
}

Response Response::internal_error(const string& message) {
    Response resp;
    resp.status(500).body(message + "\n");
    return resp;
}

string Response::to_string() const {
    // Not used on ESP32 (responses sent via httpd_resp_send)
    return mBody;
}

// ========== Server implementation (ESP32) ==========

// ESP32 stores httpd_handle_t and route contexts via mListenSocket as an opaque int
// and mClientSockets is unused. We use a static map for the httpd handle.

// We store the httpd_handle_t in a file-scoped variable since the Server class
// members (mListenSocket, mClientSockets) are typed for POSIX sockets.
static httpd_handle_t s_esp_httpd = nullptr;
static fl::vector<fl::unique_ptr<EspRouteContext>> s_esp_route_contexts;

Server::Server() {
    EngineEvents::addListener(this);
}

Server::~Server() {
    EngineEvents::removeListener(this);
    stop();
}

void Server::onExit() {
    stop();
}

bool Server::start(int port) {
    if (mRunning) {
        mLastError = "Server already running";
        return false;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = static_cast<u16>(port);

    esp_err_t err = httpd_start(&s_esp_httpd, &config);
    if (err != ESP_OK) {
        mLastError = "httpd_start failed";
        FL_WARN("[HTTP] httpd_start failed: " << esp_err_to_name(err));
        return false;
    }

    // Register all routes that were added before start()
    for (size_t i = 0; i < mRoutes.size(); ++i) {
        auto ctx = fl::make_unique<EspRouteContext>(EspRouteContext{this, i});

        httpd_uri_t uri_handler = {};
        uri_handler.uri = mRoutes[i].path.c_str();
        uri_handler.method = to_httpd_method(mRoutes[i].method);
        uri_handler.handler = esp_route_handler;
        uri_handler.user_ctx = ctx.get();

        esp_err_t reg_err = httpd_register_uri_handler(s_esp_httpd, &uri_handler);
        if (reg_err != ESP_OK) {
            FL_WARN("[HTTP] Failed to register route " << mRoutes[i].path.c_str());
        }
        s_esp_route_contexts.push_back(fl::move(ctx));
    }

    mPort = port;
    mRunning = true;
    mLastError.clear();

    FL_WARN("[HTTP] Server started on port " << port);
    return true;
}

void Server::stop() {
    if (!mRunning) return;

    if (s_esp_httpd) {
        httpd_stop(s_esp_httpd);
        s_esp_httpd = nullptr;
    }

    // Clean up route contexts (unique_ptr auto-deletes on clear)
    s_esp_route_contexts.clear();

    mRunning = false;
    FL_WARN("[HTTP] Server stopped");
}

void Server::route(const string& method, const string& path, RouteHandler handler) {
    mRoutes.push_back({method, path, handler});

    // If server is already running, register the route immediately
    if (mRunning && s_esp_httpd) {
        size_t idx = mRoutes.size() - 1;
        auto ctx = fl::make_unique<EspRouteContext>(EspRouteContext{this, idx});

        httpd_uri_t uri_handler = {};
        uri_handler.uri = mRoutes[idx].path.c_str();
        uri_handler.method = to_httpd_method(method);
        uri_handler.handler = esp_route_handler;
        uri_handler.user_ctx = ctx.get();

        httpd_register_uri_handler(s_esp_httpd, &uri_handler);
        s_esp_route_contexts.push_back(fl::move(ctx));
    }
}

void Server::get(const string& path, RouteHandler handler) {
    route("GET", path, handler);
}

void Server::post(const string& path, RouteHandler handler) {
    route("POST", path, handler);
}

void Server::put(const string& path, RouteHandler handler) {
    route("PUT", path, handler);
}

void Server::del(const string& path, RouteHandler handler) {
    route("DELETE", path, handler);
}

size_t Server::update() {
    // ESP-IDF HTTP server runs in its own task, no polling needed
    return 0;
}

} // namespace http
} // namespace asio
} // namespace fl

#else // !FASTLED_HAS_NETWORKING && !FL_IS_ESP32

// Stub implementation for platforms without networking
namespace fl {
namespace asio {
namespace http {

// Minimal definition for unique_ptr<ServerAsyncRunner> destruction
class Server::ServerAsyncRunner {};

optional<string> Request::header(const string&) const { return nullopt; }
optional<string> Request::query(const string&) const { return nullopt; }

Response::Response() = default;
Response& Response::status(int) { return *this; }
Response& Response::header(const string&, const string&) { return *this; }
Response& Response::body(const string&) { return *this; }
Response& Response::json(const json&) { return *this; }

Response Response::ok(const string&) { return Response(); }
Response Response::not_found() { return Response(); }
Response Response::bad_request(const string&) { return Response(); }
Response Response::internal_error(const string&) { return Response(); }

string Response::to_string() const { return ""; }

Server::Server() {}
Server::~Server() = default;
void Server::onExit() {}
bool Server::start(int) { return false; }
void Server::stop() {}
void Server::route(const string&, const string&, RouteHandler) {}
void Server::get(const string&, RouteHandler) {}
void Server::post(const string&, RouteHandler) {}
void Server::put(const string&, RouteHandler) {}
void Server::del(const string&, RouteHandler) {}
size_t Server::update() { return 0; }

} // namespace http
} // namespace asio
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
