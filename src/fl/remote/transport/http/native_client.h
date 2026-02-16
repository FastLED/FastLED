#pragma once

#include "fl/remote/transport/http/connection.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"

namespace fl {

// Platform-specific socket type
#ifdef FL_IS_WIN
    // Forward declare to avoid including winsock2.h in header
    typedef unsigned long long SocketHandle;
#else
    typedef int SocketHandle;
#endif

// Native HTTP client using POSIX sockets
// Supports blocking and non-blocking I/O
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
    int send(const u8* data, size_t len);
    int recv(u8* buffer, size_t maxLen);

    // Configuration
    void setNonBlocking(bool enabled);

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
    SocketHandle mSocket;  // Platform-specific socket handle
    bool mNonBlocking;
    HttpConnection mConnection;

    // Platform-specific connection
    bool platformConnect();
    void platformDisconnect();
    bool isSocketConnected() const;

    // Platform initialization
    void initPlatform();
    void cleanupPlatform();
};

} // namespace fl
