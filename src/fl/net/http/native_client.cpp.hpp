#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

// Platform-specific socket includes (provides normalized POSIX API)
#ifdef FL_IS_WIN
    #include "platforms/win/socket_win.h"  // ok platform headers  // IWYU pragma: keep
#else
    #include "platforms/posix/socket_posix.h"  // ok platform headers  // IWYU pragma: keep
#endif

// Now include FastLED headers
#include "fl/net/http/native_client.h"

// Ensure MSG_NOSIGNAL is available (not defined on Windows/macOS)
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace fl {

namespace {
    // Platform socket wrappers - avoids name conflicts with class members
    // On Windows: delegates to fl:: wrappers from socket_win.h
    // On POSIX: delegates to :: system calls from socket_posix.h
    ssize_t platform_client_send(int fd, const void* buf, size_t len, int flags) {
        #ifdef FL_IS_WIN
        return fl::send(fd, buf, len, flags);
        #else
        return ::send(fd, buf, len, flags);
        #endif
    }

    ssize_t platform_client_recv(int fd, void* buf, size_t len, int flags) {
        #ifdef FL_IS_WIN
        return fl::recv(fd, buf, len, flags);
        #else
        return ::recv(fd, buf, len, flags);
        #endif
    }

    int platform_client_connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
        #ifdef FL_IS_WIN
        return fl::connect(fd, addr, addrlen);
        #else
        return ::connect(fd, addr, addrlen);
        #endif
    }

    int platform_client_close(int fd) {
        #ifdef FL_IS_WIN
        return fl::close(fd);
        #else
        return ::close(fd);
        #endif
    }

    // Helper: Set socket to non-blocking mode
    bool set_client_socket_nonblocking(int fd, bool enabled) {
        #ifdef FL_IS_WIN
        u_long mode = enabled ? 1 : 0;
        return ioctlsocket(fd, FIONBIO, &mode) == 0;
        #else
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) return false;
        if (enabled) {
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
        } else {
            return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != -1;
        }
        #endif
    }

    // Helper: Check if socket error indicates "would block" (non-blocking)
    bool is_client_error_would_block() {
        #ifdef FL_IS_WIN
        int err = WSAGetLastError();
        return err == WSAEWOULDBLOCK;
        #else
        return errno == EWOULDBLOCK || errno == EAGAIN;
        #endif
    }

    // Helper: Check if socket error indicates "connect in progress" (non-blocking)
    bool is_client_error_in_progress() {
        #ifdef FL_IS_WIN
        int err = WSAGetLastError();
        return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
        #else
        return errno == EWOULDBLOCK || errno == EINPROGRESS;
        #endif
    }
}

NativeHttpClient::NativeHttpClient(const string& host, u16 port, const ConnectionConfig& config)
    : mHost(host)
    , mPort(port)
    , mSocket(-1)
    , mConnection(config)
{
}

NativeHttpClient::~NativeHttpClient() {
    disconnect();
}

bool NativeHttpClient::connect() {
    if (mConnection.getState() == ConnectionState::CLOSED) {
        return false;  // Permanently closed
    }

    // If already connected, return true
    if (isConnected()) {
        return true;
    }

    // Notify connection state machine
    mConnection.connect();

    // Attempt platform-specific connection
    if (platformConnect()) {
        mConnection.onConnected(0);  // TODO: pass actual currentTimeMs
        return true;
    }

    // Connection failed
    mConnection.onDisconnected();
    return false;
}

void NativeHttpClient::disconnect() {
    if (mSocket != -1) {
        platformDisconnect();
        mConnection.disconnect();
    }
}

void NativeHttpClient::close() {
    disconnect();
    mConnection.close();
}

bool NativeHttpClient::isConnected() const {
    return mConnection.isConnected() && isSocketConnected();
}

ConnectionState NativeHttpClient::getState() const {
    return mConnection.getState();
}

int NativeHttpClient::send(fl::span<const u8> data) {
    if (!isConnected() || mSocket == -1) {
        return -1;
    }

    ssize_t result = platform_client_send(mSocket, (const char*)data.data(), data.size(), MSG_NOSIGNAL);

    if (result < 0) {
        // Connection error
        mConnection.onDisconnected();
        return -1;
    }

    return static_cast<int>(result);
}

int NativeHttpClient::recv(fl::span<u8> buffer) {
    if (!isConnected() || mSocket == -1) {
        return -1;
    }

    ssize_t result = platform_client_recv(mSocket, (char*)buffer.data(), buffer.size(), 0);

    if (result < 0) {
        if (is_client_error_would_block()) {
            return 0;  // Non-blocking socket, no data available
        }

        // Connection error
        mConnection.onDisconnected();
        return -1;
    } else if (result == 0) {
        // Connection closed by peer
        mConnection.onDisconnected();
        return -1;
    }

    // Data received, update heartbeat tracking
    mConnection.onHeartbeatReceived();

    return static_cast<int>(result);
}

void NativeHttpClient::update(u32 currentTimeMs) {
    // Update connection state machine
    mConnection.update(currentTimeMs);

    // Check if we should reconnect
    if (mConnection.shouldReconnect()) {
        // Attempt reconnection
        connect();
    }

    // Check if connection was lost
    if (!isConnected() && mSocket != -1) {
        disconnect();
    }
}

bool NativeHttpClient::shouldSendHeartbeat(u32 currentTimeMs) const {
    return mConnection.shouldSendHeartbeat(currentTimeMs);
}

void NativeHttpClient::onHeartbeatSent() {
    mConnection.onHeartbeatSent();
}

void NativeHttpClient::onHeartbeatReceived() {
    mConnection.onHeartbeatReceived();
}

u32 NativeHttpClient::getReconnectDelayMs() const {
    return mConnection.getReconnectDelayMs();
}

u32 NativeHttpClient::getReconnectAttempts() const {
    return mConnection.getReconnectAttempts();
}

bool NativeHttpClient::platformConnect() {
    // Clean up existing socket
    if (mSocket != -1) {
        platformDisconnect();
    }

    // Ensure platform socket layer is initialized (needed before getaddrinfo)
    #ifdef FL_IS_WIN
    if (!initialize_winsock()) {
        return false;
    }
    #endif

    // Resolve hostname
    struct addrinfo hints{};
    struct addrinfo* result = nullptr;

    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // Convert port to string
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%u", mPort);

    int ret = getaddrinfo(mHost.c_str(), portStr, &hints, &result);
    if (ret != 0 || result == nullptr) {
        return false;
    }

    // Try to connect to first address
    bool connected = false;
    for (struct addrinfo* addr = result; addr != nullptr; addr = addr->ai_next) {
        // Create socket
        int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock < 0) {
            continue;
        }
        mSocket = sock;

        // Always set non-blocking — blocking is never appropriate on embedded
        set_client_socket_nonblocking(mSocket, true);

        // Attempt connection
        ret = platform_client_connect(mSocket, addr->ai_addr, static_cast<socklen_t>(addr->ai_addrlen));

        if (ret == 0) {
            // Connection successful
            connected = true;
            break;
        }

        if (is_client_error_in_progress()) {
            // Non-blocking connect in progress
            connected = true;
            break;
        }

        // Connection failed, close socket and try next address
        platform_client_close(mSocket);
        mSocket = -1;
    }

    freeaddrinfo(result);

    if (!connected) {
        mSocket = -1;
        return false;
    }

    return true;
}

void NativeHttpClient::platformDisconnect() {
    if (mSocket != -1) {
        platform_client_close(mSocket);
        mSocket = -1;
    }
}

bool NativeHttpClient::isSocketConnected() const {
    if (mSocket == -1) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    socklen_t len = sizeof(error);
    int ret = getsockopt(mSocket, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
