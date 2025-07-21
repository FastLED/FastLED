#pragma once

#ifdef FASTLED_HAS_NETWORKING
#ifndef _WIN32

#include "real_socket.h"
#include "fl/str.h"
#include <stdio.h>   // For snprintf
#include <string.h>  // For strerror

// Unix-style includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

namespace fl {

// Unix socket type definitions
using socket_t = int;
static const socket_t INVALID_SOCKET_VALUE = -1;
static const int SOCKET_ERROR_VALUE = -1;

// Note: Unix uses 'close' natively - no macros needed

// Platform-specific helper functions
inline socket_t to_platform_socket(socket_handle_t handle) {
    return static_cast<socket_t>(handle);
}

inline socket_handle_t from_platform_socket(socket_t sock) {
    return static_cast<socket_handle_t>(sock);
}

inline bool platform_initialize_networking() {
    // Unix systems don't need special networking initialization
    return true;
}

inline void platform_cleanup_networking() {
    // Unix systems don't need special networking cleanup
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
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
    if (flags == -1) {
        return false;
    }
    
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
    return setsockopt(sock, level, option, reinterpret_cast<const char*>(value), 
                      static_cast<socklen_t>(value_size)) == 0;
}

inline bool platform_get_socket_option(socket_t sock, int level, int option, 
                                       void* value, fl::size* value_size) {
    socklen_t size = static_cast<socklen_t>(*value_size);
    bool result = getsockopt(sock, level, option, reinterpret_cast<char*>(value), &size) == 0;
    *value_size = static_cast<fl::size>(size);
    return result;
}

inline bool platform_inet_pton(const char* src, void* dst) {
    return inet_aton(src, reinterpret_cast<struct in_addr*>(dst)) != 0;
}

} // namespace fl

#endif // !_WIN32
#endif // FASTLED_HAS_NETWORKING 
