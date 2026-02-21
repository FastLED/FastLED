#pragma once

#include "fl/remote/transport/http/stream_transport.h"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "mock_http_server.h"

namespace fl {

/// Mock HTTP client for unit testing
/// In-memory implementation without actual sockets
/// Connects to MockHttpServer via shared byte queues
/// HttpStreamTransport handles chunked encoding on top
class MockHttpClient : public HttpStreamTransport {
public:
    /// Constructor
    /// @param server Mock server to connect to
    /// @param heartbeatIntervalMs Heartbeat interval in milliseconds
    MockHttpClient(MockHttpServer& server, uint32_t heartbeatIntervalMs = 30000)
        : HttpStreamTransport("localhost", 47701, heartbeatIntervalMs)
        , mServer(server)
        , mClientId(0)
        , mConnected(false)
        , mCurrentTime(0)
    {
    }

    /// Virtual destructor
    ~MockHttpClient() override {
        disconnect();
    }

    // Connection Management (override from HttpStreamTransport)

    /// Connect to server
    bool connect() override {
        if (mConnected) {
            return true;
        }

        // Simulate connection to server
        if (!mServer.isConnected()) {
            return false;  // Server not listening
        }

        mClientId = mServer.acceptClient();
        if (mClientId == 0) {
            return false;  // Failed to accept
        }

        mConnected = true;
        mConnection.onConnected(getCurrentTimeMs());
        return true;
    }

    /// Disconnect from server
    void disconnect() override {
        if (!mConnected) {
            return;
        }

        mServer.disconnectClient(mClientId);
        mClientId = 0;
        mConnected = false;
        mConnection.onDisconnected();
    }

    /// Check if connected to server
    bool isConnected() const override {
        return mConnected;
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
    /// Send data to server
    int sendData(const uint8_t* data, size_t length) override {
        if (!mConnected) {
            return -1;
        }

        // Write to server's receive queue
        fl::vector<uint8_t>* recvQueue = mServer.getClientRecvQueue(mClientId);
        if (!recvQueue) {
            return -1;
        }

        recvQueue->insert(recvQueue->end(), data, data + length);
        return static_cast<int>(length);
    }

    /// Receive data from server
    int recvData(uint8_t* buffer, size_t maxLength) override {
        if (!mConnected) {
            return -1;
        }

        // Read from server's send queue
        fl::vector<uint8_t>* sendQueue = mServer.getClientSendQueue(mClientId);
        if (!sendQueue || sendQueue->empty()) {
            return 0;  // No data available
        }

        size_t toCopy = (sendQueue->size() < maxLength) ? sendQueue->size() : maxLength;
        fl::memcpy(buffer, sendQueue->data(), toCopy);

        // Remove consumed data (fl::vector doesn't have range erase, so create new vector)
        fl::vector<uint8_t> remaining;
        if (toCopy < sendQueue->size()) {
            remaining.reserve(sendQueue->size() - toCopy);
            for (size_t i = toCopy; i < sendQueue->size(); i++) {
                remaining.push_back((*sendQueue)[i]);
            }
        }
        *sendQueue = fl::move(remaining);

        return static_cast<int>(toCopy);
    }

private:
    MockHttpServer& mServer;
    uint32_t mClientId;
    bool mConnected;
    uint32_t mCurrentTime;
};

}  // namespace fl
