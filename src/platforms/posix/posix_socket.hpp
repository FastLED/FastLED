#pragma once

#ifdef FASTLED_HAS_NETWORKING
#if !defined(_WIN32) && !defined(FASTLED_STUB_IMPL)

#include "fl/str.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/ioctl.h>

namespace fl {

// Forward declarations for socket types
enum class SocketError;

// Platform-neutral socket handle type
using socket_handle_t = int;
static constexpr socket_handle_t INVALID_SOCKET_HANDLE = -1;

// POSIX socket type definitions
using socket_t = int;
static const socket_t INVALID_SOCKET_VALUE = -1;
static const int SOCKET_ERROR_VALUE = -1;

// Platform-specific helper functions
inline socket_t to_platform_socket(socket_handle_t handle) {
    return (handle == INVALID_SOCKET_HANDLE) ? INVALID_SOCKET_VALUE : static_cast<socket_t>(handle);
}

inline socket_handle_t from_platform_socket(socket_t sock) {
    return (sock == INVALID_SOCKET_VALUE) ? INVALID_SOCKET_HANDLE : static_cast<socket_handle_t>(sock);
}

inline bool platform_initialize_networking() {
    // POSIX doesn't require explicit network initialization
    return true;
}

inline void platform_cleanup_networking() {
    // POSIX doesn't require explicit network cleanup
}

inline fl::string platform_get_socket_error_string(int error_code) {
    return fl::string(strerror(error_code));
}

inline SocketError platform_translate_socket_error(int error_code) {
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

inline int platform_get_last_socket_error() {
    return errno;
}

inline bool platform_would_block(int error_code) {
    return error_code == EAGAIN || error_code == EWOULDBLOCK;
}

inline socket_t platform_create_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

inline int platform_connect_socket(socket_t sock, const sockaddr* addr, int addr_len) {
    return ::connect(sock, addr, addr_len);
}

inline int platform_send_data(socket_t sock, const char* data, int len) {
    return send(sock, data, len, 0);
}

inline int platform_recv_data(socket_t sock, char* buffer, int len) {
    return recv(sock, buffer, len, 0);
}

inline void platform_close_socket(socket_t sock) {
    ::close(sock);
}

inline bool platform_set_socket_timeout(socket_t sock, fl::u32 timeout_ms) {
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    bool recv_ok = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
    bool send_ok = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
    return recv_ok && send_ok;
}

inline bool platform_set_socket_non_blocking(socket_t sock, bool non_blocking) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    return fcntl(sock, F_SETFL, flags) == 0;
}

inline fl::size platform_get_available_bytes(socket_t sock) {
    int bytes_available = 0;
    if (ioctl(sock, FIONREAD, &bytes_available) == 0) {
        return static_cast<fl::size>(bytes_available);
    }
    return 0;
}

inline bool platform_set_socket_option(socket_t sock, int level, int option, 
                                       const void* value, fl::size value_size) {
    return setsockopt(sock, level, option, value, static_cast<socklen_t>(value_size)) == 0;
}

inline bool platform_get_socket_option(socket_t sock, int level, int option, 
                                       void* value, fl::size* value_size) {
    socklen_t size = static_cast<socklen_t>(*value_size);
    bool result = getsockopt(sock, level, option, value, &size) == 0;
    *value_size = static_cast<fl::size>(size);
    return result;
}

inline bool platform_inet_pton(const char* src, void* dst) {
    return inet_pton(AF_INET, src, dst) == 1;
}

//=============================================================================
// Server socket platform functions
//=============================================================================

inline socket_t platform_create_server_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

inline SocketError platform_bind_server_socket(socket_handle_t handle, const fl::string& address, int port) {
    socket_t sock = to_platform_socket(handle);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return SocketError::INVALID_ADDRESS;
    }
    
    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        return platform_translate_socket_error(errno);
    }
    
    return SocketError::SUCCESS;
}

inline SocketError platform_listen_server_socket(socket_handle_t handle, int backlog) {
    socket_t sock = to_platform_socket(handle);
    if (::listen(sock, backlog) == -1) {
        return platform_translate_socket_error(errno);
    }
    return SocketError::SUCCESS;
}

inline socket_handle_t platform_accept_connection(socket_handle_t server_handle) {
    socket_t server_sock = to_platform_socket(server_handle);
    sockaddr_in client_addr = {};
    socklen_t addr_len = sizeof(client_addr);
    
    socket_t client_sock = ::accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    return from_platform_socket(client_sock);
}

inline void platform_close_server_socket(socket_handle_t handle) {
    socket_t sock = to_platform_socket(handle);
    if (sock != INVALID_SOCKET_VALUE) {
        ::close(sock);
    }
}

inline bool platform_set_server_socket_reuse_address(socket_handle_t handle, bool enable) {
    socket_t sock = to_platform_socket(handle);
    int optval = enable ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == 0;
}

inline bool platform_set_server_socket_reuse_port(socket_handle_t handle, bool enable) {
#ifdef SO_REUSEPORT
    socket_t sock = to_platform_socket(handle);
    int optval = enable ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == 0;
#else
    // SO_REUSEPORT not available on this platform
    (void)handle;
    (void)enable;
    return false;
#endif
}

inline bool platform_set_server_socket_non_blocking(socket_handle_t handle, bool non_blocking) {
    socket_t sock = to_platform_socket(handle);
    return platform_set_socket_non_blocking(sock, non_blocking);
}

inline bool platform_server_socket_has_pending_connections(socket_handle_t handle) {
    socket_t sock = to_platform_socket(handle);
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    
    struct timeval timeout = {0, 0};  // Non-blocking check
    int result = select(sock + 1, &read_fds, nullptr, nullptr, &timeout);
    return result > 0 && FD_ISSET(sock, &read_fds);
}

inline fl::size platform_get_server_socket_pending_count(socket_handle_t handle) {
    // POSIX doesn't provide a direct way to get pending connection count
    // Return 1 if there are pending connections, 0 otherwise
    return platform_server_socket_has_pending_connections(handle) ? 1 : 0;
}

inline fl::string platform_get_server_socket_bound_address(socket_handle_t handle) {
    socket_t sock = to_platform_socket(handle);
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        char addr_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr.sin_addr, addr_str, sizeof(addr_str))) {
            return fl::string(addr_str);
        }
    }
    return fl::string("0.0.0.0");
}

inline int platform_get_server_socket_bound_port(socket_handle_t handle) {
    socket_t sock = to_platform_socket(handle);
    sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

} // namespace fl

#endif // !defined(_WIN32) && !defined(FASTLED_STUB_IMPL)
#endif // FASTLED_HAS_NETWORKING 
