#pragma once

#include "fl/stl/asio/http/stream_transport.h"
#include "fl/stl/asio/http/native_client.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"

namespace fl {

/// HTTP streaming client for RPC
/// Extends HttpStreamTransport with client-side HTTP logic
/// Sends JSON-RPC requests over HTTP POST with chunked encoding
/// Receives JSON-RPC responses over chunked HTTP response
class HttpStreamClient : public HttpStreamTransport {
public:
    /// Constructor
    /// @param host Server hostname or IP address
    /// @param port Server port (default: 8080)
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds (default: 30000ms = 30s)
    HttpStreamClient(const fl::string& host, u16 port = 8080, u32 heartbeatIntervalMs = 30000);

    /// Virtual destructor
    ~HttpStreamClient() override;

    // Connection Management (override from HttpStreamTransport)

    /// Connect to HTTP server
    /// Sends initial HTTP POST request with headers
    /// @return true if connected successfully, false otherwise
    bool connect() override;

    /// Disconnect from HTTP server
    void disconnect() override;

    /// Check if connected to server
    /// @return true if connected, false otherwise
    bool isConnected() const override;

protected:
    // Abstract methods implementation (from HttpStreamTransport)

    /// Send raw data over socket
    /// @param data Data to send
    /// @return Number of bytes sent, or -1 on error
    int sendData(fl::span<const u8> data) override;

    /// Receive raw data from socket
    /// @param buffer Buffer to receive data into
    /// @return Number of bytes received, or -1 on error
    int recvData(fl::span<u8> buffer) override;

    /// Trigger reconnection (override from HttpStreamTransport)
    void triggerReconnect() override;

private:
    /// Send HTTP POST request header
    /// Called once at connection time
    /// @return true if sent successfully, false otherwise
    bool sendHttpRequestHeader();

    /// Read and validate HTTP response header
    /// Called once after connection
    /// @return true if valid response header received, false otherwise
    bool readHttpResponseHeader();

    /// Native socket client
    fl::unique_ptr<NativeHttpClient> mNativeClient;

    /// Connection state
    bool mHttpHeaderSent;
    bool mHttpHeaderReceived;

    /// Server address
    fl::string mHost;
    u16 mPort;
};

}  // namespace fl
