#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/connection.h"
#include "fl/stl/string.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {

// Native HTTP client using POSIX sockets
// Always non-blocking — blocking I/O is never appropriate on embedded
class NativeHttpClient {
public:
    // Constructor
    NativeHttpClient(const string& host, u16 port, const ConnectionConfig& config = ConnectionConfig());
    ~NativeHttpClient();

    // Disable copy (socket ownership)
    NativeHttpClient(const NativeHttpClient&) = delete;
    NativeHttpClient& operator=(const NativeHttpClient&) = delete;

    // Connection management
    bool connect();           // Initiate connection
    void disconnect();        // Close connection
    void close();            // Permanent close (no reconnect)
    bool isConnected() const;
    ConnectionState getState() const;

    // Socket I/O
    int send(fl::span<const u8> data);
    int recv(fl::span<u8> buffer);

    // Update loop (handles reconnection, heartbeat)
    void update(u32 currentTimeMs);

    // Heartbeat
    bool shouldSendHeartbeat(u32 currentTimeMs) const;
    void onHeartbeatSent();
    void onHeartbeatReceived();

    // Reconnection state
    u32 getReconnectDelayMs() const;
    u32 getReconnectAttempts() const;

private:
    string mHost;
    u16 mPort;
    int mSocket;  // Socket file descriptor
    HttpConnection mConnection;

    // Platform-specific connection
    bool platformConnect();
    void platformDisconnect();
    bool isSocketConnected() const;
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
