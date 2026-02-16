#pragma once

#include "stream_transport.h"
#include "fl/json.h"
#include "fl/stl/string.h"
#include <cstring>  // IWYU pragma: keep

namespace fl {

HttpStreamTransport::HttpStreamTransport(const char* host, u16 port, u32 heartbeatIntervalMs)
    : mConnection(ConnectionConfig{})  // Use default config
    , mLastHeartbeatSent(0)
    , mLastHeartbeatReceived(0)
    , mHeartbeatInterval(heartbeatIntervalMs)
    , mTimeoutMs(60000)  // Default 60s timeout
    , mHost(host)
    , mPort(port)
    , mWasConnected(false)
    , mOnConnect(nullptr)
    , mOnDisconnect(nullptr) {
}

HttpStreamTransport::~HttpStreamTransport() {
    // Note: Cannot call pure virtual disconnect() here
    // Subclasses must clean up in their own destructors
}

fl::optional<fl::Json> HttpStreamTransport::readRequest() {
    if (!isConnected()) {
        return fl::nullopt;
    }

    // Process incoming data (best effort - may return false if no new data)
    processIncomingData();

    // Try to read a complete chunk (reader may have buffered chunks from previous feed)
    fl::optional<fl::vector<u8>> chunk = mReader.readChunk();
    if (!chunk) {
        return fl::nullopt;
    }

    // Parse JSON from chunk
    fl::string jsonStr(reinterpret_cast<const char*>(chunk->data()), chunk->size()); // ok reinterpret cast
    fl::Json json = fl::Json::parse(jsonStr.c_str());
    if (json.is_null()) {
        // Failed to parse JSON
        return fl::nullopt;
    }

    // Check if this is a heartbeat (rpc.ping)
    if (json["method"].as_string() == "rpc.ping") {
        mLastHeartbeatReceived = getCurrentTimeMs();
        // Don't return heartbeat as a request (filtered out)
        return fl::nullopt;
    }

    // Update last received time
    mLastHeartbeatReceived = getCurrentTimeMs();

    return json;
}

void HttpStreamTransport::writeResponse(const fl::Json& response) {
    if (!isConnected()) {
        return;
    }

    // Serialize JSON to string
    fl::string jsonStr = response.to_string();

    // Write as chunked data
    fl::vector<u8> chunked = mWriter.writeChunk(
        reinterpret_cast<const u8*>(jsonStr.c_str()), // ok reinterpret cast
        jsonStr.size()
    );

    // Send chunked data
    sendData(chunked.data(), chunked.size());

    // Update last sent time
    mLastHeartbeatSent = getCurrentTimeMs();
}

void HttpStreamTransport::update(u32 currentTimeMs) {
    // Update connection state
    mConnection.update(currentTimeMs);
    bool nowConnected = isConnected();

    // Handle state changes
    if (mWasConnected != nowConnected) {
        handleConnectionStateChange();
        mWasConnected = nowConnected;
    }

    if (!isConnected()) {
        // Attempt reconnection if needed
        if (mConnection.shouldReconnect()) {
            triggerReconnect();
        }
        return;
    }

    // Send heartbeat if needed
    u32 timeSinceLastSent = currentTimeMs - mLastHeartbeatSent;
    if (timeSinceLastSent >= mHeartbeatInterval) {
        sendHeartbeat();
    }

    // Check for heartbeat timeout
    checkHeartbeatTimeout(currentTimeMs);

    // Process incoming data
    processIncomingData();
}

void HttpStreamTransport::setOnConnect(StateCallback callback) {
    mOnConnect = callback;
}

void HttpStreamTransport::setOnDisconnect(StateCallback callback) {
    mOnDisconnect = callback;
}

void HttpStreamTransport::setHeartbeatInterval(u32 intervalMs) {
    mHeartbeatInterval = intervalMs;
}

u32 HttpStreamTransport::getHeartbeatInterval() const {
    return mHeartbeatInterval;
}

void HttpStreamTransport::setTimeout(u32 timeoutMs) {
    mTimeoutMs = timeoutMs;
}

u32 HttpStreamTransport::getTimeout() const {
    return mTimeoutMs;
}

u32 HttpStreamTransport::getCurrentTimeMs() const {
    // Default implementation uses system time
    // Subclasses can override for testing
    return fl::millis();
}

void HttpStreamTransport::triggerReconnect() {
    // Default implementation does nothing
    // Subclasses should override to implement reconnection logic
}

void HttpStreamTransport::sendHeartbeat() {
    if (!isConnected()) {
        return;
    }

    // Create heartbeat request (rpc.ping notification)
    fl::Json heartbeat = fl::Json::object();
    heartbeat.set("jsonrpc", "2.0");
    heartbeat.set("method", "rpc.ping");
    heartbeat.set("id", fl::Json());  // null value

    // Write as response (uses same chunked encoding)
    writeResponse(heartbeat);
}

void HttpStreamTransport::checkHeartbeatTimeout(u32 currentTimeMs) {
    u32 timeSinceLastReceived = currentTimeMs - mLastHeartbeatReceived;
    if (timeSinceLastReceived >= getTimeout()) {
        // Heartbeat timeout - connection is dead
        mConnection.onDisconnected();
        disconnect();
    }
}

bool HttpStreamTransport::processIncomingData() {
    if (!isConnected()) {
        return false;
    }

    // Read incoming data from socket
    u8 buffer[1024];
    int bytesRead = recvData(buffer, sizeof(buffer));

    if (bytesRead < 0) {
        // Error reading from socket
        mConnection.onDisconnected();
        disconnect();
        return false;
    }

    if (bytesRead == 0) {
        // No data available (non-blocking socket)
        return false;
    }

    // Feed data to chunked reader
    mReader.feed(buffer, bytesRead);

    return true;
}

void HttpStreamTransport::handleConnectionStateChange() {
    if (isConnected()) {
        // Just connected
        mLastHeartbeatSent = getCurrentTimeMs();
        mLastHeartbeatReceived = getCurrentTimeMs();
        if (mOnConnect) {
            mOnConnect();
        }
    } else {
        // Just disconnected
        if (mOnDisconnect) {
            mOnDisconnect();
        }
    }
}

}  // namespace fl
