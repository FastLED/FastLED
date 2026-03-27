#pragma once

#include "fl/task/promise.h"
#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/vector.h"
#include "fl/stl/asio/http/connection.h"
#include "fl/net/http/chunked_encoding.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace net {
namespace http {

class HttpStreamTransport;  // IWYU pragma: keep

/// Handle for ASYNC_STREAM calls
/// Provides onData() for intermediate updates, plus then()/catch_() for final result
class StreamHandle {
public:
    StreamHandle() FL_NOEXCEPT = default;

    /// Register callback for intermediate stream data
    StreamHandle& onData(fl::function<void(const fl::json&)> cb);

    /// Register callback for final result
    StreamHandle& then(fl::function<void(const fl::json&)> cb);

    /// Register callback for errors
    StreamHandle& catch_(fl::function<void(const fl::task::Error&)> cb);

    /// Access the underlying promise
    fl::task::Promise<fl::json>& promise();

    /// Check if handle is valid
    bool valid() const;

private:
    friend class HttpStreamTransport;
    StreamHandle(fl::task::Promise<fl::json> p,
                 fl::shared_ptr<fl::function<void(const fl::json&)>> updateCb);

    fl::task::Promise<fl::json> mPromise;
    fl::shared_ptr<fl::function<void(const fl::json&)>> mUpdateCallback;
};

/// Base class for HTTP streaming transport
/// Implements RequestSource and ResponseSink for Remote class
/// Manages HTTP connection lifecycle with chunked encoding
class HttpStreamTransport {
public:
    /// Connection state callback
    using StateCallback = fl::function<void()>;

    /// Constructor
    /// @param host Server hostname or IP address
    /// @param port Server port
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds (default: 30000ms = 30s)
    HttpStreamTransport(const fl::string& host, u16 port, u32 heartbeatIntervalMs = 30000);

    /// Virtual destructor
    virtual ~HttpStreamTransport() FL_NOEXCEPT;

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
    fl::optional<fl::json> readRequest();

    // ResponseSink Implementation (for Remote)

    /// Write JSON-RPC response to stream
    /// @param response JSON-RPC response object
    void writeResponse(const fl::json& response);

    // Promise-based RPC API

    /// Send a JSON-RPC request, returns promise that resolves with the final response.
    /// For ASYNC methods, the ACK is automatically filtered out.
    fl::task::Promise<fl::json> rpc(const fl::string& method, const fl::json& params);

    /// Send a pre-built JSON-RPC request
    fl::task::Promise<fl::json> rpc(const fl::json& fullRequest);

    /// Send a streaming JSON-RPC request, returns StreamHandle for intermediate data.
    /// Use onData() for intermediate chunks, then()/catch_() for final result.
    StreamHandle rpcStream(const fl::string& method, const fl::json& params);

    /// Send a streaming pre-built JSON-RPC request
    StreamHandle rpcStream(const fl::json& fullRequest);

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
    /// @return Number of bytes sent, or -1 on error
    virtual int sendData(fl::span<const u8> data) = 0;

    /// Receive raw data from connection
    /// @param buffer Buffer to receive data into
    /// @return Number of bytes received, or -1 on error
    virtual int recvData(fl::span<u8> buffer) = 0;

    /// Get current time in milliseconds
    /// @return Current time in milliseconds
    virtual u32 getCurrentTimeMs() const;

    /// Trigger reconnection (for subclasses to override)
    virtual void triggerReconnect();

    // Connection management
    HttpConnection mConnection;

private:
    // Pending call state (for rpc())
    struct PendingCall {
        fl::task::Promise<fl::json> promise;
        bool ackReceived = false;
    };

    // Pending stream state (for rpcStream())
    struct PendingStream {
        fl::task::Promise<fl::json> promise;
        fl::shared_ptr<fl::function<void(const fl::json&)>> updateCallback;
        bool ackReceived = false;
    };

    // Chunked encoding
    ChunkedReader mReader;
    ChunkedWriter mWriter;

    // Heartbeat tracking
    u32 mLastHeartbeatSent;
    u32 mLastHeartbeatReceived;
    u32 mHeartbeatInterval;
    u32 mTimeoutMs;

    // Connection state
    bool mWasConnected;

    // Callbacks
    StateCallback mOnConnect;
    StateCallback mOnDisconnect;

    // Promise-based call tracking
    fl::flat_map<fl::string, PendingCall, fl::StringFastLess> mPendingCalls;
    fl::flat_map<fl::string, PendingStream, fl::StringFastLess> mPendingStreams;
    fl::vector<fl::json> mIncomingQueue;
    int mNextCallId = 1;

    // Internal methods
    void sendHeartbeat();
    void checkHeartbeatTimeout(u32 currentTimeMs);
    bool processIncomingData();
    void handleConnectionStateChange(u32 currentTimeMs);
    void parseChunkedMessages();
    bool resolveRpc(const fl::json& msg, const fl::string& idKey);
    bool resolveRpcStream(const fl::json& msg, const fl::string& idKey);
    static fl::string idToString(const fl::json& id);
};

}  // namespace http
}  // namespace net
}  // namespace fl
