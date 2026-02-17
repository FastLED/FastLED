/// @file net/http/server.cpp.hpp
/// @brief HTTP server implementation

#pragma once

#include "fl/net/http/server.h"

#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/cctype.h"
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
namespace net {
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
        if (tolower(a[i]) != tolower(b[i])) return false;
    }
    return true;
}

// Helper: Trim whitespace from string
string trim(const string& s) {
    size_t start = 0;
    while (start < s.size() && isspace(s[start])) ++start;
    size_t end = s.size();
    while (end > start && isspace(s[end - 1])) --end;
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

Response& Response::json(const Json& data) {
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
        mAsyncRunner = new ServerAsyncRunner(this);
        AsyncManager::instance().register_runner(mAsyncRunner);
    }

    return true;
}

void Server::stop() {
    if (!mRunning) return;

    // Unregister from async system
    if (mAsyncRunner) {
        AsyncManager::instance().unregister_runner(mAsyncRunner);
        delete mAsyncRunner;
        mAsyncRunner = nullptr;
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
} // namespace net
} // namespace fl

#else // !FASTLED_HAS_NETWORKING

// Stub implementation for platforms without networking
namespace fl {
namespace net {
namespace http {

optional<string> Request::header(const string&) const { return nullopt; }
optional<string> Request::query(const string&) const { return nullopt; }

Response::Response() = default;
Response& Response::status(int) { return *this; }
Response& Response::header(const string&, const string&) { return *this; }
Response& Response::body(const string&) { return *this; }
Response& Response::json(const Json&) { return *this; }

Response Response::ok(const string&) { return Response(); }
Response Response::not_found() { return Response(); }
Response Response::bad_request(const string&) { return Response(); }
Response Response::internal_error(const string&) { return Response(); }

string Response::to_string() const { return ""; }

Server::~Server() = default;
bool Server::start(int) { return false; }
void Server::stop() {}
void Server::route(const string&, const string&, RouteHandler) {}
void Server::get(const string&, RouteHandler) {}
void Server::post(const string&, RouteHandler) {}
void Server::put(const string&, RouteHandler) {}
void Server::del(const string&, RouteHandler) {}
size_t Server::update() { return 0; }

} // namespace http
} // namespace net
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
