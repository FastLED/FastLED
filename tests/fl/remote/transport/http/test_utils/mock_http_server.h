#pragma once

#include "fl/remote/transport/http/stream_transport.h"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/map.h"

namespace fl {

/// Mock HTTP server for unit testing
/// In-memory implementation without actual sockets
/// Stores raw bytes in/out queues shared with clients
/// HttpStreamTransport handles chunked encoding on top
class MockHttpServer : public HttpStreamTransport {
public:
    /// Constructor
    /// @param port Server port (ignored for mock)
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds
    MockHttpServer(uint16_t port = 8080, uint32_t heartbeatIntervalMs = 30000)
        : HttpStreamTransport("localhost", port, heartbeatIntervalMs)
        , mListening(false)
        , mCurrentTime(0)
        , mNextClientId(1)
    {
    }

    /// Virtual destructor
    ~MockHttpServer() override = default;

    // Connection Management (override from HttpStreamTransport)

    /// Start server (start listening)
    bool connect() override {
        if (mListening) {
            return true;
        }
        mListening = true;
        mConnection.onConnected(getCurrentTimeMs());
        return true;
    }

    /// Stop server (disconnect all clients)
    void disconnect() override {
        if (!mListening) {
            return;
        }
        mListening = false;
        mClientQueues.clear();
        mConnection.onDisconnected();
    }

    /// Check if server is listening
    bool isConnected() const override {
        return mListening;
    }

    // Mock-specific methods

    /// Simulate client connection
    /// @return Client ID
    uint32_t acceptClient() {
        if (!mListening) {
            return 0;
        }
        uint32_t clientId = mNextClientId++;
        mClientQueues[clientId] = ClientQueue();
        return clientId;
    }

    /// Disconnect specific client
    /// @param clientId Client ID
    void disconnectClient(uint32_t clientId) {
        mClientQueues.erase(clientId);
    }

    /// Get number of connected clients
    size_t getClientCount() const {
        return mClientQueues.size();
    }

    /// Get pointer to client's receive queue (for client to write to)
    /// @param clientId Client ID
    /// @return Pointer to queue, or nullptr if not found
    fl::vector<uint8_t>* getClientRecvQueue(uint32_t clientId) {
        if (mClientQueues.find(clientId) == mClientQueues.end()) {
            return nullptr;
        }
        return &mClientQueues[clientId].recvData;
    }

    /// Get pointer to client's send queue (for client to read from)
    /// @param clientId Client ID
    /// @return Pointer to queue, or nullptr if not found
    fl::vector<uint8_t>* getClientSendQueue(uint32_t clientId) {
        if (mClientQueues.find(clientId) == mClientQueues.end()) {
            return nullptr;
        }
        return &mClientQueues[clientId].sentData;
    }

    // Time management (for testing)

    uint32_t getCurrentTimeMs() const override {
        return mCurrentTime;
    }

    void setCurrentTime(uint32_t time) {
        mCurrentTime = time;
    }

    void advanceTime(uint32_t delta) {
        mCurrentTime += delta;
    }

protected:
    /// Send data to all clients (broadcast)
    int sendData(const uint8_t* data, size_t length) override {
        if (!mListening) {
            return -1;
        }

        // Broadcast to all clients
        for (auto& pair : mClientQueues) {
            pair.second.sentData.insert(pair.second.sentData.end(), data, data + length);
        }

        return static_cast<int>(length);
    }

    /// Receive data from any client (round-robin)
    int recvData(uint8_t* buffer, size_t maxLength) override {
        if (!mListening || mClientQueues.empty()) {
            return 0;
        }

        // Try each client in sequence
        for (auto& pair : mClientQueues) {
            fl::vector<uint8_t>& recvQueue = pair.second.recvData;
            if (!recvQueue.empty()) {
                size_t toCopy = (recvQueue.size() < maxLength) ? recvQueue.size() : maxLength;
                fl::memcpy(buffer, recvQueue.data(), toCopy);

                // Remove consumed data (fl::vector doesn't have range erase, so create new vector)
                fl::vector<uint8_t> remaining;
                if (toCopy < recvQueue.size()) {
                    remaining.reserve(recvQueue.size() - toCopy);
                    for (size_t i = toCopy; i < recvQueue.size(); i++) {
                        remaining.push_back(recvQueue[i]);
                    }
                }
                recvQueue = fl::move(remaining);

                return static_cast<int>(toCopy);
            }
        }

        return 0;  // No data available
    }

private:
    /// Client queue (recv/sent data per client)
    struct ClientQueue {
        fl::vector<uint8_t> recvData;  // Data from client → server
        fl::vector<uint8_t> sentData;  // Data from server → client
    };

    bool mListening;
    uint32_t mCurrentTime;
    uint32_t mNextClientId;
    fl::map<uint32_t, ClientQueue> mClientQueues;
};

}  // namespace fl
