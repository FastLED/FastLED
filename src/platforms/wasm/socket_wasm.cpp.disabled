#ifdef FASTLED_HAS_NETWORKING
// Real WASM POSIX socket implementation for Emscripten
#if defined(__EMSCRIPTEN__)

#include "socket_wasm.h"
#include "fl/str.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>  // For snprintf
#include <errno.h>

// Include real POSIX socket headers for Emscripten
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace fl {

//=============================================================================
// WASM Socket State Management (Real Implementation)
//=============================================================================

// Global state for WASM socket management
static bool g_initialized = false;

// Statistics tracking
static WasmSocketStats g_stats = {};

//=============================================================================
// Helper Functions for WASM Socket Management
//=============================================================================

static void set_socket_nonblocking_with_timeout(int sockfd, int timeout_ms) {
    // Set receive timeout using SO_RCVTIMEO since we can't use select/poll
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
}

static bool is_valid_socket_fd(int sockfd) {
    // Basic validation - real socket descriptors are typically small positive integers
    return sockfd >= 0;
}

//=============================================================================
// Core Socket Operations (Real POSIX Implementation)
//=============================================================================

int socket(int domain, int type, int protocol) {
    if (!g_initialized) {
        initialize_wasm_sockets();
    }
    
    // Call real POSIX socket function
    int sockfd = ::socket(domain, type, protocol);
    
    if (sockfd >= 0) {
        g_stats.total_sockets_created++;
        
        // Set default timeout for WASM - manual polling approach
        set_socket_nonblocking_with_timeout(sockfd, 5000);  // 5 second default
    }
    
    return sockfd;
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    // WASM/Emscripten doesn't support socketpair - return appropriate error
    errno = EAFNOSUPPORT;
    return -1;
}

//=============================================================================
// Addressing Operations (Real POSIX Implementation)
//=============================================================================

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX bind function
    return ::bind(sockfd, addr, addrlen);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    g_stats.total_connections_attempted++;
    
    // Call real POSIX connect function
    int result = ::connect(sockfd, addr, addrlen);
    
    if (result == 0) {
        // Connection successful
        g_stats.total_connections_successful++;
    }
    
    return result;
}

int listen(int sockfd, int backlog) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX listen function
    return ::listen(sockfd, backlog);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX accept function
    int client_fd = ::accept(sockfd, addr, addrlen);
    
    if (client_fd >= 0) {
        // Set default timeout for accepted connection
        set_socket_nonblocking_with_timeout(client_fd, 5000);
    }
    
    return client_fd;
}

//=============================================================================
// Data Transfer Operations (Real POSIX Implementation)
//=============================================================================

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    if (!buf && len > 0) {
        errno = EFAULT;
        return -1;
    }
    
    // Call real POSIX send function
    ssize_t result = ::send(sockfd, buf, len, flags);
    
    if (result > 0) {
        g_stats.total_bytes_sent += result;
    }
    
    return result;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    if (!buf && len > 0) {
        errno = EFAULT;
        return -1;
    }
    
    // Call real POSIX recv function
    ssize_t result = ::recv(sockfd, buf, len, flags);
    
    if (result > 0) {
        g_stats.total_bytes_received += result;
    }
    
    return result;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX sendto function
    ssize_t result = ::sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    
    if (result > 0) {
        g_stats.total_bytes_sent += result;
    }
    
    return result;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX recvfrom function
    ssize_t result = ::recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    
    if (result > 0) {
        g_stats.total_bytes_received += result;
    }
    
    return result;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    if (!is_valid_socket_fd(sockfd) || !msg) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX sendmsg function
    ssize_t result = ::sendmsg(sockfd, msg, flags);
    
    if (result > 0) {
        g_stats.total_bytes_sent += result;
    }
    
    return result;
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    if (!is_valid_socket_fd(sockfd) || !msg) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX recvmsg function
    ssize_t result = ::recvmsg(sockfd, msg, flags);
    
    if (result > 0) {
        g_stats.total_bytes_received += result;
    }
    
    return result;
}

//=============================================================================
// Connection Teardown (Real POSIX Implementation - Using shutdown, NOT close)
//=============================================================================

int shutdown(int sockfd, int how) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX shutdown function
    // This is the ONLY way to close connections in WASM since close() doesn't work
    return ::shutdown(sockfd, how);
}

int close(int fd) {
    // WASM WARNING: close() doesn't work properly for sockets due to proxying
    // We should use shutdown() instead, but this function exists for compatibility
    // with non-socket file descriptors
    
    if (is_valid_socket_fd(fd)) {
        // For sockets, use shutdown instead
        return shutdown(fd, SHUT_RDWR);
    }
    
    // For non-socket file descriptors, attempt real close
    return ::close(fd);
}

//=============================================================================
// Socket Options (Real POSIX Implementation)
//=============================================================================

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX setsockopt function
    return ::setsockopt(sockfd, level, optname, optval, optlen);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX getsockopt function
    return ::getsockopt(sockfd, level, optname, optval, optlen);
}

//=============================================================================
// Peer & Local Address Retrieval (Real POSIX Implementation)
//=============================================================================

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX getpeername function
    return ::getpeername(sockfd, addr, addrlen);
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        errno = EBADF;
        return -1;
    }
    
    // Call real POSIX getsockname function
    return ::getsockname(sockfd, addr, addrlen);
}

//=============================================================================
// Name and Service Translation (Real POSIX Implementation)
//=============================================================================

int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res) {
    // Call real POSIX getaddrinfo function
    return ::getaddrinfo(node, service, hints, res);
}

void freeaddrinfo(struct addrinfo *res) {
    // Call real POSIX freeaddrinfo function
    ::freeaddrinfo(res);
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    // Call real POSIX getnameinfo function
    return ::getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

//=============================================================================
// Address Conversion (Real POSIX Implementation)
//=============================================================================

int inet_pton(int af, const char *src, void *dst) {
    // Call real POSIX inet_pton function
    return ::inet_pton(af, src, dst);
}

const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    // Call real POSIX inet_ntop function
    return ::inet_ntop(af, src, dst, size);
}

//=============================================================================
// File Control (Real POSIX Implementation with WASM limitations)
//=============================================================================

int fcntl(int fd, int cmd, ...) {
    if (!is_valid_socket_fd(fd)) {
        errno = EBADF;
        return -1;
    }
    
    // For WASM, we support limited fcntl operations
    // Mainly for getting/setting O_NONBLOCK flag
    va_list args;
    va_start(args, cmd);
    
    int result = -1;
    
    switch (cmd) {
        case F_GETFL: {
            // Return current flags - for WASM we'll assume blocking by default
            result = 0;  // No flags set initially
            break;
        }
        case F_SETFL: {
            int flags = va_arg(args, int);
            // We use SO_RCVTIMEO/SO_SNDTIMEO for timeout behavior instead of O_NONBLOCK
            // since select/poll don't work properly in WASM
            if (flags & O_NONBLOCK) {
                // Set very short timeout to simulate non-blocking
                set_socket_nonblocking_with_timeout(fd, 1);  // 1ms timeout
            } else {
                // Set longer timeout for blocking behavior  
                set_socket_nonblocking_with_timeout(fd, 5000);  // 5s timeout
            }
            result = 0;
            break;
        }
        default:
            errno = EINVAL;
            result = -1;
            break;
    }
    
    va_end(args);
    return result;
}

//=============================================================================
// I/O Control (Real POSIX Implementation with WASM limitations)
//=============================================================================

int ioctl(int fd, unsigned long request, ...) {
    if (!is_valid_socket_fd(fd)) {
        errno = EBADF;
        return -1;
    }
    
    // For WASM, we support limited ioctl operations
    va_list args;
    va_start(args, request);
    
    int result = -1;
    
    switch (request) {
        case FIONBIO: {
            // Set non-blocking I/O mode
            int *argp = va_arg(args, int*);
            if (argp) {
                if (*argp) {
                    // Enable non-blocking mode with short timeout
                    set_socket_nonblocking_with_timeout(fd, 1);
                } else {
                    // Disable non-blocking mode with longer timeout
                    set_socket_nonblocking_with_timeout(fd, 5000);
                }
                result = 0;
            } else {
                errno = EFAULT;
            }
            break;
        }
        case FIONREAD: {
            // Get number of bytes available to read
            // In WASM, we'll try MSG_PEEK to check available data
            int *argp = va_arg(args, int*);
            if (argp) {
                char peek_buf[1];
                ssize_t peek_result = ::recv(fd, peek_buf, 1, MSG_PEEK | MSG_DONTWAIT);
                if (peek_result > 0) {
                    *argp = 1;  // At least 1 byte available
                } else if (peek_result == 0) {
                    *argp = 0;  // EOF
                } else {
                    *argp = 0;  // No data or error
                }
                result = 0;
            } else {
                errno = EFAULT;
            }
            break;
        }
        default:
            errno = EINVAL;
            result = -1;
            break;
    }
    
    va_end(args);
    return result;
}

//=============================================================================
// Error handling (Real Implementation)
//=============================================================================

int get_errno() {
    return errno;
}

//=============================================================================
// WASM-specific Helper Functions
//=============================================================================

bool initialize_wasm_sockets() {
    if (g_initialized) {
        return true;
    }
    
    // Initialize statistics
    fl::memfill(&g_stats, 0, sizeof(g_stats));
    
    g_initialized = true;
    return true;
}

void cleanup_wasm_sockets() {
    if (!g_initialized) {
        return;
    }
    
    // Nothing special to cleanup for real POSIX sockets
    g_initialized = false;
}

void set_wasm_socket_mock_behavior(bool should_fail, int error_code) {
    // Not applicable for real implementation - this was for fake/mock behavior
    (void)should_fail;
    (void)error_code;
}

WasmSocketStats get_wasm_socket_stats() {
    return g_stats;
}

void reset_wasm_socket_stats() {
    fl::memfill(&g_stats, 0, sizeof(g_stats));
}

//=============================================================================
// WASM Socket Class Implementation (Real POSIX Sockets)
//=============================================================================

WasmSocket::WasmSocket(const SocketOptions& options) 
    : mOptions(options), mTimeout(options.read_timeout_ms) {
    // Create real POSIX socket
    mSocketHandle = fl::socket(AF_INET, SOCK_STREAM, 0);
    if (mSocketHandle == -1) {
        set_error(SocketError::UNKNOWN_ERROR, "Failed to create socket");
        return;
    }
    
    setup_socket_options();
}

fl::future<SocketError> WasmSocket::connect(const fl::string& host, int port) {
    if (mSocketHandle == -1) {
        return fl::make_ready_future(SocketError::UNKNOWN_ERROR);
    }
    
    set_state(SocketState::CONNECTING);
    
    // Resolve hostname using real getaddrinfo
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;  // IPv4 for now
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo* result = nullptr;
    fl::string port_str = fl::to_string(port);
    
    int gai_result = fl::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (gai_result != 0) {
        set_error(SocketError::INVALID_ADDRESS, "Failed to resolve hostname");
        return fl::make_ready_future(SocketError::INVALID_ADDRESS);
    }
    
    // Attempt connection using real connect
    int connect_result = fl::connect(mSocketHandle, result->ai_addr, result->ai_addrlen);
    fl::freeaddrinfo(result);
    
    if (connect_result == 0) {
        // Connection successful
        mRemoteHost = host;
        mRemotePort = port;
        set_state(SocketState::CONNECTED);
        return fl::make_ready_future(SocketError::SUCCESS);
    } else {
        // Connection failed
        int error = fl::get_errno();
        SocketError socket_error = translate_errno_to_socket_error(error);
        set_error(socket_error, "Connection failed");
        set_state(SocketState::ERROR);
        return fl::make_ready_future(socket_error);
    }
}

fl::future<SocketError> WasmSocket::connect_async(const fl::string& host, int port) {
    // For WASM, async connect is the same as sync for now
    return connect(host, port);
}

void WasmSocket::disconnect() {
    if (mSocketHandle != -1) {
        // WASM: Use shutdown instead of close for sockets
        fl::shutdown(mSocketHandle, SHUT_RDWR);
        mSocketHandle = -1;
    }
    
    set_state(SocketState::CLOSED);
    mRemoteHost = "";
    mRemotePort = 0;
}

bool WasmSocket::is_connected() const {
    return mState == SocketState::CONNECTED && mSocketHandle != -1;
}

SocketState WasmSocket::get_state() const {
    return mState;
}

fl::size WasmSocket::read(fl::span<fl::u8> buffer) {
    if (!is_connected() || buffer.empty()) {
        return 0;
    }
    
    // Use real POSIX recv
    ssize_t bytes_read = fl::recv(mSocketHandle, buffer.data(), buffer.size(), 0);
    
    if (bytes_read > 0) {
        return static_cast<fl::size>(bytes_read);
    } else if (bytes_read == 0) {
        // Connection closed by peer
        set_state(SocketState::CLOSED);
        return 0;
    } else {
        // Error occurred
        int error = fl::get_errno();
        if (error == EWOULDBLOCK || error == EAGAIN) {
            // No data available (non-blocking mode)
            return 0;
        } else {
            // Real error
            SocketError socket_error = translate_errno_to_socket_error(error);
            set_error(socket_error, "Read failed");
            set_state(SocketState::ERROR);
            return 0;
        }
    }
}

fl::size WasmSocket::write(fl::span<const fl::u8> data) {
    if (!is_connected() || data.empty()) {
        return 0;
    }
    
    // Use real POSIX send
    ssize_t bytes_sent = fl::send(mSocketHandle, data.data(), data.size(), 0);
    
    if (bytes_sent > 0) {
        return static_cast<fl::size>(bytes_sent);
    } else if (bytes_sent == 0) {
        // Connection issue
        return 0;
    } else {
        // Error occurred
        int error = fl::get_errno();
        if (error == EWOULDBLOCK || error == EAGAIN) {
            // Would block (non-blocking mode)
            return 0;
        } else {
            // Real error
            SocketError socket_error = translate_errno_to_socket_error(error);
            set_error(socket_error, "Write failed");
            set_state(SocketState::ERROR);
            return 0;
        }
    }
}

fl::size WasmSocket::available() const {
    if (!is_connected()) {
        return 0;
    }
    
    // Use FIONREAD ioctl to get available bytes
    int available_bytes = 0;
    if (fl::ioctl(mSocketHandle, FIONREAD, &available_bytes) == 0) {
        return static_cast<fl::size>(available_bytes);
    }
    
    return 0;
}

void WasmSocket::flush() {
    // For TCP sockets, flush doesn't have much meaning
    // Data is sent immediately by the kernel
}

bool WasmSocket::has_data_available() const {
    return available() > 0;
}

bool WasmSocket::can_write() const {
    return is_connected();
}

void WasmSocket::set_non_blocking(bool non_blocking) {
    if (mSocketHandle == -1) {
        return;
    }
    
    // Use fcntl to set/clear O_NONBLOCK flag
    int flags = fl::fcntl(mSocketHandle, F_GETFL, 0);
    if (flags != -1) {
        if (non_blocking) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        fl::fcntl(mSocketHandle, F_SETFL, flags);
        mIsNonBlocking = non_blocking;
    }
}

bool WasmSocket::is_non_blocking() const {
    return mIsNonBlocking;
}

void WasmSocket::set_timeout(fl::u32 timeout_ms) {
    mTimeout = timeout_ms;
    
    if (mSocketHandle != -1) {
        // Set socket timeouts using SO_RCVTIMEO and SO_SNDTIMEO
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        fl::setsockopt(mSocketHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        fl::setsockopt(mSocketHandle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
}

fl::u32 WasmSocket::get_timeout() const {
    return mTimeout;
}

void WasmSocket::set_keep_alive(bool enable) {
    if (mSocketHandle != -1) {
        int optval = enable ? 1 : 0;
        fl::setsockopt(mSocketHandle, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    }
}

void WasmSocket::set_nodelay(bool enable) {
    if (mSocketHandle != -1) {
        int optval = enable ? 1 : 0;
        fl::setsockopt(mSocketHandle, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    }
}

fl::string WasmSocket::remote_address() const {
    if (mSocketHandle == -1 || !is_connected()) {
        return "";
    }
    
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (fl::getpeername(mSocketHandle, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        char addr_str[INET_ADDRSTRLEN];
        if (fl::inet_ntop(AF_INET, &addr.sin_addr, addr_str, sizeof(addr_str))) {
            return fl::string(addr_str);
        }
    }
    
    return mRemoteHost;  // Fallback to stored host
}

int WasmSocket::remote_port() const {
    if (mSocketHandle == -1 || !is_connected()) {
        return 0;
    }
    
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (fl::getpeername(mSocketHandle, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        return ntohs(addr.sin_port);
    }
    
    return mRemotePort;  // Fallback to stored port
}

fl::string WasmSocket::local_address() const {
    if (mSocketHandle == -1) {
        return "127.0.0.1";
    }
    
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (fl::getsockname(mSocketHandle, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        char addr_str[INET_ADDRSTRLEN];
        if (fl::inet_ntop(AF_INET, &addr.sin_addr, addr_str, sizeof(addr_str))) {
            return fl::string(addr_str);
        }
    }
    
    return mLocalAddress.empty() ? "127.0.0.1" : mLocalAddress;
}

int WasmSocket::local_port() const {
    if (mSocketHandle == -1) {
        return 0;
    }
    
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (fl::getsockname(mSocketHandle, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        return ntohs(addr.sin_port);
    }
    
    return mLocalPort;
}

SocketError WasmSocket::get_last_error() const {
    return mLastError;
}

fl::string WasmSocket::get_error_message() const {
    return mErrorMessage;
}

bool WasmSocket::set_socket_option(int level, int option, const void* value, fl::size value_size) {
    if (mSocketHandle == -1 || !value) {
        return false;
    }
    
    return fl::setsockopt(mSocketHandle, level, option, value, static_cast<socklen_t>(value_size)) == 0;
}

bool WasmSocket::get_socket_option(int level, int option, void* value, fl::size* value_size) {
    if (mSocketHandle == -1 || !value || !value_size) {
        return false;
    }
    
    socklen_t len = static_cast<socklen_t>(*value_size);
    int result = fl::getsockopt(mSocketHandle, level, option, value, &len);
    *value_size = static_cast<fl::size>(len);
    
    return result == 0;
}

int WasmSocket::get_socket_handle() const {
    return mSocketHandle;
}

void WasmSocket::set_state(SocketState state) {
    mState = state;
}

void WasmSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
}

SocketError WasmSocket::translate_errno_to_socket_error(int error_code) {
    switch (error_code) {
        case ECONNREFUSED: return SocketError::CONNECTION_REFUSED;
        case ETIMEDOUT: return SocketError::CONNECTION_TIMEOUT;
        case ENETUNREACH: return SocketError::NETWORK_UNREACHABLE;
        case EACCES: return SocketError::PERMISSION_DENIED;
        case EADDRINUSE: return SocketError::ADDRESS_IN_USE;
        case EINVAL: return SocketError::INVALID_ADDRESS;
        default: return SocketError::UNKNOWN_ERROR;
    }
}

bool WasmSocket::setup_socket_options() {
    if (mSocketHandle == -1) {
        return false;
    }
    
    // Set default socket options based on SocketOptions
    if (mOptions.enable_keepalive) {
        set_keep_alive(true);
    }
    
    if (mOptions.enable_nodelay) {
        set_nodelay(true);
    }
    
    // Set timeouts
    set_timeout(mTimeout);
    
    return true;
}

//=============================================================================
// Platform-specific functions (required by socket_factory.cpp)
//=============================================================================

fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options) {
    return fl::make_shared<WasmSocket>(options);
}

bool platform_supports_ipv6() {
    // WASM/Emscripten supports IPv6 through the browser
    return true;
}

bool platform_supports_tls() {
    // TLS would need to be implemented at a higher level for WASM
    return false;
}

bool platform_supports_non_blocking_connect() {
    // WASM supports non-blocking operations through timeouts
    return true;
}

bool platform_supports_socket_reuse() {
    // WASM supports SO_REUSEADDR through Emscripten
    return true;
}

} // namespace fl

#endif // __EMSCRIPTEN__
#endif // FASTLED_HAS_NETWORKING 
