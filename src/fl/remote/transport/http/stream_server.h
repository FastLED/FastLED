#pragma once

#include "stream_transport.h"
#include "native_server.h"
#include "http_parser.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/map.h"
#include "fl/stl/vector.h"

namespace fl {

/// HTTP streaming server for RPC
/// Extends HttpStreamTransport with server-side HTTP logic
/// Accepts HTTP POST requests and sends JSON-RPC responses
/// Supports multiple concurrent clients with chunked encoding
class HttpStreamServer : public HttpStreamTransport {
public:
    /// Constructor
    /// @param port Server port (default: 8080)
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds (default: 30000ms = 30s)
    HttpStreamServer(u16 port = 8080, u32 heartbeatIntervalMs = 30000);

    /// Virtual destructor
    ~HttpStreamServer() override;

    // Connection Management (override from HttpStreamTransport)

    /// Start HTTP server (listen for connections)
    /// @return true if server started successfully, false otherwise
    bool connect() override;

    /// Stop HTTP server (disconnect all clients)
    void disconnect() override;

    /// Check if server is listening
    /// @return true if listening, false otherwise
    bool isConnected() const override;

    // Server-specific methods

    /// Accept new client connections (non-blocking)
    /// Call this in update loop to accept new clients
    void acceptClients();

    /// Get number of connected clients
    /// @return Number of connected clients
    size_t getClientCount() const;

    /// Disconnect specific client
    /// @param clientId Client ID
    void disconnectClient(u32 clientId);

    /// Get list of connected client IDs
    /// @return Vector of client IDs
    fl::vector<u32> getClientIds() const;

protected:
    // Abstract methods implementation (from HttpStreamTransport)

    /// Send raw data to all connected clients (broadcast)
    /// @param data Data to send
    /// @param length Data length
    /// @return Number of bytes sent, or -1 on error
    int sendData(const u8* data, size_t length) override;

    /// Receive raw data from any client
    /// Processes data from all clients, returns first available
    /// @param buffer Buffer to receive data
    /// @param maxLength Maximum bytes to receive
    /// @return Number of bytes received, or -1 on error
    int recvData(u8* buffer, size_t maxLength) override;

    /// Trigger reconnection (for server: disconnect all and restart)
    void triggerReconnect() override;

private:
    /// Client connection state
    struct ClientState {
        u32 clientId;
        bool httpHeaderReceived;
        bool httpHeaderSent;
        HttpRequestParser requestParser;
        fl::vector<u8> pendingData;  // Data waiting to be processed

        // Default constructor (required for map operations)
        ClientState()
            : clientId(0)
            , httpHeaderReceived(false)
            , httpHeaderSent(false)
            , requestParser()
            , pendingData()
        {
        }

        ClientState(u32 id)
            : clientId(id)
            , httpHeaderReceived(false)
            , httpHeaderSent(false)
            , requestParser()
            , pendingData()
        {
        }
    };

    /// Read and validate HTTP request header from client
    /// @param clientId Client ID
    /// @return true if valid request header received, false otherwise
    bool readHttpRequestHeader(u32 clientId);

    /// Send HTTP response header to client
    /// @param clientId Client ID
    /// @return true if sent successfully, false otherwise
    bool sendHttpResponseHeader(u32 clientId);

    /// Process incoming data from client
    /// @param clientId Client ID
    /// @return true if data processed successfully, false otherwise
    bool processClientData(u32 clientId);

    /// Get or create client state
    /// @param clientId Client ID
    /// @return Pointer to client state, or nullptr on error
    ClientState* getOrCreateClientState(u32 clientId);

    /// Remove client state
    /// @param clientId Client ID
    void removeClientState(u32 clientId);

    /// Native socket server
    fl::unique_ptr<NativeHttpServer> mNativeServer;

    /// Server port
    u16 mPort;

    /// Client states (maps client ID to state)
    fl::map<u32, ClientState> mClientStates;

    /// Buffer for reading data from clients
    fl::vector<u8> mRecvBuffer;

    /// Last client ID that sent data (for round-robin processing)
    u32 mLastProcessedClientId;
};

}  // namespace fl
