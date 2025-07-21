#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/http_transport.h"
#include "fl/networking/socket_factory.h"
#include "fl/algorithm.h"

namespace fl {

// ========== Transport Error Functions ==========

fl::string to_string(TransportError error) {
    switch (error) {
        case TransportError::SUCCESS: return "Success";
        case TransportError::NETWORK_ERROR: return "Network Error";
        case TransportError::TIMEOUT: return "Timeout";
        case TransportError::SSL_ERROR: return "SSL Error";
        case TransportError::INVALID_URL: return "Invalid URL";
        case TransportError::INVALID_RESPONSE: return "Invalid Response";
        case TransportError::TOO_MANY_REDIRECTS: return "Too Many Redirects";
        case TransportError::RESPONSE_TOO_LARGE: return "Response Too Large";
        case TransportError::CONNECTION_FAILED: return "Connection Failed";
        case TransportError::UNSUPPORTED_SCHEME: return "Unsupported Scheme";
        case TransportError::PROTOCOL_ERROR: return "Protocol Error";
        case TransportError::UNKNOWN_ERROR: return "Unknown Error";
        default: return "Unknown Error";
    }
}

// ========== BaseTransport Implementation ==========

BaseTransport::BaseTransport() {
    clear_error();
}

void BaseTransport::reset_stats() {
    mStats = TransportStats{};
}

void BaseTransport::update_stats_request_start() {
    mRequestStartTime = get_current_time_ms();
    mStats.total_requests++;
}

void BaseTransport::update_stats_request_success(fl::size bytes_sent, fl::size bytes_received, fl::u32 duration_ms) {
    mStats.successful_requests++;
    mStats.bytes_sent += bytes_sent;
    mStats.bytes_received += bytes_received;
    mStats.last_request_time_ms = duration_ms;
    
    // Update average response time
    if (mStats.successful_requests > 0) {
        fl::u64 total_time = static_cast<fl::u64>(mStats.average_response_time_ms) * (mStats.successful_requests - 1) + duration_ms;
        mStats.average_response_time_ms = static_cast<fl::u32>(total_time / mStats.successful_requests);
    }
}

void BaseTransport::update_stats_request_failure() {
    mStats.failed_requests++;
}

void BaseTransport::update_stats_redirect() {
    mStats.redirects_followed++;
}

void BaseTransport::set_error(TransportError error, const fl::string& message) {
    mLastError = error;
    mLastErrorMessage = message.empty() ? to_string(error) : message;
}

void BaseTransport::clear_error() {
    mLastError = TransportError::SUCCESS;
    mLastErrorMessage.clear();
}

fl::future<Response> BaseTransport::handle_redirects(const Request& original_request, const Response& response) {
    if (!should_follow_redirect(response)) {
        // Return a future that's already completed with the response
        return fl::make_ready_future(response);
    }
    
    if (mCurrentRedirectCount >= mMaxRedirects) {
        set_error(TransportError::TOO_MANY_REDIRECTS, "Maximum redirect limit exceeded");
        return fl::make_error_future<Response>("Maximum redirect limit exceeded");
    }
    
    auto redirect_request = build_redirect_request(original_request, response);
    if (redirect_request.empty()) {
        set_error(TransportError::INVALID_RESPONSE, "Invalid redirect location");
        return fl::make_error_future<Response>("Invalid redirect location");
    }
    
    mCurrentRedirectCount++;
    update_stats_redirect();
    
    // Send the redirect request
    return send_request(*redirect_request.ptr());
}

bool BaseTransport::should_follow_redirect(const Response& response) const {
    if (!mFollowRedirects) {
        return false;
    }
    
    fl::u16 status = response.get_status_code_int();
    return (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) &&
           !response.get_location().empty();
}

fl::optional<Request> BaseTransport::build_redirect_request(const Request& original_request, const Response& response) {
    auto location = response.get_location();
    if (location.empty()) {
        return fl::optional<Request>();
    }
    
    // Create new request with redirect URL
    Request redirect_request = original_request;
    redirect_request.set_url(*location.ptr());
    
    // For 303 See Other, always use GET
    if (response.get_status_code_int() == 303) {
        redirect_request.set_method(HttpMethod::GET);
        redirect_request.clear_body();
    }
    
    return redirect_request;
}

bool BaseTransport::validate_response(const Response& response) {
    if (!response.is_valid()) {
        set_error(TransportError::INVALID_RESPONSE, response.get_validation_error());
        return false;
    }
    
    // Check response size limits
    auto content_length = response.get_content_length();
    if (!content_length.empty() && !check_response_size(*content_length.ptr())) {
        return false;
    }
    
    return true;
}

bool BaseTransport::check_response_size(fl::size content_length) {
    if (content_length > mMaxResponseSize) {
        set_error(TransportError::RESPONSE_TOO_LARGE, "Response size exceeds maximum allowed");
        return false;
    }
    return true;
}

fl::u32 BaseTransport::get_current_time_ms() {
    return fl::time();
}

// ========== SimpleConnectionPool Implementation ==========

SimpleConnectionPool::SimpleConnectionPool(const Options& options) 
    : mOptions(options) {
}

SimpleConnectionPool::~SimpleConnectionPool() {
    close_all_connections();
}

fl::shared_ptr<Socket> SimpleConnectionPool::get_connection(const fl::string& host, int port) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    
    // Cleanup expired connections first
    cleanup_expired_connections();
    
    // Look for existing available connection
    for (auto& entry : mConnections) {
        if (entry.host == host && entry.port == port && !entry.in_use && is_connection_valid(entry)) {
            entry.in_use = true;
            entry.last_used_time = fl::time();
            return entry.socket;
        }
    }
    
    // Check if we can create a new connection
    fl::size host_connections = 0;
    for (const auto& entry : mConnections) {
        if (entry.host == host && entry.port == port) {
            host_connections++;
        }
    }
    
    if (host_connections >= mOptions.max_connections_per_host || 
        mConnections.size() >= mOptions.max_total_connections) {
        return nullptr; // Connection limit reached
    }
    
    // Create new connection
    auto socket = create_new_connection(host, port);
    if (socket) {
        ConnectionEntry entry;
        entry.socket = socket;
        entry.host = host;
        entry.port = port;
        entry.last_used_time = fl::time();
        entry.in_use = true;
        
        mConnections.push_back(entry);
    }
    
    return socket;
}

void SimpleConnectionPool::return_connection(fl::shared_ptr<Socket> socket, const fl::string& host, int port) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    
    for (auto& entry : mConnections) {
        if (entry.socket == socket && entry.host == host && entry.port == port) {
            entry.in_use = false;
            entry.last_used_time = fl::time();
            break;
        }
    }
}

void SimpleConnectionPool::close_all_connections() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    
    for (auto& entry : mConnections) {
        if (entry.socket) {
            entry.socket->disconnect();
        }
    }
    
    mConnections.clear();
}

fl::size SimpleConnectionPool::get_active_connections() const {
    fl::lock_guard<fl::mutex> lock(mMutex);
    
    fl::size count = 0;
    for (const auto& entry : mConnections) {
        if (entry.in_use) {
            count++;
        }
    }
    return count;
}

fl::size SimpleConnectionPool::get_total_connections() const {
    fl::lock_guard<fl::mutex> lock(mMutex);
    return mConnections.size();
}

void SimpleConnectionPool::set_max_connections_per_host(fl::size max_connections) {
    mOptions.max_connections_per_host = max_connections;
}

void SimpleConnectionPool::set_max_total_connections(fl::size max_connections) {
    mOptions.max_total_connections = max_connections;
}

void SimpleConnectionPool::set_connection_timeout(fl::u32 timeout_ms) {
    mOptions.connection_timeout_ms = timeout_ms;
}

fl::string SimpleConnectionPool::make_connection_key(const fl::string& host, int port) {
    (void)port; // Suppress unused parameter warning - TODO: implement proper int to string
    return host + ":" + fl::string(); // Simple concatenation - would need proper int to string
}

void SimpleConnectionPool::cleanup_expired_connections() {
    fl::u32 current_time = fl::time();
    
    // Create new vector with only valid connections
    fl::vector<ConnectionEntry> valid_connections;
    
    for (const auto& entry : mConnections) {
        bool should_keep = true;
        
        if (!entry.in_use) {
            fl::u32 age = current_time - entry.last_used_time;
            if (age > mOptions.connection_timeout_ms) {
                if (entry.socket) {
                    entry.socket->disconnect();
                }
                should_keep = false;
            } else if (!is_connection_valid(entry)) {
                should_keep = false;
            }
        }
        
        if (should_keep) {
            valid_connections.push_back(entry);
        }
    }
    
    // Replace connections with cleaned list
    mConnections = fl::move(valid_connections);
}

bool SimpleConnectionPool::is_connection_valid(const ConnectionEntry& entry) {
    return entry.socket && entry.socket->is_connected();
}

fl::shared_ptr<Socket> SimpleConnectionPool::create_new_connection(const fl::string& host, int port) {
    SocketOptions socket_options;
    socket_options.connect_timeout_ms = 5000;
    socket_options.enable_keepalive = mOptions.enable_keepalive;
    
    auto socket = SocketFactory::create_client_socket(socket_options);
    if (!socket) {
        return nullptr;
    }
    
    // Connect to host
    auto connect_result = socket->connect(host, port);
    // Note: In real implementation, we'd wait for the future to complete
    // For now, assume synchronous connection
    
    if (socket->is_connected()) {
        return socket;
    }
    
    return nullptr;
}

// ========== TransportFactory Implementation ==========

fl::shared_ptr<Transport> TransportFactory::create_for_scheme(const fl::string& scheme) {
    if (scheme == "http") {
        return create_tcp_transport();
    } else if (scheme == "https") {
        return create_tls_transport();
    }
    
    // Check registered transports
    auto& registry = get_transport_registry();
    auto it = registry.find(scheme);
    if (it != registry.end()) {
        // Use (*it).second instead of it->second to avoid HashMap iterator operator->() issue
        return (*it).second();
    }
    
    return nullptr;
}

fl::shared_ptr<Transport> TransportFactory::create_tcp_transport() {
    // Forward declaration - will be implemented in http_tcp_transport.cpp
    extern fl::shared_ptr<Transport> create_tcp_transport_impl();
    return create_tcp_transport_impl();
}

fl::shared_ptr<Transport> TransportFactory::create_tls_transport() {
    // Forward declaration - will be implemented in http_tls_transport.cpp
    extern fl::shared_ptr<Transport> create_tls_transport_impl();
    return create_tls_transport_impl();
}

void TransportFactory::register_transport(const fl::string& scheme, TransportCreator creator) {
    auto& registry = get_transport_registry();
    registry[scheme] = creator;
}

bool TransportFactory::is_scheme_supported(const fl::string& scheme) {
    if (scheme == "http" || scheme == "https") {
        return true;
    }
    
    auto& registry = get_transport_registry();
    return registry.find(scheme) != registry.end();
}

fl::vector<fl::string> TransportFactory::get_supported_schemes() {
    fl::vector<fl::string> schemes;
    schemes.push_back("http");
    schemes.push_back("https");
    
    auto& registry = get_transport_registry();
    for (const auto& pair : registry) {
        schemes.push_back(pair.first);
    }
    
    return schemes;
}

fl::hash_map<fl::string, TransportFactory::TransportCreator>& TransportFactory::get_transport_registry() {
    static fl::hash_map<fl::string, TransportCreator> registry;
    return registry;
}

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
