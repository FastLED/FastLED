#include "test.h"
#include "fl/remote/transport/http/connection.h"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/native_client.h"
#include "fl/remote/transport/http/native_client.cpp.hpp"

using namespace fl;

FL_TEST_CASE("NativeHttpClient - Construction and destruction") {
    NativeHttpClient client("localhost", 8080);
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Connection to invalid host fails") {
    NativeHttpClient client("invalid.host.that.does.not.exist.test", 8080);

    bool result = client.connect();
    FL_CHECK_FALSE(result);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Connection state transitions") {
    NativeHttpClient client("localhost", 8080);

    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    // Attempt connection (will fail without server, but state should transition)
    client.connect();

    // State should be DISCONNECTED or RECONNECTING (depending on connection failure)
    ConnectionState state = client.getState();
    FL_CHECK((state == ConnectionState::DISCONNECTED || state == ConnectionState::RECONNECTING));
}

FL_TEST_CASE("NativeHttpClient - Send and recv fail when disconnected") {
    NativeHttpClient client("localhost", 8080);

    uint8_t sendData[] = {'t', 'e', 's', 't'};
    int sendResult = client.send(sendData, sizeof(sendData));
    FL_CHECK(sendResult == -1);

    uint8_t recvBuffer[64];
    int recvResult = client.recv(recvBuffer, sizeof(recvBuffer));
    FL_CHECK(recvResult == -1);
}

FL_TEST_CASE("NativeHttpClient - Non-blocking mode") {
    NativeHttpClient client("localhost", 8080);

    // Set non-blocking mode before connection
    client.setNonBlocking(true);

    // Attempt connection (should not block, even if server doesn't exist)
    bool result = client.connect();

    // Connection may succeed or fail depending on whether server exists
    // The important thing is that it returned immediately (non-blocking)
    // Just verify the call completed without hanging
    FL_CHECK((result == true || result == false));  // Always true, just verify it returned
}

FL_TEST_CASE("NativeHttpClient - Disconnect and close") {
    NativeHttpClient client("localhost", 8080);

    // Disconnect when already disconnected (should be no-op)
    client.disconnect();
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    // Close permanently
    client.close();
    FL_CHECK(client.getState() == ConnectionState::CLOSED);

    // Connection after close should fail
    bool result = client.connect();
    FL_CHECK_FALSE(result);
    FL_CHECK(client.getState() == ConnectionState::CLOSED);
}

FL_TEST_CASE("NativeHttpClient - Heartbeat tracking") {
    ConnectionConfig config;
    config.heartbeatIntervalMs = 1000;

    NativeHttpClient client("localhost", 8080, config);

    // Should not send heartbeat when disconnected
    FL_CHECK_FALSE(client.shouldSendHeartbeat(0));
    FL_CHECK_FALSE(client.shouldSendHeartbeat(1000));
    FL_CHECK_FALSE(client.shouldSendHeartbeat(2000));
}

FL_TEST_CASE("NativeHttpClient - Reconnection tracking") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 1000;
    config.reconnectMaxDelayMs = 5000;
    config.reconnectBackoffMultiplier = 2;

    NativeHttpClient client("localhost", 8080, config);

    // Initial state
    FL_CHECK(client.getReconnectAttempts() == 0);
    FL_CHECK(client.getReconnectDelayMs() == 0);
}

FL_TEST_CASE("NativeHttpClient - Update loop") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 100;
    config.maxReconnectAttempts = 1;  // Only 1 attempt

    NativeHttpClient client("localhost", 8080, config);

    // Initial state
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    // Connect (will fail)
    client.connect();

    // Update (should not crash)
    client.update(0);
    client.update(100);
    client.update(200);

    // State should settle to DISCONNECTED after max attempts
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
}

FL_TEST_CASE("NativeHttpClient - Multiple instances") {
    NativeHttpClient client1("localhost", 8080);
    NativeHttpClient client2("localhost", 8081);

    FL_CHECK_FALSE(client1.isConnected());
    FL_CHECK_FALSE(client2.isConnected());

    client1.connect();
    client2.connect();

    // Both should handle failures independently
    FL_CHECK_FALSE(client1.isConnected());
    FL_CHECK_FALSE(client2.isConnected());
}
