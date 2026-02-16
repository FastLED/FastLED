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
    #include <errno.h>  // IWYU pragma: keep
#endif

// Now include FastLED headers
#include "fl/remote/transport/http/native_server.h"

// Platform-specific constants
#ifdef FL_IS_WIN
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
    static const fl::SocketHandle INVALID_SOCKET_HANDLE_SERVER = static_cast<fl::SocketHandle>(INVALID_SOCKET);
#else
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET ::close
    static const fl::SocketHandle INVALID_SOCKET_HANDLE_SERVER = static_cast<fl::SocketHandle>(-1);
#endif

namespace fl {

namespace {
    // Global Windows socket initialization counter
    #ifdef FL_IS_WIN
    static int gWinsockServerInitCount = 0;
    #endif
}

NativeHttpServer::NativeHttpServer(u16 port, const ConnectionConfig& config)
    : mPort(port)
    , mListenSocket(INVALID_SOCKET_HANDLE_SERVER)
    , mNonBlocking(false)
    , mIsListening(false)
    , mNextClientId(1)
    , mConfig(config)
{
    initPlatform();
}

NativeHttpServer::~NativeHttpServer() {
    stop();
    cleanupPlatform();
}

void NativeHttpServer::initPlatform() {
    #ifdef FL_IS_WIN
    if (gWinsockServerInitCount == 0) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            // Failed to initialize Winsock
            return;
        }
    }
    gWinsockServerInitCount++;
    #endif
}

void NativeHttpServer::cleanupPlatform() {
    #ifdef FL_IS_WIN
    gWinsockServerInitCount--;
    if (gWinsockServerInitCount == 0) {
        WSACleanup();
    }
    #endif
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
        SocketHandle clientSocket = platformAcceptClient();
        if (clientSocket == INVALID_SOCKET_HANDLE_SERVER) {
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
        CLOSE_SOCKET(client.socket);
    }
    mClients.clear();
}

int NativeHttpServer::send(u32 clientId, const u8* data, size_t len) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || client->socket == INVALID_SOCKET_HANDLE_SERVER) {
        return -1;
    }

    #ifdef FL_IS_WIN
    int result = ::send(static_cast<SOCKET>(client->socket), reinterpret_cast<const char*>(data), static_cast<int>(len), 0); // ok reinterpret cast
    #else
    ssize_t result = ::send(client->socket, data, len, MSG_NOSIGNAL);
    #endif

    if (result < 0) {
        // Connection error, disconnect client
        client->connection.onDisconnected();
        return -1;
    }

    return static_cast<int>(result);
}

int NativeHttpServer::recv(u32 clientId, u8* buffer, size_t maxLen) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || client->socket == INVALID_SOCKET_HANDLE_SERVER) {
        return -1;
    }

    #ifdef FL_IS_WIN
    int result = ::recv(static_cast<SOCKET>(client->socket), reinterpret_cast<char*>(buffer), static_cast<int>(maxLen), 0); // ok reinterpret cast
    #else
    ssize_t result = ::recv(client->socket, buffer, maxLen, 0);
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

void NativeHttpServer::broadcast(const u8* data, size_t len) {
    // Send to all clients
    for (auto& client : mClients) {
        send(client.clientId, data, len);
    }
}

void NativeHttpServer::setNonBlocking(bool enabled) {
    mNonBlocking = enabled;

    // Apply to listen socket
    if (mListenSocket != INVALID_SOCKET_HANDLE_SERVER) {
        #ifdef FL_IS_WIN
        u_long mode = enabled ? 1 : 0;
        ioctlsocket(static_cast<SOCKET>(mListenSocket), FIONBIO, &mode);
        #else
        int flags = fcntl(mListenSocket, F_GETFL, 0);
        if (enabled) {
            fcntl(mListenSocket, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(mListenSocket, F_SETFL, flags & ~O_NONBLOCK);
        }
        #endif
    }

    // Apply to all client sockets
    for (auto& client : mClients) {
        if (client.socket != INVALID_SOCKET_HANDLE_SERVER) {
            #ifdef FL_IS_WIN
            u_long mode = enabled ? 1 : 0;
            ioctlsocket(static_cast<SOCKET>(client.socket), FIONBIO, &mode);
            #else
            int flags = fcntl(client.socket, F_GETFL, 0);
            if (enabled) {
                fcntl(client.socket, F_SETFL, flags | O_NONBLOCK);
            } else {
                fcntl(client.socket, F_SETFL, flags & ~O_NONBLOCK);
            }
            #endif
        }
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
            CLOSE_SOCKET(client.socket);
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
    if (mListenSocket != INVALID_SOCKET_HANDLE_SERVER) {
        platformStopListening();
    }

    // Create socket
    #ifdef FL_IS_WIN
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }
    mListenSocket = static_cast<SocketHandle>(sock);
    #else
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        return false;
    }
    mListenSocket = sock;
    #endif

    // Set SO_REUSEADDR to allow quick restarts
    int reuse = 1;
    #ifdef FL_IS_WIN
    setsockopt(static_cast<SOCKET>(mListenSocket), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)); // ok reinterpret cast
    #else
    setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const void*>(&reuse), sizeof(reuse)); // ok reinterpret cast
    #endif

    // Bind to port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mPort);
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces

    #ifdef FL_IS_WIN
    int ret = bind(static_cast<SOCKET>(mListenSocket), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)); // ok reinterpret cast
    #else
    int ret = bind(mListenSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)); // ok reinterpret cast
    #endif

    if (ret != 0) {
        CLOSE_SOCKET(mListenSocket);
        mListenSocket = INVALID_SOCKET_HANDLE_SERVER;
        return false;
    }

    // Start listening (backlog = 10 pending connections)
    #ifdef FL_IS_WIN
    ret = listen(static_cast<SOCKET>(mListenSocket), 10);
    #else
    ret = listen(mListenSocket, 10);
    #endif

    if (ret != 0) {
        CLOSE_SOCKET(mListenSocket);
        mListenSocket = INVALID_SOCKET_HANDLE_SERVER;
        return false;
    }

    // Apply non-blocking mode if enabled
    if (mNonBlocking) {
        setNonBlocking(true);
    }

    return true;
}

void NativeHttpServer::platformStopListening() {
    if (mListenSocket != INVALID_SOCKET_HANDLE_SERVER) {
        CLOSE_SOCKET(mListenSocket);
        mListenSocket = INVALID_SOCKET_HANDLE_SERVER;
    }
}

SocketHandle NativeHttpServer::platformAcceptClient() {
    if (mListenSocket == INVALID_SOCKET_HANDLE_SERVER) {
        return INVALID_SOCKET_HANDLE_SERVER;
    }

    struct sockaddr_in clientAddr{};
    #ifdef FL_IS_WIN
    int addrLen = sizeof(clientAddr);
    SOCKET clientSocket = accept(static_cast<SOCKET>(mListenSocket), reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen); // ok reinterpret cast
    if (clientSocket == INVALID_SOCKET) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return INVALID_SOCKET_HANDLE_SERVER;  // No clients waiting
        }
        return INVALID_SOCKET_HANDLE_SERVER;  // Error
    }
    #else
    socklen_t addrLen = sizeof(clientAddr);
    int clientSocket = accept(mListenSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen); // ok reinterpret cast
    if (clientSocket == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return INVALID_SOCKET_HANDLE_SERVER;  // No clients waiting
        }
        return INVALID_SOCKET_HANDLE_SERVER;  // Error
    }
    #endif

    // Apply non-blocking mode to client socket if enabled
    if (mNonBlocking) {
        #ifdef FL_IS_WIN
        u_long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);
        #else
        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
        #endif
    }

    #ifdef FL_IS_WIN
    return static_cast<SocketHandle>(clientSocket);
    #else
    return clientSocket;
    #endif
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
            CLOSE_SOCKET(mClients[i].socket);
            mClients.erase(mClients.begin() + i);
            return;
        }
    }
}

bool NativeHttpServer::isSocketConnected(SocketHandle socket) const {
    if (socket == INVALID_SOCKET_HANDLE_SERVER) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    #ifdef FL_IS_WIN
    int len = sizeof(error);
    int ret = getsockopt(static_cast<SOCKET>(socket), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len); // ok reinterpret cast
    #else
    socklen_t len = sizeof(error);
    int ret = getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&error), &len); // ok reinterpret cast
    #endif

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl
