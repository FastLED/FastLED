#pragma once

#include "fl/remote/transport/http/connection.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

namespace fl {

// Platform-specific socket type
#ifdef FL_IS_WIN
    // Forward declare to avoid including winsock2.h in header
    typedef unsigned long long SocketHandle;
#else
    typedef int SocketHandle;
#endif

// Client connection managed by server
struct ServerClientConnection {
    SocketHandle socket;
    HttpConnection connection;
    u32 clientId;  // Unique identifier for this client

    // Default constructor (required for fl::vector operations)
    ServerClientConnection()
        : socket(0)
        , connection(ConnectionConfig())
        , clientId(0)
    {
    }

    ServerClientConnection(SocketHandle s, u32 id, const ConnectionConfig& config = ConnectionConfig())
        : socket(s)
        , connection(config)
        , clientId(id)
    {
        connection.onConnected(0);  // Mark as connected immediately
    }
};

// Native HTTP server using POSIX sockets
// Supports multiple concurrent clients, non-blocking I/O
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

    // Client management
    void acceptClients();     // Accept new client connections (non-blocking)
    size_t getClientCount() const;
    bool hasClient(u32 clientId) const;
    void disconnectClient(u32 clientId);
    void disconnectAllClients();

    // Socket I/O (per-client)
    int send(u32 clientId, const u8* data, size_t len);
    int recv(u32 clientId, u8* buffer, size_t maxLen);

    // Broadcast to all clients
    void broadcast(const u8* data, size_t len);

    // Configuration
    void setNonBlocking(bool enabled);

    // Update loop (handles disconnections, heartbeat)
    void update(u32 currentTimeMs);

    // Get list of active client IDs
    fl::vector<u32> getClientIds() const;

private:
    u16 mPort;
    SocketHandle mListenSocket;  // Server listening socket
    bool mNonBlocking;
    bool mIsListening;
    u32 mNextClientId;
    ConnectionConfig mConfig;
    fl::vector<ServerClientConnection> mClients;

    // Platform-specific server operations
    bool platformStartListening();
    void platformStopListening();
    SocketHandle platformAcceptClient();

    // Client helpers
    ServerClientConnection* findClient(u32 clientId);
    const ServerClientConnection* findClient(u32 clientId) const;
    void removeClient(u32 clientId);
    bool isSocketConnected(SocketHandle socket) const;

    // Platform initialization
    void initPlatform();
    void cleanupPlatform();
};

} // namespace fl
