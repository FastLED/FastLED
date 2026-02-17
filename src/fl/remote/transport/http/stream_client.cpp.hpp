#pragma once

#include "stream_client.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include "fl/warn.h"
#include <cstdio>  // IWYU pragma: keep
#include <thread>  // IWYU pragma: keep

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
    FL_WARN("HttpStreamClient::connect() called for " << mHost.c_str() << ":" << mPort);

    // If already connected, return true
    if (isConnected()) {
        FL_WARN("HttpStreamClient::connect() already connected");
        return true;
    }

    // Reset HTTP state
    mHttpHeaderSent = false;
    mHttpHeaderReceived = false;

    // Connect native socket
    FL_WARN("HttpStreamClient: Connecting socket to " << mHost.c_str() << ":" << mPort << "...");
    if (!mNativeClient->connect()) {
        FL_WARN("HttpStreamClient: Socket connection FAILED");
        return false;
    }
    FL_WARN("HttpStreamClient: Socket connected successfully");

    // Send HTTP POST request header
    FL_WARN("HttpStreamClient: Sending HTTP request header...");
    if (!sendHttpRequestHeader()) {
        FL_WARN("HttpStreamClient: Failed to send HTTP request header");
        mNativeClient->disconnect();
        return false;
    }
    FL_WARN("HttpStreamClient: HTTP request header sent");

    // Read HTTP response header
    FL_WARN("HttpStreamClient: Reading HTTP response header...");
    if (!readHttpResponseHeader()) {
        FL_WARN("HttpStreamClient: Failed to read HTTP response header");
        mNativeClient->disconnect();
        return false;
    }
    FL_WARN("HttpStreamClient: HTTP response header received, connection established");

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

    FL_WARN("HttpStreamClient: Waiting for HTTP response header from server...");

    // Read until we find \r\n\r\n (end of headers)
    fl::string headerBuffer;
    u8 buffer[256];  // Read in chunks instead of byte-by-byte

    // Maximum header size: 4KB, max retries when no data (yield to server thread)
    const size_t MAX_HEADER_SIZE = 4096;
    const int MAX_READ_ATTEMPTS = 500;  // 500 yields before giving up
    int readAttempts = 0;

    while (headerBuffer.size() < MAX_HEADER_SIZE) {
        int received = mNativeClient->recv(buffer, sizeof(buffer));

        if (received < 0) {
            FL_WARN("HttpStreamClient: recv() error, aborting");
            return false;
        }

        if (received == 0) {
            // No data yet - server may still be processing the request
            // Yield to allow other threads (e.g. server thread) to run
            readAttempts++;
            if (readAttempts >= MAX_READ_ATTEMPTS) {
                FL_WARN("HttpStreamClient: Timeout waiting for HTTP response header");
                return false;
            }
            // Sleep briefly to allow server thread to run
            std::this_thread::yield();
            continue;
        }

        headerBuffer.append(reinterpret_cast<const char*>(buffer), received); // ok reinterpret cast
        FL_WARN("HttpStreamClient: Received " << received << " bytes, total header size: " << headerBuffer.size());

        // Check for \r\n\r\n pattern
        if (headerBuffer.size() >= 4) {
            size_t pos = headerBuffer.find("\r\n\r\n");
            if (pos != fl::string::npos) {
                FL_WARN("HttpStreamClient: Found end of HTTP headers at position " << pos);
                break;
            }
        }
    }

    FL_WARN("HttpStreamClient: Complete header received (" << headerBuffer.size() << " bytes):\n" << headerBuffer.c_str());

    // Validate response
    // Must start with "HTTP/1.1 200"
    if (headerBuffer.size() < 12) {
        FL_WARN("HttpStreamClient: Header too short (" << headerBuffer.size() << " bytes)");
        return false;
    }

    if (headerBuffer.substr(0, 12) != "HTTP/1.1 200") {
        FL_WARN("HttpStreamClient: Invalid HTTP status line: " << headerBuffer.substr(0, 12).c_str());
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
        FL_WARN("HttpStreamClient: Missing 'Transfer-Encoding: chunked' header");
        return false;
    }

    FL_WARN("HttpStreamClient: HTTP response header validated successfully");
    mHttpHeaderReceived = true;
    return true;
}

}  // namespace fl
