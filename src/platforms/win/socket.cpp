#if defined(FASTLED_HAS_NETWORKING) && defined(_WIN32) && !defined(FASTLED_STUB_IMPL)

#include "socket.h"
#include "socket_impl.hpp"
#include "fl/str.h"

// Standard includes
#include <stdio.h>   // For snprintf
#include <string.h>  // For strerror

namespace fl {

// Static member initialization
bool WinSocket::s_networking_initialized = false;
int WinSocket::s_instance_count = 0;
fl::mutex WinSocket::s_init_mutex;

//=============================================================================
// Static Platform Helpers
//=============================================================================

bool WinSocket::initialize_networking() {
    fl::lock_guard<fl::mutex> lock(s_init_mutex);
    
    if (s_networking_initialized) {
        s_instance_count++;
        return true;
    }
    
    if (!platform_initialize_networking()) {
        return false;
    }
    
    s_networking_initialized = true;
    s_instance_count = 1;
    return true;
}

void WinSocket::cleanup_networking() {
    fl::lock_guard<fl::mutex> lock(s_init_mutex);
    
    s_instance_count--;
    if (s_instance_count <= 0 && s_networking_initialized) {
        platform_cleanup_networking();
        s_networking_initialized = false;
        s_instance_count = 0;
    }
}

fl::string WinSocket::get_socket_error_string(int error_code) {
    return platform_get_socket_error_string(error_code);
}

SocketError WinSocket::translate_socket_error(int error_code) {
    return platform_translate_socket_error(error_code);
}

//=============================================================================
// WinSocket Implementation
//=============================================================================

WinSocket::WinSocket(const SocketOptions& options) 
    : mOptions(options), mTimeout(options.read_timeout_ms) {
    
    if (!initialize_networking()) {
        set_error(SocketError::UNKNOWN_ERROR, "Failed to initialize networking");
        return;
    }
    
    // Create socket
    socket_t platform_socket = platform_create_socket();
    mSocket = from_platform_socket(platform_socket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        int error = platform_get_last_socket_error();
        set_error(translate_socket_error(error), get_socket_error_string(error));
        return;
    }
    
    setup_socket_options();
}

WinSocket::~WinSocket() {
    disconnect();
    cleanup_networking();
}

fl::future<SocketError> WinSocket::connect(const fl::string& host, int port) {
    auto error = connect_internal(host, port);
    return fl::make_ready_future(error);
}

fl::future<SocketError> WinSocket::connect_async(const fl::string& host, int port) {
    // For now, same as synchronous connect
    return connect(host, port);
}

SocketError WinSocket::connect_internal(const fl::string& host, int port) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return SocketError::UNKNOWN_ERROR;
    }
    
    if (is_connected()) {
        return SocketError::SUCCESS;
    }
    
    set_state(SocketState::CONNECTING);
    
    // Resolve hostname to IP address
    fl::string ip_address = resolve_hostname(host);
    if (ip_address.empty()) {
        set_error(SocketError::INVALID_ADDRESS, "Failed to resolve hostname: " + host);
        return mLastError;
    }
    
    // Setup address structure
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (!platform_inet_pton(ip_address.c_str(), &addr.sin_addr)) {
        set_error(SocketError::INVALID_ADDRESS, "Invalid IP address: " + ip_address);
        return mLastError;
    }
    
    // Connect to server
    int result = platform_connect_socket(platform_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result == SOCKET_ERROR_VALUE) {
        int error = platform_get_last_socket_error();
        set_error(translate_socket_error(error), get_socket_error_string(error));
        return mLastError;
    }
    
    mRemoteHost = host;
    mRemotePort = port;
    set_state(SocketState::CONNECTED);
    
    return SocketError::SUCCESS;
}

void WinSocket::disconnect() {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket != INVALID_SOCKET_VALUE) {
        platform_close_socket(platform_socket);
        mSocket = INVALID_SOCKET_HANDLE;
    }
    
    set_state(SocketState::CLOSED);
    mRemoteHost.clear();
    mRemotePort = 0;
}

bool WinSocket::is_connected() const {
    return mState == SocketState::CONNECTED && mSocket != INVALID_SOCKET_HANDLE;
}

SocketState WinSocket::get_state() const {
    return mState;
}

fl::size WinSocket::read(fl::span<fl::u8> buffer) {
    if (!is_connected() || buffer.empty()) {
        return 0;
    }
    
    socket_t platform_socket = to_platform_socket(mSocket);
    int result = platform_recv_data(platform_socket, reinterpret_cast<char*>(buffer.data()), 
                                   static_cast<int>(buffer.size()));
    
    if (result == SOCKET_ERROR_VALUE) {
        int error = platform_get_last_socket_error();
        if (platform_would_block(error)) {
            return 0;  // No data available (non-blocking)
        }
        set_error(translate_socket_error(error), get_socket_error_string(error));
        return 0;
    }
    
    if (result == 0) {
        // Connection closed by peer
        set_state(SocketState::CLOSED);
        return 0;
    }
    
    return static_cast<fl::size>(result);
}

fl::size WinSocket::write(fl::span<const fl::u8> data) {
    if (!is_connected() || data.empty()) {
        return 0;
    }
    
    socket_t platform_socket = to_platform_socket(mSocket);
    int result = platform_send_data(platform_socket, reinterpret_cast<const char*>(data.data()), 
                                   static_cast<int>(data.size()));
    
    if (result == SOCKET_ERROR_VALUE) {
        int error = platform_get_last_socket_error();
        if (platform_would_block(error)) {
            return 0;  // Socket buffer full (non-blocking)
        }
        set_error(translate_socket_error(error), get_socket_error_string(error));
        return 0;
    }
    
    return static_cast<fl::size>(result);
}

fl::size WinSocket::available() const {
    if (!is_connected()) {
        return 0;
    }
    
    socket_t platform_socket = to_platform_socket(mSocket);
    return platform_get_available_bytes(platform_socket);
}

void WinSocket::flush() {
    // TCP sockets don't require explicit flushing
}

bool WinSocket::has_data_available() const {
    return available() > 0;
}

bool WinSocket::can_write() const {
    return is_connected();
}

void WinSocket::set_non_blocking(bool non_blocking) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return;
    }
    
    platform_set_socket_non_blocking(platform_socket, non_blocking);
    mIsNonBlocking = non_blocking;
}

bool WinSocket::is_non_blocking() const {
    return mIsNonBlocking;
}

void WinSocket::set_timeout(fl::u32 timeout_ms) {
    mTimeout = timeout_ms;
    
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return;
    }
    
    platform_set_socket_timeout(platform_socket, timeout_ms);
}

fl::u32 WinSocket::get_timeout() const {
    return mTimeout;
}

void WinSocket::set_keep_alive(bool enable) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return;
    }
    
    int value = enable ? 1 : 0;
    platform_set_socket_option(platform_socket, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
}

void WinSocket::set_nodelay(bool enable) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return;
    }
    
    int value = enable ? 1 : 0;
    platform_set_socket_option(platform_socket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
}

fl::string WinSocket::remote_address() const {
    return mRemoteHost;
}

int WinSocket::remote_port() const {
    return mRemotePort;
}

fl::string WinSocket::local_address() const {
    return mLocalAddress;
}

int WinSocket::local_port() const {
    return mLocalPort;
}

SocketError WinSocket::get_last_error() const {
    return mLastError;
}

fl::string WinSocket::get_error_message() const {
    return mErrorMessage;
}

bool WinSocket::set_socket_option(int level, int option, const void* value, fl::size value_size) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return false;
    }
    
    return platform_set_socket_option(platform_socket, level, option, value, value_size);
}

bool WinSocket::get_socket_option(int level, int option, void* value, fl::size* value_size) {
    socket_t platform_socket = to_platform_socket(mSocket);
    if (platform_socket == INVALID_SOCKET_VALUE) {
        return false;
    }
    
    return platform_get_socket_option(platform_socket, level, option, value, value_size);
}

int WinSocket::get_socket_handle() const {
    return static_cast<int>(mSocket);
}

void WinSocket::set_state(SocketState state) {
    mState = state;
}

void WinSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
    if (error != SocketError::SUCCESS) {
        set_state(SocketState::ERROR);
    }
}

bool WinSocket::setup_socket_options() {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    
    // Set timeout
    set_timeout(mTimeout);
    
    // Set keep alive if enabled
    if (mOptions.enable_keepalive) {
        set_keep_alive(true);
    }
    
    // Set no delay if enabled
    if (mOptions.enable_nodelay) {
        set_nodelay(true);
    }
    
    return true;
}

fl::string WinSocket::resolve_hostname(const fl::string& hostname) {
    // Check if it's already an IP address
    struct sockaddr_in sa;
    if (platform_inet_pton(hostname.c_str(), &(sa.sin_addr))) {
        return hostname;  // Already an IP address
    }
    
    // Resolve hostname
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo* result = nullptr;
    int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    
    if (status != 0 || result == nullptr) {
        if (result) {
            freeaddrinfo(result);
        }
        return fl::string();
    }
    
    // Extract IP address
    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
    
    freeaddrinfo(result);
    return fl::string(ip_str);
}

//=============================================================================
// WinServerSocket Implementation  
//=============================================================================

WinServerSocket::WinServerSocket(const SocketOptions& options) : mOptions(options) {
    // Server socket implementation would go here
    // For now, just basic initialization
}

WinServerSocket::~WinServerSocket() {
    this->close();
}

SocketError WinServerSocket::bind(const fl::string& address, int port) {
    // Server socket bind implementation
    mBoundAddress = address;
    mBoundPort = port;
    return SocketError::SUCCESS;
}

SocketError WinServerSocket::listen(int backlog) {
    mBacklog = backlog;
    mIsListening = true;
    return SocketError::SUCCESS;
}

void WinServerSocket::close() {
    mIsListening = false;
}

bool WinServerSocket::is_listening() const {
    return mIsListening;
}

fl::shared_ptr<Socket> WinServerSocket::accept() {
    // Accept implementation would go here
    return nullptr;
}

fl::vector<fl::shared_ptr<Socket>> WinServerSocket::accept_multiple(fl::size max_connections) {
    return fl::vector<fl::shared_ptr<Socket>>();
}

bool WinServerSocket::has_pending_connections() const {
    return false;
}

void WinServerSocket::set_reuse_address(bool enable) {
    (void)enable;
}

void WinServerSocket::set_reuse_port(bool enable) {
    (void)enable;
}

void WinServerSocket::set_non_blocking(bool non_blocking) {
    mIsNonBlocking = non_blocking;
}

fl::string WinServerSocket::bound_address() const {
    return mBoundAddress;
}

int WinServerSocket::bound_port() const {
    return mBoundPort;
}

fl::size WinServerSocket::max_connections() const {
    return static_cast<fl::size>(mBacklog);
}

fl::size WinServerSocket::current_connections() const {
    return mCurrentConnections;
}

SocketError WinServerSocket::get_last_error() const {
    return mLastError;
}

fl::string WinServerSocket::get_error_message() const {
    return mErrorMessage;
}

int WinServerSocket::get_socket_handle() const {
    return static_cast<int>(mSocket);
}

void WinServerSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
}

//=============================================================================
// Platform-specific factory functions
//=============================================================================

fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options) {
    return fl::make_shared<WinSocket>(options);
}

fl::shared_ptr<ServerSocket> create_platform_server_socket(const SocketOptions& options) {
    return fl::make_shared<WinServerSocket>(options);
}

bool platform_supports_ipv6() {
    return true;  // Windows supports IPv6
}

bool platform_supports_tls() {
    return false;  // TLS would require additional libraries (OpenSSL, etc.)
}

bool platform_supports_non_blocking_connect() {
    return true;
}

bool platform_supports_socket_reuse() {
    return true;
}

} // namespace fl

#endif // defined(FASTLED_HAS_NETWORKING) && defined(_WIN32) && !defined(FASTLED_STUB_IMPL) 
