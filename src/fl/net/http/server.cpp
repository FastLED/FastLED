#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/server.h"
#include "fl/net/http/types.h"
#include "fl/net/server_socket.h"
#include "fl/net/socket_factory.h"
#include "fl/string.h"
#include "fl/sstream.h"
#include "fl/vector.h"
#include "fl/algorithm.h"
#include "fl/str.h"
#include "fl/math_macros.h"

namespace fl {

// ========== Simple HTTP Server Functions ==========

fl::shared_ptr<HttpServer> create_local_server() {
    return HttpServer::create_local_server();
}

fl::shared_ptr<HttpServer> create_development_server() {
    return HttpServer::create_development_server();
}

void serve_health_check(int port) {
    auto server = create_local_server();
    server->get("/health", [](const Request& req) {
        return ResponseBuilder::ok("Server is healthy");
    });
    
    if (server->listen(port)) {
        FL_WARN("Health check server listening on port " << port);
    } else {
        FL_WARN("Failed to start health check server on port " << port);
    }
}

void serve_device_control(int port) {
    auto server = create_local_server();
    
    server->get("/status", [](const Request& req) {
        fl::string status_json = fl::string("{") +
            "\"status\": \"running\", " +
            "\"uptime\": " + fl::to_string(millis()) + ", " +
            "\"device\": \"FastLED Controller\"" +
            "}";
        return ResponseBuilder::json_response(status_json);
    });
    
    if (server->listen(port)) {
        FL_WARN("Device control server listening on port " << port);
    } else {
        FL_WARN("Failed to start device control server on port " << port);
    }
}

// ========== ResponseBuilder Implementation ==========

ResponseBuilder::ResponseBuilder() = default;

ResponseBuilder& ResponseBuilder::status(int status_code) {
    mStatusCode = status_code;
    mStatusText = "";  // Will be auto-filled
    return *this;
}

ResponseBuilder& ResponseBuilder::status(int status_code, const fl::string& status_text) {
    mStatusCode = status_code;
    mStatusText = status_text;
    return *this;
}

ResponseBuilder& ResponseBuilder::header(const fl::string& name, const fl::string& value) {
    mHeaders.emplace_back(name, value);
    return *this;
}

ResponseBuilder& ResponseBuilder::header(const char* name, const char* value) {
    return header(fl::string(name), fl::string(value));
}

ResponseBuilder& ResponseBuilder::content_type(const fl::string& type) {
    return header("Content-Type", type);
}

ResponseBuilder& ResponseBuilder::content_length(fl::size length) {
    return header("Content-Length", fl::to_string(length));
}

ResponseBuilder& ResponseBuilder::cors_allow_origin(const fl::string& origin) {
    return header("Access-Control-Allow-Origin", origin);
}

ResponseBuilder& ResponseBuilder::cors_allow_methods(const fl::string& methods) {
    return header("Access-Control-Allow-Methods", methods);
}

ResponseBuilder& ResponseBuilder::cors_allow_headers(const fl::string& headers) {
    return header("Access-Control-Allow-Headers", headers);
}

ResponseBuilder& ResponseBuilder::cache_control(const fl::string& directives) {
    return header("Cache-Control", directives);
}

ResponseBuilder& ResponseBuilder::no_cache() {
    return header("Cache-Control", "no-cache, no-store, must-revalidate");
}

ResponseBuilder& ResponseBuilder::redirect(const fl::string& location, int status_code) {
    status(status_code);
    return header("Location", location);
}

ResponseBuilder& ResponseBuilder::body(const fl::string& content) {
    mBody.clear();
    const char* content_data = content.c_str();
    const fl::u8* u8_data = reinterpret_cast<const fl::u8*>(content_data);
    mBody.assign(u8_data, u8_data + content.size());
    return *this;
}

ResponseBuilder& ResponseBuilder::body(const char* content) {
    return body(fl::string(content));
}

ResponseBuilder& ResponseBuilder::body(fl::span<const fl::u8> data) {
    mBody.assign(data.begin(), data.end());
    return *this;
}

ResponseBuilder& ResponseBuilder::json(const fl::string& json_content) {
    content_type("application/json");
    return body(json_content);
}

ResponseBuilder& ResponseBuilder::html(const fl::string& html_content) {
    content_type("text/html");
    return body(html_content);
}

ResponseBuilder& ResponseBuilder::text(const fl::string& text_content) {
    content_type("text/plain");
    return body(text_content);
}

Response ResponseBuilder::build() && {
    Response response(static_cast<HttpStatusCode>(mStatusCode));
    
    // Set status text if not provided
    if (mStatusText.empty()) {
        mStatusText = get_status_text(static_cast<HttpStatusCode>(mStatusCode));
    }
    
    // Add headers
    for (const auto& header : mHeaders) {
        response.set_header(header.first, header.second);
    }
    
    // Set body
    if (!mBody.empty()) {
        response.set_body(mBody);
        
        // Auto-add Content-Length if not set
        bool has_content_length = false;
        for (const auto& header : mHeaders) {
            if (header.first == "Content-Length") {
                has_content_length = true;
                break;
            }
        }
        if (!has_content_length) {
            response.set_header("Content-Length", fl::to_string(mBody.size()));
        }
    }
    
    return response;
}



// Static response builders
Response ResponseBuilder::ok(const fl::string& content) {
    ResponseBuilder builder;
    builder.status(200).text(content);
    return fl::move(builder).build();
}

Response ResponseBuilder::json_response(const fl::string& json_content) {
    ResponseBuilder builder;
    builder.status(200).json(json_content);
    return fl::move(builder).build();
}

Response ResponseBuilder::html_response(const fl::string& html_content) {
    ResponseBuilder builder;
    builder.status(200).html(html_content);
    return fl::move(builder).build();
}

Response ResponseBuilder::text_response(const fl::string& text_content) {
    ResponseBuilder builder;
    builder.status(200).text(text_content);
    return fl::move(builder).build();
}

Response ResponseBuilder::not_found(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(404).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::bad_request(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(400).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::internal_error(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(500).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::method_not_allowed(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(405).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::unauthorized(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(401).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::forbidden(const fl::string& message) {
    ResponseBuilder builder;
    builder.status(403).text(message);
    return fl::move(builder).build();
}

Response ResponseBuilder::redirect_response(const fl::string& location, int status_code) {
    ResponseBuilder builder;
    builder.status(status_code).redirect(location);
    return fl::move(builder).build();
}

// ========== HttpServer Implementation ==========

HttpServer::HttpServer() {
    initialize_default_handlers();
    mServerStartTime = millis();
}

HttpServer::HttpServer(const Config& config) : mConfig(config) {
    initialize_default_handlers();
    mServerStartTime = millis();
}

HttpServer::~HttpServer() {
    stop();
}

fl::shared_ptr<HttpServer> HttpServer::create_local_server() {
    Config config;
    config.request_timeout_ms = 30000;  // Extended timeout for local dev
    config.enable_request_logging = true;
    return fl::make_shared<HttpServer>(config);
}

fl::shared_ptr<HttpServer> HttpServer::create_development_server() {
    Config config;
    config.request_timeout_ms = 60000;  // Very extended timeout for development
    config.keep_alive_timeout_ms = 60000;
    config.enable_request_logging = true;
    config.max_request_size = 10485760;  // 10MB for development
    return fl::make_shared<HttpServer>(config);
}

bool HttpServer::listen(int port, const fl::string& bind_address) {
    if (mIsListening) {
        return false;
    }
    
    // Create server socket using existing ServerSocket class
    SocketOptions socket_options;
    socket_options.enable_reuse_addr = true;
    socket_options.enable_nodelay = true;
    
    mServerSocket = fl::make_shared<ServerSocket>(socket_options);
    
    // Bind to address and port
    SocketError bind_result = mServerSocket->bind(bind_address, port);
    if (bind_result != SocketError::SUCCESS) {
        FL_WARN("Failed to bind server socket: " << mServerSocket->get_error_message());
        return false;
    }
    
    // Start listening
    SocketError listen_result = mServerSocket->listen(5);
    if (listen_result != SocketError::SUCCESS) {
        FL_WARN("Failed to listen on server socket: " << mServerSocket->get_error_message());
        return false;
    }
    
    mIsListening = true;
    
    FL_WARN("HTTP Server listening on " << bind_address << ":" << port);
    return true;
}

void HttpServer::stop() {
    if (!mIsListening) {
        return;
    }
    
    mIsListening = false;
    
    // Close all active connections
    for (auto& connection : mActiveConnections) {
        close_connection(connection);
    }
    mActiveConnections.clear();
    
    // Close server socket
    if (mServerSocket) {
        mServerSocket->close();
        mServerSocket.reset();
    }
    
    FL_WARN("HTTP Server stopped");
}

bool HttpServer::is_listening() const {
    return mIsListening && mServerSocket && mServerSocket->is_listening();
}

int HttpServer::get_port() const {
    return mServerSocket ? mServerSocket->bound_port() : 0;
}

fl::string HttpServer::get_address() const {
    return mServerSocket ? mServerSocket->bound_address() : "";
}

// Route registration methods
void HttpServer::get(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::GET, pattern, handler);
}

void HttpServer::post(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::POST, pattern, handler);
}

void HttpServer::put(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::PUT, pattern, handler);
}

void HttpServer::delete_(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::DELETE, pattern, handler);
}

void HttpServer::patch(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::PATCH, pattern, handler);
}

void HttpServer::head(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::HEAD, pattern, handler);
}

void HttpServer::options(const fl::string& pattern, RouteHandler handler) {
    route(HttpMethod::OPTIONS, pattern, handler);
}

void HttpServer::route(HttpMethod method, const fl::string& pattern, RouteHandler handler) {
    mRoutes.emplace_back(method, pattern, handler);
}

void HttpServer::route_async(HttpMethod method, const fl::string& pattern, AsyncRouteHandler handler) {
    mRoutes.emplace_back(method, pattern, handler);
}

// Middleware support
void HttpServer::use(Middleware middleware) {
    mMiddlewares.emplace_back(middleware);
}

void HttpServer::use(const fl::string& path_prefix, Middleware middleware) {
    mMiddlewares.emplace_back(path_prefix, middleware);
}

void HttpServer::use_cors(const fl::string& origin, const fl::string& methods, const fl::string& headers) {
    use([=](const Request& req, ResponseBuilder& res) {
        res.cors_allow_origin(origin)
           .cors_allow_methods(methods)
           .cors_allow_headers(headers);
        return true;
    });
}

void HttpServer::use_request_logging() {
    use([](const Request& req, ResponseBuilder& res) {
        FL_WARN("HTTP " << to_string(req.get_method()) << " " << req.get_url());
        return true;
    });
}

void HttpServer::use_json_parser() {
    // TODO: Implement JSON parsing middleware
    FL_WARN("JSON parser middleware not yet implemented");
}

void HttpServer::use_form_parser() {
    // TODO: Implement form parsing middleware  
    FL_WARN("Form parser middleware not yet implemented");
}

void HttpServer::use_static_files(const fl::string& mount_path, const fl::string& file_directory) {
    // TODO: Implement static file serving middleware
    FL_WARN("Static file middleware not yet implemented");
}

// Error handling
void HttpServer::on_error(ErrorHandler handler) {
    mErrorHandler = handler;
}

void HttpServer::on_not_found(RouteHandler handler) {
    mNotFoundHandler = handler;
}

// Server statistics
HttpServer::ServerStats HttpServer::get_stats() const {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    
    ServerStats stats;
    stats.active_connections = mActiveConnections.size();
    stats.total_connections_accepted = mServerSocket ? mServerSocket->current_connections() : 0;
    stats.total_requests_handled = mTotalRequestsHandled;
    stats.current_request_queue_size = 0;  // Not implemented yet
    stats.middleware_executions = mMiddlewareExecutions;
    stats.route_matches = mRouteMatches;
    stats.not_found_responses = mNotFoundResponses;
    stats.error_responses = mErrorResponses;
    stats.average_request_duration_ms = 0;  // Not implemented yet
    stats.server_uptime_ms = millis() - mServerStartTime;
    
    return stats;
}

// Configuration
void HttpServer::configure(const Config& config) {
    mConfig = config;
}

const HttpServer::Config& HttpServer::get_config() const {
    return mConfig;
}

// Request processing (main server loop)
void HttpServer::process_requests() {
    if (!is_listening()) {
        return;
    }
    
    accept_new_connections();
    handle_existing_connections();
    cleanup_closed_connections();
    update_stats();
}

// Private implementation methods

void HttpServer::accept_new_connections() {
    if (!mServerSocket || !mServerSocket->has_pending_connections()) {
        return;
    }
    
    // Accept up to 5 new connections per call to avoid blocking
    auto new_connections = mServerSocket->accept_multiple(5);
    
    for (auto& connection : new_connections) {
        if (connection) {
            mActiveConnections.push_back(connection);
            FL_WARN("Accepted new connection from " << connection->remote_address());
        }
    }
}

void HttpServer::handle_existing_connections() {
    for (auto& connection : mActiveConnections) {
        if (connection && connection->is_connected() && connection->has_data_available()) {
            handle_single_connection(connection);
        }
    }
}

void HttpServer::handle_single_connection(fl::shared_ptr<Socket> connection) {
    // Parse HTTP request
    Request request = parse_http_request(connection.get());
    
    if (!request.is_valid()) {
        FL_WARN("Invalid HTTP request received");
        Response error_response = ResponseBuilder::bad_request("Invalid HTTP request");
        send_http_response(connection.get(), error_response);
        close_connection(connection);
        return;
    }
    
    // Process request through middleware and routes
    Response response = process_request(request);
    
    // Send response
    send_http_response(connection.get(), response);
    
    // Close connection (HTTP/1.0 style for now)
    close_connection(connection);
    
    record_request_processed();
}

Request HttpServer::parse_http_request(Socket* socket) {
    Request request;
    
    // Read request line (e.g., "GET /path HTTP/1.1")
    fl::string request_line = read_http_line(socket);
    if (request_line.empty()) {
        return request; // Invalid request
    }
    
    // Parse request line manually (split by space)
    fl::vector<fl::string> parts;
    fl::size start = 0;
    for (fl::size i = 0; i <= request_line.size(); ++i) {
        if (i == request_line.size() || request_line[i] == ' ') {
            if (i > start) {
                parts.push_back(request_line.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    if (parts.size() != 3) {
        return request; // Invalid request line
    }
    
    // Set method
    auto method = parse_http_method(parts[0]);
    if (method) {
        request.set_method(*method);
    }
    
    // Set URL
    request.set_url(parts[1]);
    
    // Read headers
    fl::vector<fl::pair<fl::string, fl::string>> headers;
    read_http_headers(socket, headers);
    
    for (const auto& header : headers) {
        request.set_header(header.first, header.second);
    }
    
    // Read body if Content-Length is specified
    auto content_length_header = request.get_header("Content-Length");
    if (content_length_header) {
        fl::size content_length = fl::StringFormatter::parseInt(content_length_header->c_str());
        if (content_length > 0 && content_length <= mConfig.max_request_size) {
            fl::vector<fl::u8> body = read_http_body(socket, content_length);
            request.set_body(body);
        }
    }
    
    return request;
}

Response HttpServer::process_request(const Request& request) {
    ResponseBuilder response_builder;
    
    // Execute middleware chain
    if (!execute_middlewares(request, response_builder)) {
        // Middleware stopped processing - return current response
        return fl::move(response_builder).build();
    }
    
    // Find matching route
    Route* matching_route = find_matching_route(request);
    
    if (matching_route) {
        record_route_match();
        
        if (matching_route->is_async) {
            // TODO: Implement async route handling
            FL_WARN("Async routes not yet implemented");
            return handle_error(request, 501, "Async routes not implemented");
        } else {
            // Execute synchronous route handler
            return matching_route->handler(request);
        }
    } else {
        // No matching route found
        record_not_found();
        
        if (mNotFoundHandler) {
            return mNotFoundHandler(request);
        } else {
            return handle_default_not_found(request);
        }
    }
}

void HttpServer::send_http_response(Socket* socket, const Response& response) {
    fl::string response_string = build_http_response_string(response);
    
    // Convert to bytes and send
    fl::vector<fl::u8> response_bytes;
    response_bytes.reserve(response_string.size());
    for (char c : response_string) {
        response_bytes.push_back(static_cast<fl::u8>(c));
    }
    
    socket->write(fl::span<const fl::u8>(response_bytes.data(), response_bytes.size()));
    socket->flush();
}

Route* HttpServer::find_matching_route(const Request& request) {
    for (auto& route : mRoutes) {
        if (route.method == request.get_method()) {
            fl::vector<fl::pair<fl::string, fl::string>> path_params;
            if (matches_pattern(route.pattern, request.get_url(), path_params)) {
                // TODO: Set path parameters on request
                return &route;
            }
        }
    }
    return nullptr;
}

bool HttpServer::execute_middlewares(const Request& request, ResponseBuilder& response_builder) {
    for (const auto& middleware_entry : mMiddlewares) {
        // Check if middleware applies to this path
        if (middleware_entry.has_prefix) {
            if (!request.get_url().starts_with(middleware_entry.path_prefix)) {
                continue; // Skip this middleware
            }
        }
        
        // Execute middleware
        record_middleware_execution();
        bool continue_processing = middleware_entry.middleware(request, response_builder);
        
        if (!continue_processing) {
            return false; // Stop processing
        }
    }
    
    return true; // Continue processing
}

bool HttpServer::matches_pattern(const fl::string& pattern, const fl::string& path, 
                                fl::vector<fl::pair<fl::string, fl::string>>& path_params) {
    // Simple pattern matching for now - exact match or path parameters
    if (pattern == path) {
        return true;
    }
    
    // TODO: Implement proper path parameter matching (e.g., /users/{id})
    // For now, just do exact matches
    return false;
}

// HTTP parsing helpers
fl::string HttpServer::read_http_line(Socket* socket) {
    fl::string line;
    fl::u8 buffer[1];
    
    while (true) {
        fl::size bytes_read = socket->read(fl::span<fl::u8>(buffer, 1));
        if (bytes_read == 0) {
            break; // Connection closed or timeout
        }
        
        char c = static_cast<char>(buffer[0]);
        if (c == '\n') {
            break; // End of line
        } else if (c != '\r') {
            line += c;
        }
    }
    
    return line;
}

fl::string HttpServer::read_http_headers(Socket* socket, fl::vector<fl::pair<fl::string, fl::string>>& headers) {
    while (true) {
        fl::string line = read_http_line(socket);
        if (line.empty()) {
            break; // End of headers
        }
        
        // Parse header line (Name: Value)
        auto colon_pos = line.find(':');
        if (colon_pos != fl::string::npos) {
            fl::string name = line.substr(0, colon_pos);
            fl::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace from value
            while (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            
            headers.emplace_back(name, value);
        }
    }
    
    return "";
}

fl::vector<fl::u8> HttpServer::read_http_body(Socket* socket, fl::size content_length) {
    fl::vector<fl::u8> body;
    body.reserve(content_length);
    
    fl::u8 buffer[1024];
    fl::size total_read = 0;
    
    while (total_read < content_length) {
        fl::size to_read = fl::fl_min(content_length - total_read, sizeof(buffer));
        fl::size bytes_read = socket->read(fl::span<fl::u8>(buffer, to_read));
        
        if (bytes_read == 0) {
            break; // Connection closed or timeout
        }
        
        for (fl::size i = 0; i < bytes_read; ++i) {
            body.push_back(buffer[i]);
        }
        total_read += bytes_read;
    }
    
    return body;
}

fl::string HttpServer::build_http_response_string(const Response& response) {
    fl::sstream ss;
    
    // Status line
    ss << "HTTP/1.1 " << response.get_status_code_int() << " " << response.get_status_text() << "\r\n";
    
    // Headers
    auto headers = response.headers();
    for (const auto& header : headers.get_all()) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    // Default headers if not present
    if (!headers.has("Server")) {
        ss << "Server: FastLED/1.0\r\n";
    }
    
    if (!headers.has("Connection")) {
        ss << "Connection: close\r\n";
    }
    
    // End of headers
    ss << "\r\n";
    
    // Body
    if (response.has_body()) {
        ss << response.get_body_text();
    }
    
    return ss.str();
}

Response HttpServer::handle_error(const Request& request, int status_code, const fl::string& message) {
    record_error();
    
    if (mErrorHandler) {
        return mErrorHandler(request, status_code, message);
    } else {
        return handle_default_error(request, status_code, message);
    }
}

void HttpServer::cleanup_closed_connections() {
    auto it = fl::remove_if(mActiveConnections.begin(), mActiveConnections.end(),
        [](const fl::shared_ptr<Socket>& connection) {
            return !connection || !connection->is_connected();
        });
    
    mActiveConnections.erase(it, mActiveConnections.end());
}

void HttpServer::close_connection(fl::shared_ptr<Socket> connection) {
    if (connection && connection->is_connected()) {
        connection->disconnect();
    }
}

void HttpServer::initialize_default_handlers() {
    // Default 404 handler
    mNotFoundHandler = [this](const Request& request) {
        return handle_default_not_found(request);
    };
    
    // Default error handler
    mErrorHandler = [this](const Request& request, int status_code, const fl::string& message) {
        return handle_default_error(request, status_code, message);
    };
}

Response HttpServer::handle_default_not_found(const Request& request) {
    fl::string message = fl::string("Not Found: ") + request.get_url();
    return ResponseBuilder::not_found(message);
}

Response HttpServer::handle_default_error(const Request& request, int status_code, const fl::string& message) {
    return fl::move(ResponseBuilder().status(status_code).text(message)).build();
}

// Statistics methods
void HttpServer::update_stats() {
    // Update server statistics - called periodically
}

void HttpServer::record_request_processed() {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    mTotalRequestsHandled++;
}

void HttpServer::record_middleware_execution() {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    mMiddlewareExecutions++;
}

void HttpServer::record_route_match() {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    mRouteMatches++;
}

void HttpServer::record_not_found() {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    mNotFoundResponses++;
}

void HttpServer::record_error() {
    fl::lock_guard<fl::mutex> lock(mStatsMutex);
    mErrorResponses++;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
