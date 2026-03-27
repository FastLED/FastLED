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
    ServerClientConnection() FL_NOEXCEPT
        : connection(ConnectionConfig())
        , clientId(0)
    {
    }

    ServerClientConnection(ServerClientConnection&& other) FL_NOEXCEPT
        : socket(fl::move(other.socket))
        , connection(other.connection)
        , clientId(other.clientId)
    {
    }

    ServerClientConnection& operator=(ServerClientConnection&& other) FL_NOEXCEPT {
        if (this != &other) {
            socket = fl::move(other.socket);
            connection = other.connection;
            clientId = other.clientId;
        }
        return *this;
    }

    // Not copyable (socket is not copyable)
    ServerClientConnection(const ServerClientConnection&) FL_NOEXCEPT = delete;
    ServerClientConnection& operator=(const ServerClientConnection&) FL_NOEXCEPT = delete;
};

// Native HTTP server using POSIX sockets
// Always non-blocking — blocking I/O is never appropriate on embedded
class NativeHttpServer {
public:
    // Constructor
    NativeHttpServer(u16 port, const ConnectionConfig& config = ConnectionConfig());
    ~NativeHttpServer() FL_NOEXCEPT;

    // Disable copy (socket ownership)
    NativeHttpServer(const NativeHttpServer&) FL_NOEXCEPT = delete;
    NativeHttpServer& operator=(const NativeHttpServer&) FL_NOEXCEPT = delete;

    // Server lifecycle
    bool start();             // Start listening for connections
    void stop();              // Stop server and disconnect all clients
    bool isListening() const;
    u16 port() const { return mPort; }  // Actual port (useful when constructed with port 0)

    // Client management
    void acceptClients();     // Accept new client connections (non-blocking)
    size_t getClientCount() const;
    bool hasClient(u32 clientId) const;
    void disconnectClient(u32 clientId);
    void disconnectAllClients();

    // Socket I/O (per-client)
    int send(u32 clientId, fl::span<const u8> data);
    int recv(u32 clientId, fl::span<u8> buffer);

    // Broadcast to all clients
    void broadcast(fl::span<const u8> data);

    // Update loop (handles disconnections, heartbeat)
    void update(u32 currentTimeMs);

    // Get list of active client IDs
    fl::vector<u32> getClientIds() const;

    // Access the underlying acceptor
    asio::ip::tcp::acceptor& acceptorRef() { return mAcceptor; }
    const asio::ip::tcp::acceptor& acceptorRef() const { return mAcceptor; }

private:
    u16 mPort;
    asio::ip::tcp::acceptor mAcceptor;
    bool mIsListening;
    u32 mNextClientId;
    ConnectionConfig mConfig;
    fl::vector<ServerClientConnection> mClients;

    // Platform-specific server operations
    bool platformStartListening();
    void platformStopListening();

    // Client helpers
    ServerClientConnection* findClient(u32 clientId);
    const ServerClientConnection* findClient(u32 clientId) const;
    void removeClient(u32 clientId);
    bool isSocketConnected(const asio::ip::tcp::socket& sock) const;
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
