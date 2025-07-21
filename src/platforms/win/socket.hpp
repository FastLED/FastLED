#pragma once

#ifdef FASTLED_HAS_NETWORKING
#ifdef _WIN32

#include "real_socket.h"
#include "fl/str.h"
#include <stdio.h>   // For snprintf
#include <string.h>  // For strerror

// Include Windows base headers first with strict isolation
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMSG
#define NOMSG
#endif
#ifndef NOWINSTYLES
#define NOWINSTYLES
#endif
#ifndef NOSYSMETRICS
#define NOSYSMETRICS
#endif
#ifndef NOCLIPBOARD
#define NOCLIPBOARD
#endif
#ifndef NOCOLOR
#define NOCOLOR
#endif
#ifndef NOKERNEL
#define NOKERNEL
#endif
#ifndef NONLS
#define NONLS
#endif
#ifndef NOMEMMGR
#define NOMEMMGR
#endif
#ifndef NOMETAFILE
#define NOMETAFILE
#endif
#ifndef NOOPENFILE
#define NOOPENFILE
#endif
#ifndef NOSCROLL
#define NOSCROLL
#endif
#ifndef NOTEXTMETRIC
#define NOTEXTMETRIC
#endif
#ifndef NOWH
#define NOWH
#endif
#ifndef NOWINOFFSETS
#define NOWINOFFSETS
#endif
#ifndef NOKANJI
#define NOKANJI
#endif
#ifndef NOICONS
#define NOICONS
#endif
#ifndef NORASTEROPS
#define NORASTEROPS
#endif
#ifndef NOSHOWWINDOW
#define NOSHOWWINDOW
#endif
#ifndef OEMRESOURCE
#define OEMRESOURCE
#endif
#ifndef NOATOM
#define NOATOM
#endif
#ifndef NOCTLMGR
#define NOCTLMGR
#endif
#ifndef NODRAWTEXT
#define NODRAWTEXT
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>

// Windows doesn't define TCP_NODELAY in a separate header
#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif

namespace fl {

// Windows socket type definitions
using socket_t = SOCKET;
static const socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
static const int SOCKET_ERROR_VALUE = SOCKET_ERROR;

// Note: No macro redefinition needed - we use the native function names directly

// Platform-specific helper functions
inline socket_t to_platform_socket(socket_handle_t handle) {
    return (handle == INVALID_SOCKET_HANDLE) ? INVALID_SOCKET_VALUE : static_cast<socket_t>(handle);
}

inline socket_handle_t from_platform_socket(socket_t sock) {
    return (sock == INVALID_SOCKET_VALUE) ? INVALID_SOCKET_HANDLE : static_cast<socket_handle_t>(sock);
}

inline bool platform_initialize_networking() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
}

inline void platform_cleanup_networking() {
    WSACleanup();
}

inline fl::string platform_get_socket_error_string(int error_code) {
    char buffer[256];
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code, 0, 
                       buffer, sizeof(buffer), nullptr)) {
        return fl::string(buffer);
    }
    // Simple integer to string conversion
    char error_str[16];
    snprintf(error_str, sizeof(error_str), "%d", error_code);
    return fl::string("Unknown error ") + fl::string(error_str);
}

inline SocketError platform_translate_socket_error(int error_code) {
    switch (error_code) {
        case WSAECONNREFUSED: return SocketError::CONNECTION_REFUSED;
        case WSAETIMEDOUT: return SocketError::CONNECTION_TIMEOUT;
        case WSAENETUNREACH: return SocketError::NETWORK_UNREACHABLE;
        case WSAEACCES: return SocketError::PERMISSION_DENIED;
        case WSAEADDRINUSE: return SocketError::ADDRESS_IN_USE;
        case WSAEINVAL: return SocketError::INVALID_ADDRESS;
        default: return SocketError::UNKNOWN_ERROR;
    }
}

inline int platform_get_last_socket_error() {
    return WSAGetLastError();
}

inline bool platform_would_block(int error_code) {
    return error_code == WSAEWOULDBLOCK;
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
    ::closesocket(sock);
}

inline bool platform_set_socket_timeout(socket_t sock, fl::u32 timeout_ms) {
    DWORD timeout = timeout_ms;
    bool recv_ok = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, 
                             reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
    bool send_ok = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, 
                             reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
    return recv_ok && send_ok;
}

inline bool platform_set_socket_non_blocking(socket_t sock, bool non_blocking) {
    u_long mode = non_blocking ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

inline fl::size platform_get_available_bytes(socket_t sock) {
    u_long bytes_available = 0;
    if (ioctlsocket(sock, FIONREAD, &bytes_available) == 0) {
        return static_cast<fl::size>(bytes_available);
    }
    return 0;
}

inline bool platform_set_socket_option(socket_t sock, int level, int option, 
                                       const void* value, fl::size value_size) {
    return setsockopt(sock, level, option, reinterpret_cast<const char*>(value), 
                      static_cast<int>(value_size)) == 0;
}

inline bool platform_get_socket_option(socket_t sock, int level, int option, 
                                       void* value, fl::size* value_size) {
    int size = static_cast<int>(*value_size);
    bool result = getsockopt(sock, level, option, reinterpret_cast<char*>(value), &size) == 0;
    *value_size = static_cast<fl::size>(size);
    return result;
}

inline bool platform_inet_pton(const char* src, void* dst) {
    return inet_pton(AF_INET, src, dst) == 1;
}

} // namespace fl

#endif // _WIN32
#endif // FASTLED_HAS_NETWORKING 
