#pragma once

#include "fl/json.h"
#include "fl/stl/optional.h"
#include "connection.h"
#include "chunked_encoding.h"

namespace fl {

/// Base class for HTTP streaming transport
/// Implements RequestSource and ResponseSink for Remote class
/// Manages HTTP connection lifecycle with chunked encoding
class HttpStreamTransport {
public:
    /// Connection state callback
    using StateCallback = void(*)();

    /// Constructor
    /// @param host Server hostname or IP address
    /// @param port Server port
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds (default: 30000ms = 30s)
    HttpStreamTransport(const char* host, u16 port, u32 heartbeatIntervalMs = 30000);

    /// Virtual destructor
    virtual ~HttpStreamTransport();

    // Connection Management

    /// Connect to server
    /// @return true if connected successfully, false otherwise
    virtual bool connect() = 0;

    /// Disconnect from server
    virtual void disconnect() = 0;

    /// Check if connected
    /// @return true if connected, false otherwise
    virtual bool isConnected() const = 0;

    // RequestSource Implementation (for Remote)

    /// Read next JSON-RPC request from stream
    /// Non-blocking, returns nullopt if no complete request available
    /// @return JSON-RPC request object or nullopt
    fl::optional<fl::Json> readRequest();

    // ResponseSink Implementation (for Remote)

    /// Write JSON-RPC response to stream
    /// @param response JSON-RPC response object
    void writeResponse(const fl::Json& response);

    // Update Loop

    /// Update connection state (handles reconnection, heartbeat)
    /// Call this in main loop
    /// @param currentTimeMs Current time in milliseconds
    void update(u32 currentTimeMs);

    // Callbacks

    /// Set callback for connection established
    void setOnConnect(StateCallback callback);

    /// Set callback for connection lost
    void setOnDisconnect(StateCallback callback);

    // Configuration

    /// Set heartbeat interval
    /// @param intervalMs Heartbeat interval in milliseconds
    void setHeartbeatInterval(u32 intervalMs);

    /// Get heartbeat interval
    /// @return Heartbeat interval in milliseconds
    u32 getHeartbeatInterval() const;

    /// Set connection timeout
    /// @param timeoutMs Connection timeout in milliseconds
    void setTimeout(u32 timeoutMs);

    /// Get connection timeout
    /// @return Connection timeout in milliseconds
    u32 getTimeout() const;

protected:
    // Abstract methods for subclasses

    /// Send raw data over connection
    /// @param data Data to send
    /// @param length Data length
    /// @return Number of bytes sent, or -1 on error
    virtual int sendData(const u8* data, size_t length) = 0;

    /// Receive raw data from connection
    /// @param buffer Buffer to receive data
    /// @param maxLength Maximum bytes to receive
    /// @return Number of bytes received, or -1 on error
    virtual int recvData(u8* buffer, size_t maxLength) = 0;

    /// Get current time in milliseconds
    /// @return Current time in milliseconds
    virtual u32 getCurrentTimeMs() const;

    /// Trigger reconnection (for subclasses to override)
    virtual void triggerReconnect();

    // Connection management
    HttpConnection mConnection;

private:
    // Chunked encoding
    ChunkedReader mReader;
    ChunkedWriter mWriter;

    // Heartbeat tracking
    u32 mLastHeartbeatSent;
    u32 mLastHeartbeatReceived;
    u32 mHeartbeatInterval;
    u32 mTimeoutMs;

    // Connection state
    const char* mHost;
    u16 mPort;
    bool mWasConnected;

    // Callbacks
    StateCallback mOnConnect;
    StateCallback mOnDisconnect;

    // Internal methods
    void sendHeartbeat();
    void checkHeartbeatTimeout(u32 currentTimeMs);
    bool processIncomingData();
    void handleConnectionStateChange();
};

}  // namespace fl
