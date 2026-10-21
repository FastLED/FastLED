#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/asio/error_code.h"
#include "fl/stl/asio/http/connection.h"
#include "fl/stl/asio/ip/tcp.h"
#include "fl/stl/string.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Client connection managed by server
struct ServerClientConnection {
    asio::ip::tcp::socket socket;
    HttpConnection connection;
    u32 clientId;  // Unique identifier for this client

    // Default constructor (required for fl::vector operations)
    ServerClientConnection() FL_NO_EXCEPT
        : connection(ConnectionConfig())
        , clientId(0)
    {
    }

    ServerClientConnection(ServerClientConnection&& other) FL_NO_EXCEPT
        : socket(fl::move(other.socket))
        , connection(other.connection)
        , clientId(other.clientId)
    {
    }

    ServerClientConnection& operator=(ServerClientConnection&& other) FL_NO_EXCEPT {
        if (this != &other) {
            socket = fl::move(other.socket);
            connection = other.connection;
            clientId = other.clientId;
        }
        return *this;
    }

    // Not copyable (socket is not copyable)
    ServerClientConnection(const ServerClientConnection&) FL_NO_EXCEPT = delete;
    ServerClientConnection& operator=(const ServerClientConnection&) FL_NO_EXCEPT = delete;
};

// Native HTTP server using POSIX sockets
// Always non-blocking — blocking I/O is never appropriate on embedded
class NativeHttpServer {
public:
    // Constructor
    NativeHttpServer(u16 port, const ConnectionConfig& config = ConnectionConfig()) FL_NO_EXCEPT;
    ~NativeHttpServer() FL_NO_EXCEPT;

    // Disable copy (socket ownership)
    NativeHttpServer(const NativeHttpServer&) FL_NO_EXCEPT = delete;
    NativeHttpServer& operator=(const NativeHttpServer&) FL_NO_EXCEPT = delete;

    // Server lifecycle
    bool start() FL_NO_EXCEPT;             // Start listening for connections
    void stop() FL_NO_EXCEPT;              // Stop server and disconnect all clients
    bool isListening() const FL_NO_EXCEPT;
    u16 port() const FL_NO_EXCEPT { return mPort; }  // Actual port (useful when constructed with port 0)

    // Client management
    void acceptClients() FL_NO_EXCEPT;     // Accept new client connections (non-blocking)
    size_t getClientCount() const FL_NO_EXCEPT;
    bool hasClient(u32 clientId) const FL_NO_EXCEPT;
    void disconnectClient(u32 clientId) FL_NO_EXCEPT;
    void disconnectAllClients() FL_NO_EXCEPT;

    // Socket I/O (per-client)
    int send(u32 clientId, fl::span<const u8> data) FL_NO_EXCEPT;
    int recv(u32 clientId, fl::span<u8> buffer) FL_NO_EXCEPT;

    // Broadcast to all clients
    void broadcast(fl::span<const u8> data) FL_NO_EXCEPT;

    // Update loop (handles disconnections, heartbeat)
    void update(u32 currentTimeMs) FL_NO_EXCEPT;

    // Get list of active client IDs
    fl::vector<u32> getClientIds() const FL_NO_EXCEPT;

    // Access the underlying acceptor
    asio::ip::tcp::acceptor& acceptorRef() FL_NO_EXCEPT { return mAcceptor; }
    const asio::ip::tcp::acceptor& acceptorRef() const FL_NO_EXCEPT { return mAcceptor; }

private:
    u16 mPort;
    asio::ip::tcp::acceptor mAcceptor;
    bool mIsListening;
    u32 mNextClientId;
    ConnectionConfig mConfig;
    fl::vector<ServerClientConnection> mClients;

    // Platform-specific server operations
    bool platformStartListening() FL_NO_EXCEPT;
    void platformStopListening() FL_NO_EXCEPT;

    // Client helpers
    ServerClientConnection* findClient(u32 clientId) FL_NO_EXCEPT;
    const ServerClientConnection* findClient(u32 clientId) const FL_NO_EXCEPT;
    void removeClient(u32 clientId) FL_NO_EXCEPT;
    bool isSocketConnected(const asio::ip::tcp::socket& sock) const FL_NO_EXCEPT;
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
