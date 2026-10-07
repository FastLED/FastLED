#pragma once

#include "fl/stl/asio/error_code.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Connection states for HTTP streaming
enum class ConnectionState {
    DISCONNECTED,   // Not connected, idle
    CONNECTING,     // Connection attempt in progress
    CONNECTED,      // Connected and active
    RECONNECTING,   // Reconnection attempt after failure
    CLOSED          // Connection permanently closed (user requested)
};

// Connection configuration
struct ConnectionConfig {
    // Reconnection settings
    u32 reconnectInitialDelayMs = 1000;   // Initial delay: 1s
    u32 reconnectMaxDelayMs = 30000;      // Max delay: 30s
    u32 reconnectBackoffMultiplier = 2;   // Exponential backoff multiplier

    // Heartbeat settings
    u32 heartbeatIntervalMs = 30000;      // Send heartbeat every 30s

    // Timeout settings
    u32 connectionTimeoutMs = 60000;      // Detect dead connection after 60s

    // Max reconnection attempts (0 = infinite)
    u32 maxReconnectAttempts = 0;
};

// HttpConnection: Manages connection lifecycle and state transitions
class HttpConnection {
public:
    explicit HttpConnection(const ConnectionConfig& config = ConnectionConfig()) FL_NO_EXCEPT;

    // State management
    ConnectionState getState() const FL_NO_EXCEPT;
    bool isConnected() const FL_NO_EXCEPT;
    bool isDisconnected() const FL_NO_EXCEPT;
    bool shouldReconnect() const FL_NO_EXCEPT;

    // Connection control
    void connect() FL_NO_EXCEPT;          // Initiate connection
    void disconnect() FL_NO_EXCEPT;       // Graceful disconnect
    void close() FL_NO_EXCEPT;           // Permanent close (no reconnect)

    // Connection events (call these from transport layer)
    void onConnected(u32 currentTimeMs = 0) FL_NO_EXCEPT;  // Connection established
    void onDisconnected() FL_NO_EXCEPT;  // Connection lost
    void onError() FL_NO_EXCEPT;         // Connection error

    // Asio-compatible: handle connection event from error_code
    void onEvent(const asio::error_code& ec, u32 currentTimeMs = 0) FL_NO_EXCEPT;

    // Heartbeat management
    void onHeartbeatSent() FL_NO_EXCEPT;      // Called when heartbeat sent
    void onHeartbeatReceived() FL_NO_EXCEPT;  // Called when heartbeat/data received
    bool shouldSendHeartbeat(u32 currentTimeMs) const FL_NO_EXCEPT;

    // Update loop (call regularly)
    void update(u32 currentTimeMs) FL_NO_EXCEPT;

    // Reconnection state
    u32 getReconnectDelayMs() const FL_NO_EXCEPT;
    u32 getReconnectAttempts() const FL_NO_EXCEPT;

    // Timeout detection
    bool isTimedOut(u32 currentTimeMs) const FL_NO_EXCEPT;

private:
    ConnectionConfig mConfig;
    ConnectionState mState;

    // Reconnection state
    u32 mReconnectAttempts;
    u32 mReconnectDelayMs;
    u32 mNextReconnectTimeMs;
    bool mWasReconnecting;  // Track if we were in RECONNECTING before CONNECTING

    // Heartbeat state
    u32 mLastHeartbeatSentMs;
    u32 mLastDataReceivedMs;

    // State transition helpers
    void transitionTo(ConnectionState newState, u32 currentTimeMs) FL_NO_EXCEPT;
    void resetReconnectState() FL_NO_EXCEPT;
    void resetReconnectAttempts() FL_NO_EXCEPT;
    u32 calculateBackoffDelay() const FL_NO_EXCEPT;
};

} // namespace fl
