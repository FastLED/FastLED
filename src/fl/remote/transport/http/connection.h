#pragma once

#include "fl/stl/stdint.h"

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
    explicit HttpConnection(const ConnectionConfig& config = ConnectionConfig());

    // State management
    ConnectionState getState() const;
    bool isConnected() const;
    bool isDisconnected() const;
    bool shouldReconnect() const;

    // Connection control
    void connect();          // Initiate connection
    void disconnect();       // Graceful disconnect
    void close();           // Permanent close (no reconnect)

    // Connection events (call these from transport layer)
    void onConnected(u32 currentTimeMs = 0);  // Connection established
    void onDisconnected();  // Connection lost
    void onError();         // Connection error

    // Heartbeat management
    void onHeartbeatSent();      // Called when heartbeat sent
    void onHeartbeatReceived();  // Called when heartbeat/data received
    bool shouldSendHeartbeat(u32 currentTimeMs) const;

    // Update loop (call regularly)
    void update(u32 currentTimeMs);

    // Reconnection state
    u32 getReconnectDelayMs() const;
    u32 getReconnectAttempts() const;

    // Timeout detection
    bool isTimedOut(u32 currentTimeMs) const;

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
    void transitionTo(ConnectionState newState, u32 currentTimeMs);
    void resetReconnectState();
    void resetReconnectAttempts();
    u32 calculateBackoffDelay() const;
};

} // namespace fl
