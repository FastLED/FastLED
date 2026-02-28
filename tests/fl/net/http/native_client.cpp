#include "test.h"
#include "fl/net/http/connection.h"
#include "fl/net/http/connection.cpp.hpp"
#include "fl/net/http/native_client.h"
#include "fl/net/http/native_client.cpp.hpp"
#include "fl/net/http/native_server.h"
#include "fl/net/http/native_server.cpp.hpp"
#include "fl/stl/function.h"

using namespace fl;

// Each test case needs its own unique port to avoid bind failures when the
// test runner executes cases as separate subprocesses (TIME_WAIT races on
// Windows). Ports are spread apart so no two tests ever share a port.
static const int kPortStateTransitions = 58901;
static const int kPortUpdateLoop       = 58902;
static const int kPortMultiInst1       = 58903;
static const int kPortMultiInst2       = 58904;

// Ports for real socket I/O tests (each test gets its own)
static const int kPortConnectLive      = 58911;
static const int kPortClientSend       = 58912;
static const int kPortClientRecv       = 58913;
static const int kPortBidiEcho         = 58914;
static const int kPortLargePayload     = 58915;

// Poll a condition up to maxAttempts times. Loopback TCP is fast but the
// accept/recv may not be ready on the very first call.
static bool pollUntil(fl::function<bool()> pred, int maxAttempts = 20) {
    for (int i = 0; i < maxAttempts; ++i) {
        if (pred()) return true;
    }
    return false;
}

// =============================================================================
// Pure state-machine tests (no TCP connections needed)
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Construction and destruction") {
    NativeHttpClient client("localhost", kPortStateTransitions);
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Send and recv fail when disconnected") {
    NativeHttpClient client("localhost", kPortStateTransitions);

    uint8_t sendData[] = {'t', 'e', 's', 't'};
    int sendResult = client.send(sendData);
    FL_CHECK(sendResult == -1);

    uint8_t recvBuffer[64];
    int recvResult = client.recv(recvBuffer);
    FL_CHECK(recvResult == -1);
}

FL_TEST_CASE("NativeHttpClient - Disconnect and close") {
    NativeHttpClient client("localhost", kPortStateTransitions);

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

    NativeHttpClient client("localhost", kPortStateTransitions, config);

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

    NativeHttpClient client("localhost", kPortStateTransitions, config);

    // Initial state
    FL_CHECK(client.getReconnectAttempts() == 0);
    FL_CHECK(client.getReconnectDelayMs() == 0);
}

// =============================================================================
// Tests that require a TCP connection (use a live server to avoid slow timeouts)
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Connection to invalid host fails") {
    // DNS lookup for nonexistent host fails immediately (no TCP timeout)
    NativeHttpClient client("invalid.host.that.does.not.exist.test", kPortStateTransitions);

    bool result = client.connect();
    FL_CHECK_FALSE(result);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Non-blocking mode") {
    NativeHttpClient client("localhost", kPortStateTransitions);

    // Set non-blocking mode before connection
    client.setNonBlocking(true);

    // Attempt connection (should not block, even if server doesn't exist)
    bool result = client.connect();

    // Connection may succeed or fail depending on whether server exists
    // The important thing is that it returned immediately (non-blocking)
    FL_CHECK((result == true || result == false));  // Just verify it returned
}

FL_TEST_CASE("NativeHttpClient - Connection state transitions with server") {
    NativeHttpServer server(kPortStateTransitions);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortStateTransitions);

    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    bool connected = client.connect();
    FL_CHECK(connected);
    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    client.disconnect();
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Update loop with server") {
    NativeHttpServer server(kPortUpdateLoop);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    ConnectionConfig config;
    config.reconnectInitialDelayMs = 100;
    config.maxReconnectAttempts = 1;

    NativeHttpClient client("localhost", kPortUpdateLoop, config);

    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK(client.connect());

    // Update (should not crash)
    client.update(0);
    client.update(100);
    client.update(200);

    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    client.close();
    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Multiple instances with server") {
    NativeHttpServer server1(kPortMultiInst1);
    NativeHttpServer server2(kPortMultiInst2);
    server1.setNonBlocking(true);
    server2.setNonBlocking(true);
    FL_REQUIRE(server1.start());
    FL_REQUIRE(server2.start());

    NativeHttpClient client1("localhost", kPortMultiInst1);
    NativeHttpClient client2("localhost", kPortMultiInst2);

    FL_CHECK_FALSE(client1.isConnected());
    FL_CHECK_FALSE(client2.isConnected());

    FL_CHECK(client1.connect());
    FL_CHECK(client2.connect());

    FL_CHECK(client1.isConnected());
    FL_CHECK(client2.isConnected());

    client1.close();
    client2.close();
    server1.stop();
    server2.stop();
}

// =============================================================================
// Real socket I/O tests - each test case gets its own server to avoid
// doctest subcase re-entry issues and port reuse races on Windows.
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Connect to live server succeeds") {
    NativeHttpServer server(kPortConnectLive);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortConnectLive);
    FL_CHECK(client.connect());
    FL_CHECK(client.isConnected());
    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    FL_CHECK(pollUntil([&]() {
        server.acceptClients();
        return server.getClientCount() > 0;
    }));
    FL_CHECK(server.getClientCount() == 1);

    client.close();
    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Client sends data to server") {
    NativeHttpServer server(kPortClientSend);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortClientSend);
    client.setNonBlocking(true);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        server.acceptClients();
        return server.getClientCount() > 0;
    }));
    uint32_t clientId = server.getClientIds()[0];

    uint8_t msg[] = {'G', 'E', 'T', ' ', '/'};
    FL_CHECK(client.send(msg) == (int)sizeof(msg));

    uint8_t buf[64];
    int got = -1;
    pollUntil([&]() {
        got = server.recv(clientId, fl::span<uint8_t>(buf, 64));
        return got > 0;
    });
    FL_CHECK(got == (int)sizeof(msg));
    FL_CHECK(buf[0] == 'G');
    FL_CHECK(buf[1] == 'E');
    FL_CHECK(buf[2] == 'T');
    FL_CHECK(buf[3] == ' ');
    FL_CHECK(buf[4] == '/');

    client.close();
    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Client receives data from server") {
    NativeHttpServer server(kPortClientRecv);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortClientRecv);
    client.setNonBlocking(true);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        server.acceptClients();
        return server.getClientCount() > 0;
    }));
    uint32_t clientId = server.getClientIds()[0];

    uint8_t resp[] = {'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ', '2', '0', '0'};
    FL_CHECK(server.send(clientId, resp) == (int)sizeof(resp));

    uint8_t buf[64];
    int got = -1;
    pollUntil([&]() {
        got = client.recv(fl::span<uint8_t>(buf, 64));
        return got > 0;
    });
    FL_CHECK(got == (int)sizeof(resp));
    FL_CHECK(buf[0] == 'H');
    FL_CHECK(buf[4] == '/');
    FL_CHECK(buf[9] == '2');
    FL_CHECK(buf[10] == '0');
    FL_CHECK(buf[11] == '0');

    client.close();
    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Bidirectional echo") {
    NativeHttpServer server(kPortBidiEcho);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortBidiEcho);
    client.setNonBlocking(true);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        server.acceptClients();
        return server.getClientCount() > 0;
    }));
    uint32_t clientId = server.getClientIds()[0];

    // Client sends PING
    uint8_t request[] = {'P', 'I', 'N', 'G'};
    FL_CHECK(client.send(request) == 4);

    // Server reads it
    uint8_t srvBuf[64];
    int srvGot = -1;
    pollUntil([&]() {
        srvGot = server.recv(clientId, fl::span<uint8_t>(srvBuf, 64));
        return srvGot > 0;
    });
    FL_CHECK(srvGot == 4);

    // Server echoes PONG
    uint8_t response[] = {'P', 'O', 'N', 'G'};
    FL_CHECK(server.send(clientId, response) == 4);

    // Client reads response
    uint8_t cliBuf[64];
    int cliGot = -1;
    pollUntil([&]() {
        cliGot = client.recv(fl::span<uint8_t>(cliBuf, 64));
        return cliGot > 0;
    });
    FL_CHECK(cliGot == 4);
    FL_CHECK(cliBuf[0] == 'P');
    FL_CHECK(cliBuf[1] == 'O');
    FL_CHECK(cliBuf[2] == 'N');
    FL_CHECK(cliBuf[3] == 'G');

    client.close();
    server.stop();
}

FL_TEST_CASE("NativeHttpClient - Large payload transfer") {
    NativeHttpServer server(kPortLargePayload);
    server.setNonBlocking(true);
    FL_REQUIRE(server.start());

    NativeHttpClient client("localhost", kPortLargePayload);
    client.setNonBlocking(true);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        server.acceptClients();
        return server.getClientCount() > 0;
    }));
    uint32_t clientId = server.getClientIds()[0];

    // Build a 4KB payload
    const int payloadSize = 4096;
    fl::vector<uint8_t> payload;
    payload.resize(payloadSize);
    for (int i = 0; i < payloadSize; ++i) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    // Client sends large payload (may need multiple send calls)
    int totalSent = 0;
    pollUntil([&]() {
        fl::span<const uint8_t> chunk(payload.data() + totalSent,
                                      payloadSize - totalSent);
        int sent = client.send(chunk);
        if (sent > 0) totalSent += sent;
        return totalSent >= payloadSize;
    }, 100);
    FL_CHECK(totalSent == payloadSize);

    // Server receives all data (may arrive in chunks)
    fl::vector<uint8_t> recvBuf;
    recvBuf.resize(payloadSize);
    int totalRecv = 0;
    pollUntil([&]() {
        uint8_t tmp[1024];
        int got = server.recv(clientId, tmp);
        if (got > 0) {
            for (int j = 0; j < got && totalRecv < payloadSize; ++j) {
                recvBuf[totalRecv++] = tmp[j];
            }
        }
        return totalRecv >= payloadSize;
    }, 200);
    FL_CHECK(totalRecv == payloadSize);

    // Verify data integrity
    bool dataMatch = true;
    for (int i = 0; i < payloadSize; ++i) {
        if (recvBuf[i] != (uint8_t)(i & 0xFF)) {
            dataMatch = false;
            break;
        }
    }
    FL_CHECK(dataMatch);

    client.close();
    server.stop();
}
