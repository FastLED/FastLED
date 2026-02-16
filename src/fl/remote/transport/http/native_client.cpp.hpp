#pragma once

// Platform-specific socket includes MUST come first to avoid conflicts
#ifdef FL_IS_WIN
    // Define these BEFORE including any Windows headers
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
    #define NOUSER
    #include <winsock2.h>  // IWYU pragma: keep
    #include <ws2tcpip.h>  // IWYU pragma: keep
    // Now undefine conflicting macros from Arduino
    #ifdef INPUT
        #undef INPUT
    #endif
    #ifdef OUTPUT
        #undef OUTPUT
    #endif
    #ifdef INPUT_PULLUP
        #undef INPUT_PULLUP
    #endif
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>  // IWYU pragma: keep
    #include <netinet/in.h>  // IWYU pragma: keep
    #include <arpa/inet.h>  // IWYU pragma: keep
    #include <unistd.h>  // IWYU pragma: keep
    #include <fcntl.h>  // IWYU pragma: keep
    #include <netdb.h>  // IWYU pragma: keep
    #include <errno.h>  // IWYU pragma: keep
#endif

// Now include FastLED headers
#include "fl/remote/transport/http/native_client.h"

// Platform-specific constants
#ifdef FL_IS_WIN
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
    static const fl::SocketHandle INVALID_SOCKET_HANDLE = static_cast<fl::SocketHandle>(INVALID_SOCKET);
#else
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET ::close
    static const fl::SocketHandle INVALID_SOCKET_HANDLE = static_cast<fl::SocketHandle>(-1);
#endif

namespace fl {

namespace {
    // Global Windows socket initialization counter
    #ifdef FL_IS_WIN
    static int gWinsockInitCount = 0;
    #endif
}

NativeHttpClient::NativeHttpClient(const string& host, u16 port, const ConnectionConfig& config)
    : mHost(host)
    , mPort(port)
    , mSocket(INVALID_SOCKET_HANDLE)
    , mNonBlocking(false)
    , mConnection(config)
{
    initPlatform();
}

NativeHttpClient::~NativeHttpClient() {
    disconnect();
    cleanupPlatform();
}

void NativeHttpClient::initPlatform() {
    #ifdef FL_IS_WIN
    if (gWinsockInitCount == 0) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            // Failed to initialize Winsock
            return;
        }
    }
    gWinsockInitCount++;
    #endif
}

void NativeHttpClient::cleanupPlatform() {
    #ifdef FL_IS_WIN
    gWinsockInitCount--;
    if (gWinsockInitCount == 0) {
        WSACleanup();
    }
    #endif
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
    if (mSocket != INVALID_SOCKET_HANDLE) {
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

int NativeHttpClient::send(const u8* data, size_t len) {
    if (!isConnected() || mSocket == INVALID_SOCKET_HANDLE) {
        return -1;
    }

    #ifdef FL_IS_WIN
    int result = ::send(static_cast<SOCKET>(mSocket), reinterpret_cast<const char*>(data), static_cast<int>(len), 0); // ok reinterpret cast
    #else
    ssize_t result = ::send(mSocket, data, len, MSG_NOSIGNAL);
    #endif

    if (result < 0) {
        // Connection error
        mConnection.onDisconnected();
        return -1;
    }

    return static_cast<int>(result);
}

int NativeHttpClient::recv(u8* buffer, size_t maxLen) {
    if (!isConnected() || mSocket == INVALID_SOCKET_HANDLE) {
        return -1;
    }

    #ifdef FL_IS_WIN
    int result = ::recv(static_cast<SOCKET>(mSocket), reinterpret_cast<char*>(buffer), static_cast<int>(maxLen), 0); // ok reinterpret cast
    #else
    ssize_t result = ::recv(mSocket, buffer, maxLen, 0);
    #endif

    if (result < 0) {
        #ifdef FL_IS_WIN
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return 0;  // Non-blocking socket, no data available
        }
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // Non-blocking socket, no data available
        }
        #endif

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

void NativeHttpClient::setNonBlocking(bool enabled) {
    mNonBlocking = enabled;

    if (mSocket == INVALID_SOCKET_HANDLE) {
        return;  // Socket not created yet
    }

    #ifdef FL_IS_WIN
    u_long mode = enabled ? 1 : 0;
    ioctlsocket(static_cast<SOCKET>(mSocket), FIONBIO, &mode);
    #else
    int flags = fcntl(mSocket, F_GETFL, 0);
    if (enabled) {
        fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(mSocket, F_SETFL, flags & ~O_NONBLOCK);
    }
    #endif
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
    if (!isConnected() && mSocket != INVALID_SOCKET_HANDLE) {
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
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platformDisconnect();
    }

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
        #ifdef FL_IS_WIN
        SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        mSocket = static_cast<SocketHandle>(sock);
        #else
        int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == -1) {
            continue;
        }
        mSocket = sock;
        #endif

        // Apply non-blocking mode if enabled
        if (mNonBlocking) {
            setNonBlocking(true);
        }

        // Attempt connection
        #ifdef FL_IS_WIN
        ret = ::connect(static_cast<SOCKET>(mSocket), addr->ai_addr, static_cast<int>(addr->ai_addrlen));
        #else
        ret = ::connect(mSocket, addr->ai_addr, static_cast<socklen_t>(addr->ai_addrlen));
        #endif

        if (ret == 0) {
            // Connection successful
            connected = true;
            break;
        }

        #ifdef FL_IS_WIN
        int err = WSAGetLastError();
        if (mNonBlocking && err == WSAEWOULDBLOCK) {
            // Non-blocking connect in progress
            connected = true;
            break;
        }
        #else
        if (mNonBlocking && errno == EINPROGRESS) {
            // Non-blocking connect in progress
            connected = true;
            break;
        }
        #endif

        // Connection failed, close socket and try next address
        #ifdef FL_IS_WIN
        closesocket(static_cast<SOCKET>(mSocket));
        #else
        ::close(mSocket);
        #endif
        mSocket = INVALID_SOCKET_HANDLE;
    }

    freeaddrinfo(result);

    if (!connected) {
        mSocket = INVALID_SOCKET_HANDLE;
        return false;
    }

    return true;
}

void NativeHttpClient::platformDisconnect() {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        #ifdef FL_IS_WIN
        closesocket(static_cast<SOCKET>(mSocket));
        #else
        ::close(mSocket);
        #endif
        mSocket = INVALID_SOCKET_HANDLE;
    }
}

bool NativeHttpClient::isSocketConnected() const {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    #ifdef FL_IS_WIN
    int len = sizeof(error);
    int ret = getsockopt(static_cast<SOCKET>(mSocket), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len); // ok reinterpret cast
    #else
    socklen_t len = sizeof(error);
    int ret = getsockopt(mSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&error), &len); // ok reinterpret cast
    #endif

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl
