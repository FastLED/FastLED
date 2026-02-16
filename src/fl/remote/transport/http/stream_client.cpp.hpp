#pragma once

#include "stream_client.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include <cstdio>  // IWYU pragma: keep

namespace fl {

HttpStreamClient::HttpStreamClient(const char* host, u16 port, u32 heartbeatIntervalMs)
    : HttpStreamTransport(host, port, heartbeatIntervalMs)
    , mHttpHeaderSent(false)
    , mHttpHeaderReceived(false)
    , mHost(host)
    , mPort(port) {
    // Create native client with default connection config
    ConnectionConfig config;
    mNativeClient = fl::make_unique<NativeHttpClient>(mHost, mPort, config);
}

HttpStreamClient::~HttpStreamClient() {
    disconnect();
}

bool HttpStreamClient::connect() {
    // If already connected, return true
    if (isConnected()) {
        return true;
    }

    // Reset HTTP state
    mHttpHeaderSent = false;
    mHttpHeaderReceived = false;

    // Connect native socket
    if (!mNativeClient->connect()) {
        return false;
    }

    // Send HTTP POST request header
    if (!sendHttpRequestHeader()) {
        mNativeClient->disconnect();
        return false;
    }

    // Read HTTP response header
    if (!readHttpResponseHeader()) {
        mNativeClient->disconnect();
        return false;
    }

    // Mark connection as established in base class
    mConnection.onConnected();

    return true;
}

void HttpStreamClient::disconnect() {
    if (mNativeClient) {
        mNativeClient->disconnect();
    }
    mHttpHeaderSent = false;
    mHttpHeaderReceived = false;
    mConnection.onDisconnected();
}

bool HttpStreamClient::isConnected() const {
    return mNativeClient && mNativeClient->isConnected() && mHttpHeaderSent && mHttpHeaderReceived;
}

int HttpStreamClient::sendData(const u8* data, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    return mNativeClient->send(data, length);
}

int HttpStreamClient::recvData(u8* buffer, size_t maxLength) {
    if (!isConnected()) {
        return -1;
    }
    return mNativeClient->recv(buffer, maxLength);
}

void HttpStreamClient::triggerReconnect() {
    // Disconnect and let the base class reconnection logic handle it
    disconnect();
}

bool HttpStreamClient::sendHttpRequestHeader() {
    // Build HTTP POST request header
    // Format:
    // POST /rpc HTTP/1.1
    // Host: <host>:<port>
    // Content-Type: application/json
    // Transfer-Encoding: chunked
    // Connection: keep-alive
    // \r\n

    fl::string header;
    header.append("POST /rpc HTTP/1.1\r\n");
    header.append("Host: ");
    header.append(mHost);
    if (mPort != 80) {
        header.append(":");
        char portStr[8];
        snprintf(portStr, sizeof(portStr), "%u", mPort);
        header.append(portStr);
    }
    header.append("\r\n");
    header.append("Content-Type: application/json\r\n");
    header.append("Transfer-Encoding: chunked\r\n");
    header.append("Connection: keep-alive\r\n");
    header.append("\r\n");

    // Send header
    int sent = mNativeClient->send(reinterpret_cast<const u8*>(header.c_str()), header.size()); // ok reinterpret cast
    if (sent != static_cast<int>(header.size())) {
        return false;
    }

    mHttpHeaderSent = true;
    return true;
}

bool HttpStreamClient::readHttpResponseHeader() {
    // Read HTTP response header
    // Expected format:
    // HTTP/1.1 200 OK
    // Content-Type: application/json
    // Transfer-Encoding: chunked
    // Connection: keep-alive
    // \r\n

    // Read until we find \r\n\r\n (end of headers)
    fl::string headerBuffer;
    u8 buffer[1];

    // Read byte by byte until we get \r\n\r\n
    // This is simple but not efficient - for production, use a proper parser
    // Maximum header size: 4KB
    const size_t MAX_HEADER_SIZE = 4096;

    while (headerBuffer.size() < MAX_HEADER_SIZE) {
        int received = mNativeClient->recv(buffer, 1);
        if (received <= 0) {
            return false;
        }

        headerBuffer.append(reinterpret_cast<const char*>(buffer), 1); // ok reinterpret cast

        // Check for \r\n\r\n pattern
        if (headerBuffer.size() >= 4) {
            size_t pos = headerBuffer.size() - 4;
            if (headerBuffer[pos] == '\r' && headerBuffer[pos + 1] == '\n' &&
                headerBuffer[pos + 2] == '\r' && headerBuffer[pos + 3] == '\n') {
                break;
            }
        }
    }

    // Validate response
    // Must start with "HTTP/1.1 200"
    if (headerBuffer.size() < 12) {
        return false;
    }

    if (headerBuffer.substr(0, 12) != "HTTP/1.1 200") {
        return false;
    }

    // Check for required headers (case-insensitive)
    // We're being lenient here - just check that the connection is valid
    // The chunked encoding will handle the actual data parsing

    // Look for Transfer-Encoding: chunked
    bool hasChunked = false;
    if (headerBuffer.find("Transfer-Encoding: chunked") != fl::string::npos ||
        headerBuffer.find("transfer-encoding: chunked") != fl::string::npos) {
        hasChunked = true;
    }

    if (!hasChunked) {
        return false;
    }

    mHttpHeaderReceived = true;
    return true;
}

}  // namespace fl
