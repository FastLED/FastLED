#pragma once

#include "stream_server.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include "fl/warn.h"
#include "fl/stl/cstdio.h"
namespace fl {

HttpStreamServer::HttpStreamServer(u16 port, u32 heartbeatIntervalMs)
    : HttpStreamTransport("0.0.0.0", port, heartbeatIntervalMs)
    , mPort(port)
    , mLastProcessedClientId(0) {
    // Create native server with default connection config
    ConnectionConfig config;
    mNativeServer = fl::make_unique<NativeHttpServer>(mPort, config);

    // Pre-allocate receive buffer (16KB should be enough for most requests)
    mRecvBuffer.reserve(16384);
}

HttpStreamServer::~HttpStreamServer() {
    disconnect();
}

bool HttpStreamServer::connect() {
    // If already listening, return true
    if (isConnected()) {
        return true;
    }

    // Start listening
    if (!mNativeServer->start()) {
        return false;
    }

    // Enable non-blocking mode so accept() and recv() don't block the server thread.
    // Without this, the accept() loop in NativeHttpServer::acceptClients() blocks
    // forever after accepting the first client, preventing HTTP header processing.
    mNativeServer->setNonBlocking(true);

    // Mark connection as established in base class
    mConnection.onConnected();

    return true;
}

void HttpStreamServer::disconnect() {
    if (mNativeServer) {
        mNativeServer->stop();
    }
    mClientStates.clear();
    mConnection.onDisconnected();
}

bool HttpStreamServer::isConnected() const {
    return mNativeServer && mNativeServer->isListening();
}

void HttpStreamServer::acceptClients() {
    if (!isConnected()) {
        return;
    }

    // Accept new clients (non-blocking)
    mNativeServer->acceptClients();

    // Get list of all client IDs
    fl::vector<u32> clientIds = mNativeServer->getClientIds();

    // Process each client's HTTP header if not already done
    for (u32 clientId : clientIds) {
        ClientState* state = getOrCreateClientState(clientId);
        if (!state) {
            continue;
        }

        // If HTTP headers not exchanged yet, do it now
        if (!state->httpHeaderReceived) {
            if (readHttpRequestHeader(clientId)) {
                // Send HTTP response header
                if (!sendHttpResponseHeader(clientId)) {
                    // Failed to send response, disconnect client
                    disconnectClient(clientId);
                }
            }
        }
    }

    // Clean up disconnected clients
    fl::vector<u32> activeClientIds = mNativeServer->getClientIds();
    fl::vector<u32> toRemove;

    for (auto& pair : mClientStates) {
        u32 clientId = pair.first;
        bool found = false;
        for (u32 activeId : activeClientIds) {
            if (activeId == clientId) {
                found = true;
                break;
            }
        }
        if (!found) {
            toRemove.push_back(clientId);
        }
    }

    for (u32 clientId : toRemove) {
        removeClientState(clientId);
    }
}

size_t HttpStreamServer::getClientCount() const {
    return mNativeServer ? mNativeServer->getClientCount() : 0;
}

void HttpStreamServer::disconnectClient(u32 clientId) {
    if (mNativeServer) {
        mNativeServer->disconnectClient(clientId);
    }
    removeClientState(clientId);
}

fl::vector<u32> HttpStreamServer::getClientIds() const {
    return mNativeServer ? mNativeServer->getClientIds() : fl::vector<u32>();
}

int HttpStreamServer::sendData(const u8* data, size_t length) {
    if (!isConnected()) {
        return -1;
    }

    // Broadcast to all clients
    mNativeServer->broadcast(data, length);
    return static_cast<int>(length);
}

int HttpStreamServer::recvData(u8* buffer, size_t maxLength) {
    if (!isConnected()) {
        return -1;
    }

    // Get list of all client IDs
    fl::vector<u32> clientIds = mNativeServer->getClientIds();
    if (clientIds.empty()) {
        return 0;  // No clients connected
    }

    // Round-robin through clients to avoid starvation
    // Start from the last processed client + 1
    size_t startIdx = 0;
    for (size_t i = 0; i < clientIds.size(); i++) {
        if (clientIds[i] == mLastProcessedClientId) {
            startIdx = (i + 1) % clientIds.size();
            break;
        }
    }

    // Try each client starting from startIdx
    for (size_t offset = 0; offset < clientIds.size(); offset++) {
        size_t idx = (startIdx + offset) % clientIds.size();
        u32 clientId = clientIds[idx];

        ClientState* state = getOrCreateClientState(clientId);
        if (!state || !state->httpHeaderReceived || !state->httpHeaderSent) {
            continue;  // Skip clients that haven't completed HTTP handshake
        }

        // Try to receive data from this client
        int received = mNativeServer->recv(clientId, buffer, maxLength);
        if (received > 0) {
            mLastProcessedClientId = clientId;
            return received;
        }
    }

    return 0;  // No data available from any client
}

void HttpStreamServer::triggerReconnect() {
    // For server: disconnect all clients and restart
    disconnect();
    connect();
}

bool HttpStreamServer::readHttpRequestHeader(u32 clientId) {
    ClientState* state = getOrCreateClientState(clientId);
    if (!state) {
        return false;
    }

    if (state->httpHeaderReceived) {
        return true;  // Already received
    }

    // Read HTTP request header in chunks, accumulating in state->headerBuffer
    // across multiple calls (non-blocking sockets may return partial data)
    u8 buffer[256];

    // Maximum header size: 8KB (generous for HTTP headers)
    const size_t MAX_HEADER_SIZE = 8192;

    while (state->headerBuffer.size() < MAX_HEADER_SIZE) {
        int received = mNativeServer->recv(clientId, buffer, sizeof(buffer));
        if (received < 0) {
            return false;
        }
        if (received == 0) {
            return false;
        }

        state->headerBuffer.append(reinterpret_cast<const char*>(buffer), received); // ok reinterpret cast

        // Check for \r\n\r\n pattern (end of headers)
        if (state->headerBuffer.size() >= 4) {
            size_t pos = state->headerBuffer.find("\r\n\r\n");
            if (pos != fl::string::npos) {
                break;
            }
        }
    }

    // Validate the header directly (don't use HttpRequestParser which waits for
    // the chunked body to complete — we only need headers for the handshake).
    const fl::string& hdr = state->headerBuffer;

    // Must start with "POST /rpc"
    if (hdr.find("POST /rpc") != 0) {
        return false;
    }

    // Must have Content-Type: application/json (case-insensitive check)
    if (hdr.find("Content-Type: application/json") == fl::string::npos &&
        hdr.find("content-type: application/json") == fl::string::npos) {
        return false;
    }

    // Must have Transfer-Encoding: chunked (case-insensitive check)
    if (hdr.find("Transfer-Encoding: chunked") == fl::string::npos &&
        hdr.find("transfer-encoding: chunked") == fl::string::npos) {
        return false;
    }

    state->httpHeaderReceived = true;
    return true;
}

bool HttpStreamServer::sendHttpResponseHeader(u32 clientId) {
    ClientState* state = getOrCreateClientState(clientId);
    if (!state) {
        return false;
    }

    if (state->httpHeaderSent) {
        return true;  // Already sent
    }

    // Build HTTP 200 OK response header
    fl::string header;
    header.append("HTTP/1.1 200 OK\r\n");
    header.append("Content-Type: application/json\r\n");
    header.append("Transfer-Encoding: chunked\r\n");
    header.append("Connection: keep-alive\r\n");
    header.append("\r\n");

    // Send header
    int sent = mNativeServer->send(clientId, reinterpret_cast<const u8*>(header.c_str()), header.size()); // ok reinterpret cast
    if (sent != static_cast<int>(header.size())) {
        return false;
    }

    state->httpHeaderSent = true;
    return true;
}

HttpStreamServer::ClientState* HttpStreamServer::getOrCreateClientState(u32 clientId) {
    // Check if state already exists
    auto it = mClientStates.find(clientId);
    if (it != mClientStates.end()) {
        return &it->second;
    }

    // Create new state
    mClientStates.insert(fl::make_pair(clientId, ClientState(clientId)));

    // Find and return the newly created state
    it = mClientStates.find(clientId);
    if (it != mClientStates.end()) {
        return &it->second;
    }

    return nullptr;
}

void HttpStreamServer::removeClientState(u32 clientId) {
    auto it = mClientStates.find(clientId);
    if (it != mClientStates.end()) {
        mClientStates.erase(it);
    }
}

}  // namespace fl
