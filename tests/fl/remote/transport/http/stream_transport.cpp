#include "test.h"
#include "fl/remote/transport/http/stream_transport.h"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include <cstring>  // ok include

using namespace fl;

// Mock implementation for testing
class MockStreamTransport : public HttpStreamTransport {
public:
    MockStreamTransport(const char* host, uint16_t port)
        : HttpStreamTransport(host, port, 1000)  // 1s heartbeat for testing
        , mConnected(false)
        , mCurrentTime(0) {
    }

    bool connect() override {
        mConnected = true;
        mConnection.onConnected(getCurrentTimeMs());
        return true;
    }

    void disconnect() override {
        mConnected = false;
        mConnection.onDisconnected();
    }

    bool isConnected() const override {
        return mConnected;
    }

    // Expose sendData for testing
    int sendData(const uint8_t* data, size_t length) override {
        mSentData.insert(mSentData.end(), data, data + length);
        return static_cast<int>(length);
    }

    // Expose recvData for testing
    int recvData(uint8_t* buffer, size_t maxLength) override {
        if (mRecvData.empty()) {
            return 0;  // No data available
        }

        size_t toCopy = (mRecvData.size() < maxLength) ? mRecvData.size() : maxLength;
        fl::memcpy(buffer, mRecvData.data(), toCopy);
        mRecvData.erase(mRecvData.begin(), mRecvData.begin() + toCopy);
        return static_cast<int>(toCopy);
    }

    uint32_t getCurrentTimeMs() const override {
        return mCurrentTime;
    }

    void setCurrentTime(uint32_t time) {
        mCurrentTime = time;
    }

    void advanceTime(uint32_t delta) {
        mCurrentTime += delta;
    }

    // Test helpers
    void injectRecvData(const uint8_t* data, size_t length) {
        mRecvData.insert(mRecvData.end(), data, data + length);
    }

    void injectRecvChunk(const char* jsonStr) {
        // Format as chunked data
        size_t len = fl::strlen(jsonStr);
        char hexSize[32];
        fl::snprintf(hexSize, sizeof(hexSize), "%zX\r\n", len);

        injectRecvData(reinterpret_cast<const uint8_t*>(hexSize), fl::strlen(hexSize));
        injectRecvData(reinterpret_cast<const uint8_t*>(jsonStr), len);
        injectRecvData(reinterpret_cast<const uint8_t*>("\r\n"), 2);
    }

    fl::vector<uint8_t> getSentData() const {
        return mSentData;
    }

    void clearSentData() {
        mSentData.clear();
    }

private:
    bool mConnected;
    fl::vector<uint8_t> mSentData;
    fl::vector<uint8_t> mRecvData;
    uint32_t mCurrentTime;
};

FL_TEST_CASE("HttpStreamTransport: Constructor") {
    MockStreamTransport transport("localhost", 8080);
    FL_CHECK(!transport.isConnected());
    FL_CHECK(transport.getHeartbeatInterval() == 1000);  // Set in constructor
}

FL_TEST_CASE("HttpStreamTransport: Connect/Disconnect") {
    MockStreamTransport transport("localhost", 8080);

    FL_SUBCASE("Initial state") {
        FL_CHECK(!transport.isConnected());
    }

    FL_SUBCASE("Connect") {
        FL_CHECK(transport.connect());
        FL_CHECK(transport.isConnected());
    }

    FL_SUBCASE("Disconnect") {
        transport.connect();
        transport.disconnect();
        FL_CHECK(!transport.isConnected());
    }
}

FL_TEST_CASE("HttpStreamTransport: Read Request") {
    MockStreamTransport transport("localhost", 8080);
    transport.connect();

    FL_SUBCASE("No data available") {
        auto request = transport.readRequest();
        FL_CHECK(!request);
    }

    FL_SUBCASE("Read valid JSON-RPC request") {
        const char* json = R"({"jsonrpc":"2.0","method":"add","params":[1,2],"id":1})";
        transport.injectRecvChunk(json);

        auto request = transport.readRequest();
        FL_REQUIRE(request);
        FL_CHECK((*request)["jsonrpc"].as_string() == "2.0");
        FL_CHECK((*request)["method"].as_string() == "add");
        FL_CHECK((*request)["id"].as_int() == 1);
    }

    FL_SUBCASE("Filter out heartbeat (rpc.ping)") {
        const char* json = R"({"jsonrpc":"2.0","method":"rpc.ping","id":null})";
        transport.injectRecvChunk(json);

        auto request = transport.readRequest();
        FL_CHECK(!request);  // Heartbeat should be filtered
    }

    FL_SUBCASE("Read disconnected") {
        transport.disconnect();
        auto request = transport.readRequest();
        FL_CHECK(!request);
    }
}

FL_TEST_CASE("HttpStreamTransport: Write Response") {
    MockStreamTransport transport("localhost", 8080);
    transport.connect();

    FL_SUBCASE("Write valid JSON-RPC response") {
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", 3);
        response.set("id", 1);

        transport.writeResponse(response);

        auto sent = transport.getSentData();
        FL_CHECK(!sent.empty());

        // Parse sent data as chunked encoding
        fl::string sentStr(reinterpret_cast<const char*>(sent.data()), sent.size());
        FL_CHECK(sentStr.find("jsonrpc") != fl::string::npos);
    }

    FL_SUBCASE("Write disconnected does nothing") {
        transport.disconnect();

        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", 3);
        response.set("id", 1);

        transport.writeResponse(response);

        auto sent = transport.getSentData();
        FL_CHECK(sent.empty());
    }
}

FL_TEST_CASE("HttpStreamTransport: Heartbeat") {
    MockStreamTransport transport("localhost", 8080);
    transport.connect();
    transport.setCurrentTime(0);
    transport.clearSentData();

    FL_SUBCASE("Send heartbeat after interval") {
        transport.update(0);
        FL_CHECK(transport.getSentData().empty());  // No heartbeat yet

        transport.advanceTime(500);
        transport.update(500);
        FL_CHECK(transport.getSentData().empty());  // Still no heartbeat (< 1s)

        transport.advanceTime(600);
        transport.update(1100);
        FL_CHECK(!transport.getSentData().empty());  // Heartbeat sent (>= 1s)

        // Verify heartbeat contains "rpc.ping"
        auto sent = transport.getSentData();
        fl::string sentStr(reinterpret_cast<const char*>(sent.data()), sent.size());
        FL_CHECK(sentStr.find("rpc.ping") != fl::string::npos);
    }

    FL_SUBCASE("Reset heartbeat after sending data") {
        // Send response
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", 42);
        response.set("id", 1);
        transport.writeResponse(response);
        transport.clearSentData();

        // Advance time less than interval
        transport.advanceTime(500);
        transport.update(500);
        FL_CHECK(transport.getSentData().empty());  // No heartbeat (recent send)
    }
}

FL_TEST_CASE("HttpStreamTransport: Heartbeat Timeout") {
    MockStreamTransport transport("localhost", 8080);
    transport.setTimeout(2000);  // 2s timeout
    transport.connect();
    transport.setCurrentTime(0);

    FL_SUBCASE("No timeout with regular data") {
        transport.update(0);
        FL_CHECK(transport.isConnected());

        // Receive data within timeout
        transport.advanceTime(1500);
        const char* json = R"({"jsonrpc":"2.0","method":"add","params":[],"id":1})";
        transport.injectRecvChunk(json);
        transport.update(1500);
        FL_CHECK(transport.isConnected());  // Still connected
    }

    FL_SUBCASE("Timeout with no data") {
        transport.update(0);
        FL_CHECK(transport.isConnected());

        // No data received for 2s
        transport.advanceTime(2100);
        transport.update(2100);
        FL_CHECK(!transport.isConnected());  // Disconnected due to timeout
    }
}

// Helper functions for callbacks
static bool g_connectCalled = false;
static bool g_disconnectCalled = false;

static void onConnectCallback() {
    g_connectCalled = true;
}

static void onDisconnectCallback() {
    g_disconnectCalled = true;
}

FL_TEST_CASE("HttpStreamTransport: Connection Callbacks") {
    MockStreamTransport transport("localhost", 8080);

    g_connectCalled = false;
    g_disconnectCalled = false;

    transport.setOnConnect(onConnectCallback);
    transport.setOnDisconnect(onDisconnectCallback);

    FL_SUBCASE("onConnect callback") {
        FL_CHECK(!g_connectCalled);
        transport.connect();
        transport.update(0);
        FL_CHECK(g_connectCalled);
    }

    FL_SUBCASE("onDisconnect callback") {
        transport.connect();
        transport.update(0);
        g_connectCalled = false;

        FL_CHECK(!g_disconnectCalled);
        transport.disconnect();
        transport.update(0);
        FL_CHECK(g_disconnectCalled);
    }
}

FL_TEST_CASE("HttpStreamTransport: Configuration") {
    MockStreamTransport transport("localhost", 8080);

    FL_SUBCASE("Heartbeat interval") {
        FL_CHECK(transport.getHeartbeatInterval() == 1000);
        transport.setHeartbeatInterval(5000);
        FL_CHECK(transport.getHeartbeatInterval() == 5000);
    }

    FL_SUBCASE("Timeout") {
        FL_CHECK(transport.getTimeout() == 60000);  // Default 60s
        transport.setTimeout(10000);
        FL_CHECK(transport.getTimeout() == 10000);
    }
}

FL_TEST_CASE("HttpStreamTransport: Multiple Requests") {
    MockStreamTransport transport("localhost", 8080);
    transport.connect();

    FL_SUBCASE("Read multiple requests in sequence") {
        const char* json1 = R"({"jsonrpc":"2.0","method":"add","params":[1,2],"id":1})";
        const char* json2 = R"({"jsonrpc":"2.0","method":"subtract","params":[5,3],"id":2})";

        transport.injectRecvChunk(json1);
        transport.injectRecvChunk(json2);

        auto request1 = transport.readRequest();
        FL_REQUIRE(request1);
        FL_CHECK((*request1)["method"].as_string() == "add");
        FL_CHECK((*request1)["id"].as_int() == 1);

        auto request2 = transport.readRequest();
        FL_REQUIRE(request2);
        FL_CHECK((*request2)["method"].as_string() == "subtract");
        FL_CHECK((*request2)["id"].as_int() == 2);
    }
}

FL_TEST_CASE("HttpStreamTransport: Error Handling") {
    MockStreamTransport transport("localhost", 8080);
    transport.connect();

    FL_SUBCASE("Invalid JSON ignored") {
        const char* invalid = R"({invalid json})";
        transport.injectRecvChunk(invalid);

        auto request = transport.readRequest();
        FL_CHECK(!request);  // Invalid JSON ignored
    }

    FL_SUBCASE("Partial chunk ignored") {
        // Inject partial chunk (no complete chunk available)
        const uint8_t partial[] = "10\r\npartial data";
        transport.injectRecvData(partial, sizeof(partial) - 1);

        auto request = transport.readRequest();
        FL_CHECK(!request);  // Partial chunk, no complete request
    }
}
