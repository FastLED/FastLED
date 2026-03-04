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
#include "fl/net/http/native_server.h"

// Ensure MSG_NOSIGNAL is available (not defined on Windows/macOS)
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace fl {

namespace {
    // Platform socket wrappers - avoids name conflicts with class members
    // On Windows: delegates to fl:: wrappers from socket_win.h
    // On POSIX: delegates to :: system calls from socket_posix.h
    ssize_t platform_send(int fd, const void* buf, size_t len, int flags) {
        #ifdef FL_IS_WIN
        return fl::send(fd, buf, len, flags);
        #else
        return ::send(fd, buf, len, flags);
        #endif
    }

    ssize_t platform_recv(int fd, void* buf, size_t len, int flags) {
        #ifdef FL_IS_WIN
        return fl::recv(fd, buf, len, flags);
        #else
        return ::recv(fd, buf, len, flags);
        #endif
    }

    // Helper: Set socket to non-blocking mode
    bool set_socket_nonblocking(int fd, bool enabled) {
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

    int platform_close(int fd) {
        #ifdef FL_IS_WIN
        return fl::close(fd);
        #else
        return ::close(fd);
        #endif
    }

    // Helper: Check if socket error indicates "would block" (non-blocking)
    bool is_server_error_would_block() {
        #ifdef FL_IS_WIN
        int err = WSAGetLastError();
        return err == WSAEWOULDBLOCK;
        #else
        return errno == EWOULDBLOCK || errno == EAGAIN;
        #endif
    }
}

NativeHttpServer::NativeHttpServer(u16 port, const ConnectionConfig& config)
    : mPort(port)
    , mListenSocket(-1)
    , mIsListening(false)
    , mNextClientId(1)
    , mConfig(config)
{
}

NativeHttpServer::~NativeHttpServer() {
    stop();
}

bool NativeHttpServer::start() {
    if (mIsListening) {
        return true;  // Already listening
    }

    if (platformStartListening()) {
        mIsListening = true;
        return true;
    }

    return false;
}

void NativeHttpServer::stop() {
    // Disconnect all clients
    disconnectAllClients();

    // Stop listening
    platformStopListening();
    mIsListening = false;
}

bool NativeHttpServer::isListening() const {
    return mIsListening;
}

void NativeHttpServer::acceptClients() {
    if (!mIsListening) {
        return;
    }

    // Accept new clients in a loop (non-blocking)
    while (true) {
        int clientSocket = platformAcceptClient();
        if (clientSocket == -1) {
            break;  // No more clients to accept
        }

        // Create client connection
        u32 clientId = mNextClientId++;
        mClients.emplace_back(clientSocket, clientId, mConfig);
    }
}

size_t NativeHttpServer::getClientCount() const {
    return mClients.size();
}

bool NativeHttpServer::hasClient(u32 clientId) const {
    return findClient(clientId) != nullptr;
}

void NativeHttpServer::disconnectClient(u32 clientId) {
    removeClient(clientId);
}

void NativeHttpServer::disconnectAllClients() {
    for (auto& client : mClients) {
        platform_close(client.socket);
    }
    mClients.clear();
}

int NativeHttpServer::send(u32 clientId, fl::span<const u8> data) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || client->socket == -1) {
        return -1;
    }

    ssize_t result = platform_send(client->socket, (const char*)data.data(), data.size(), MSG_NOSIGNAL);

    if (result < 0) {
        // Connection error, disconnect client
        client->connection.onDisconnected();
        return -1;
    }

    return static_cast<int>(result);
}

int NativeHttpServer::recv(u32 clientId, fl::span<u8> buffer) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || client->socket == -1) {
        return -1;
    }

    ssize_t result = platform_recv(client->socket, (char*)buffer.data(), buffer.size(), 0);

    if (result < 0) {
        if (is_server_error_would_block()) {
            return 0;  // Non-blocking socket, no data available
        }

        // Connection error, disconnect client
        client->connection.onDisconnected();
        return -1;
    } else if (result == 0) {
        // Connection closed by peer
        client->connection.onDisconnected();
        return -1;
    }

    // Data received, update heartbeat tracking
    client->connection.onHeartbeatReceived();

    return static_cast<int>(result);
}

void NativeHttpServer::broadcast(fl::span<const u8> data) {
    // Send to all clients
    for (auto& client : mClients) {
        send(client.clientId, data);
    }
}

void NativeHttpServer::update(u32 currentTimeMs) {
    // Check for dead connections
    for (size_t i = 0; i < mClients.size(); ) {
        auto& client = mClients[i];

        // Update connection state
        client.connection.update(currentTimeMs);

        // Check if client is still connected
        if (!client.connection.isConnected() || !isSocketConnected(client.socket)) {
            // Remove disconnected client
            platform_close(client.socket);
            mClients.erase(mClients.begin() + i);
            continue;
        }

        ++i;
    }
}

fl::vector<u32> NativeHttpServer::getClientIds() const {
    fl::vector<u32> ids;
    ids.reserve(mClients.size());
    for (const auto& client : mClients) {
        ids.push_back(client.clientId);
    }
    return ids;
}

bool NativeHttpServer::platformStartListening() {
    // Clean up existing socket
    if (mListenSocket != -1) {
        platformStopListening();
    }

    // Ensure platform socket layer is initialized (needed before socket())
    #ifdef FL_IS_WIN
    if (!initialize_winsock()) {
        return false;
    }
    #endif

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return false;
    }
    mListenSocket = sock;

    // Set SO_REUSEADDR to allow quick restarts
    int reuse = 1;
    setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));

    // Bind to port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mPort);
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces

    int ret = bind(mListenSocket, (struct sockaddr*)&addr, sizeof(addr));

    if (ret != 0) {
        platform_close(mListenSocket);
        mListenSocket = -1;
        return false;
    }

    // If port 0 was requested, query the OS-assigned port
    if (mPort == 0) {
        struct sockaddr_in boundAddr{};
        socklen_t addrLen = sizeof(boundAddr);
        if (getsockname(mListenSocket, (struct sockaddr*)&boundAddr, &addrLen) == 0) {
            mPort = ntohs(boundAddr.sin_port);
        }
    }

    // Start listening (backlog = 10 pending connections)
    ret = listen(mListenSocket, 10);

    if (ret != 0) {
        platform_close(mListenSocket);
        mListenSocket = -1;
        return false;
    }

    // Always apply non-blocking mode — blocking is never appropriate on embedded
    set_socket_nonblocking(mListenSocket, true);

    return true;
}

void NativeHttpServer::platformStopListening() {
    if (mListenSocket != -1) {
        platform_close(mListenSocket);
        mListenSocket = -1;
    }
}

int NativeHttpServer::platformAcceptClient() {
    if (mListenSocket == -1) {
        return -1;
    }

    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientSocket = accept(mListenSocket, (struct sockaddr*)&clientAddr, &addrLen);

    if (clientSocket < 0) {
        // No clients waiting or error - both return -1
        return -1;
    }

    // Always set non-blocking on client sockets
    set_socket_nonblocking(clientSocket, true);

    return clientSocket;
}

ServerClientConnection* NativeHttpServer::findClient(u32 clientId) {
    for (auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

const ServerClientConnection* NativeHttpServer::findClient(u32 clientId) const {
    for (const auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

void NativeHttpServer::removeClient(u32 clientId) {
    for (size_t i = 0; i < mClients.size(); ++i) {
        if (mClients[i].clientId == clientId) {
            platform_close(mClients[i].socket);
            mClients.erase(mClients.begin() + i);
            return;
        }
    }
}

bool NativeHttpServer::isSocketConnected(int sock) const {
    if (sock == -1) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    socklen_t len = sizeof(error);
    int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
