#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/connection.h"
#include "fl/stl/string.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

namespace fl {

// Client connection managed by server
struct ServerClientConnection {
    int socket;
    HttpConnection connection;
    u32 clientId;  // Unique identifier for this client

    // Default constructor (required for fl::vector operations)
    ServerClientConnection()
        : socket(-1)
        , connection(ConnectionConfig())
        , clientId(0)
    {
    }

    ServerClientConnection(int s, u32 id, const ConnectionConfig& config = ConnectionConfig())
        : socket(s)
        , connection(config)
        , clientId(id)
    {
        connection.onConnected(0);  // Mark as connected immediately
    }
};

// Native HTTP server using POSIX sockets
// Always non-blocking — blocking I/O is never appropriate on embedded
class NativeHttpServer {
public:
    // Constructor
    NativeHttpServer(u16 port, const ConnectionConfig& config = ConnectionConfig());
    ~NativeHttpServer();

    // Disable copy (socket ownership)
    NativeHttpServer(const NativeHttpServer&) = delete;
    NativeHttpServer& operator=(const NativeHttpServer&) = delete;

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

private:
    u16 mPort;
    int mListenSocket;  // Server listening socket
    bool mIsListening;
    u32 mNextClientId;
    ConnectionConfig mConfig;
    fl::vector<ServerClientConnection> mClients;

    // Platform-specific server operations
    bool platformStartListening();
    void platformStopListening();
    int platformAcceptClient();

    // Client helpers
    ServerClientConnection* findClient(u32 clientId);
    const ServerClientConnection* findClient(u32 clientId) const;
    void removeClient(u32 clientId);
    bool isSocketConnected(int socket) const;
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
