#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/asio/ip/tcp.h"
#include "fl/stl/asio/error_code.h"
#include "fl/stl/asio/http/connection.h"
#include "fl/stl/string.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Native HTTP client using POSIX sockets
// Always non-blocking — blocking I/O is never appropriate on embedded
class NativeHttpClient {
public:
    // Constructor (Asio-compatible: accepts endpoint)
    NativeHttpClient(const asio::ip::tcp::endpoint& ep, const ConnectionConfig& config = ConnectionConfig()) FL_NO_EXCEPT;
    // Legacy constructor (backward compatible)
    NativeHttpClient(const string& host, u16 port, const ConnectionConfig& config = ConnectionConfig()) FL_NO_EXCEPT;
    ~NativeHttpClient() FL_NO_EXCEPT;

    // Disable copy (socket ownership)
    NativeHttpClient(const NativeHttpClient&) FL_NO_EXCEPT = delete;
    NativeHttpClient& operator=(const NativeHttpClient&) FL_NO_EXCEPT = delete;

    // Connection management
    bool connect() FL_NO_EXCEPT;           // Initiate connection
    void disconnect() FL_NO_EXCEPT;        // Close connection
    void close() FL_NO_EXCEPT;            // Permanent close (no reconnect)
    bool isConnected() const FL_NO_EXCEPT;
    ConnectionState getState() const FL_NO_EXCEPT;

    // Socket I/O
    int send(fl::span<const u8> data) FL_NO_EXCEPT;
    int recv(fl::span<u8> buffer) FL_NO_EXCEPT;

    // Update loop (handles reconnection, heartbeat)
    void update(u32 currentTimeMs) FL_NO_EXCEPT;

    // Heartbeat
    bool shouldSendHeartbeat(u32 currentTimeMs) const FL_NO_EXCEPT;
    void onHeartbeatSent() FL_NO_EXCEPT;
    void onHeartbeatReceived() FL_NO_EXCEPT;

    // Reconnection state
    u32 getReconnectDelayMs() const FL_NO_EXCEPT;
    u32 getReconnectAttempts() const FL_NO_EXCEPT;

    // Access the endpoint
    const asio::ip::tcp::endpoint& endpoint() const FL_NO_EXCEPT { return mEndpoint; }

    // Access the underlying tcp::socket
    asio::ip::tcp::socket& socket() FL_NO_EXCEPT { return mSocket; }
    const asio::ip::tcp::socket& socket() const FL_NO_EXCEPT { return mSocket; }

private:
    asio::ip::tcp::endpoint mEndpoint;
    asio::ip::tcp::socket mSocket;
    HttpConnection mConnection;

    // Platform-specific connection
    bool platformConnect() FL_NO_EXCEPT;
    void platformDisconnect() FL_NO_EXCEPT;
    bool isSocketConnected() const FL_NO_EXCEPT;
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
