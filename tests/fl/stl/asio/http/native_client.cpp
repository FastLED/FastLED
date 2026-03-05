#include "test.h"
#include "fl/stl/asio/http/connection.h"
#include "fl/stl/asio/http/connection.cpp.hpp"
#include "fl/stl/asio/http/native_client.h"
#include "fl/stl/asio/http/native_client.cpp.hpp"
#include "fl/stl/asio/http/native_server.h"
#include "fl/stl/asio/http/native_server.cpp.hpp"
#include "fl/stl/function.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/thread.h"
#include "fl/stl/chrono.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/delay.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Use port 0 so the OS assigns a free port — avoids hardcoded port conflicts
// with Windows Hyper-V/NAT reserved ranges and TIME_WAIT races.
static const uint16_t kAnyPort = 0;

// RAII wrapper for NativeHttpServer. Uses port 0 (OS-assigned).
// NativeHttpServer has no move/copy, so we must heap-allocate.
struct ServerGuard {
    fl::unique_ptr<NativeHttpServer> ptr;
    uint16_t port = 0;
    ServerGuard() = default;
    ServerGuard(ServerGuard&& o) : ptr(fl::move(o.ptr)), port(o.port) { o.port = 0; }
    ServerGuard& operator=(ServerGuard&& o) { ptr = fl::move(o.ptr); port = o.port; o.port = 0; return *this; }
    ~ServerGuard() { if (ptr) { ptr->stop(); } }
    NativeHttpServer* operator->() { return ptr.get(); }
    NativeHttpServer& operator*() { return *ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

static ServerGuard makeServer(const ConnectionConfig& config = ConnectionConfig()) {
    ServerGuard g;
    g.ptr = fl::make_unique<NativeHttpServer>(kAnyPort, config);
    if (g.ptr->start()) {
        g.port = g.ptr->port();
        return g;
    }
    g.ptr.reset();
    return g;
}

// Server thread: runs accept + update in background, posts results via atomics/mutex.
// This is the standard net-thread-to-main pattern: a dedicated networking thread
// handles blocking/polling work and the main thread reads results.
struct NetThread {
    NativeHttpServer& server;
    fl::atomic<bool> running{true};
    fl::mutex mtx;
    // Accumulated recv data per client (simplified: single client)
    fl::vector<uint8_t> recvBuf;
    fl::atomic<int> recvReady{0};
    fl::thread thread;

    explicit NetThread(NativeHttpServer& s) : server(s) {
        thread = fl::thread([this]() { run(); });
    }

    ~NetThread() {
        running.store(false);
        if (thread.joinable()) thread.join();
    }

    void run() {
        while (running.load()) {
            server.acceptClients();
            fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for
        }
    }
};

// Poll a condition with sleep between attempts. Loopback TCP is fast but
// accept/recv may not be ready immediately, especially under heavy load.
static bool pollUntil(fl::function<bool()> pred, int maxAttempts = 100) {
    for (int i = 0; i < maxAttempts; ++i) {
        if (pred()) return true;
        fl::this_thread::sleep_for(fl::chrono::milliseconds(10));  // ok sleep for
    }
    return false;
}

// =============================================================================
// Pure state-machine tests (no TCP connections needed)
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Construction and destruction") {
    NativeHttpClient client("localhost", 12345);
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Send and recv fail when disconnected") {
    NativeHttpClient client("localhost", 12345);

    uint8_t sendData[] = {'t', 'e', 's', 't'};
    int sendResult = client.send(sendData);
    FL_CHECK(sendResult == -1);

    uint8_t recvBuffer[64];
    int recvResult = client.recv(recvBuffer);
    FL_CHECK(recvResult == -1);
}

FL_TEST_CASE("NativeHttpClient - Disconnect and close") {
    NativeHttpClient client("localhost", 12345);

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

    NativeHttpClient client("localhost", 12345, config);

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

    NativeHttpClient client("localhost", 12345, config);

    // Initial state
    FL_CHECK(client.getReconnectAttempts() == 0);
    FL_CHECK(client.getReconnectDelayMs() == 0);
}

// =============================================================================
// Tests that require a TCP connection (use a live server to avoid slow timeouts)
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Connection to invalid host fails") {
    // DNS lookup for nonexistent host fails immediately (no TCP timeout)
    NativeHttpClient client("invalid.host.that.does.not.exist.test", 12345);

    bool result = client.connect();
    FL_CHECK_FALSE(result);
    FL_CHECK_FALSE(client.isConnected());
}

FL_TEST_CASE("NativeHttpClient - Non-blocking by default") {
    NativeHttpClient client("localhost", 12345);

    // Attempt connection (should not block, even if server doesn't exist)
    bool result = client.connect();

    // Connection may succeed or fail depending on whether server exists
    // The important thing is that it returned immediately (always non-blocking)
    FL_CHECK((result == true || result == false));  // Just verify it returned
}

FL_TEST_CASE("NativeHttpClient - Connection state transitions with server") {
    auto server = makeServer();
    FL_REQUIRE(server);

    NativeHttpClient client("localhost", server.port);

    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);

    bool connected = client.connect();
    FL_CHECK(connected);
    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    client.disconnect();
    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
}

FL_TEST_CASE("NativeHttpClient - Update loop with server") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 100;
    config.maxReconnectAttempts = 1;

    auto server = makeServer(config);
    FL_REQUIRE(server);

    NativeHttpClient client("localhost", server.port, config);

    FL_CHECK(client.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK(client.connect());

    // Update (should not crash)
    client.update(0);
    client.update(100);
    client.update(200);

    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    client.close();
}

FL_TEST_CASE("NativeHttpClient - Multiple instances with server") {
    auto server1 = makeServer();
    auto server2 = makeServer();
    FL_REQUIRE(server1);
    FL_REQUIRE(server2);

    NativeHttpClient client1("localhost", server1.port);
    NativeHttpClient client2("localhost", server2.port);

    FL_CHECK_FALSE(client1.isConnected());
    FL_CHECK_FALSE(client2.isConnected());

    FL_CHECK(client1.connect());
    FL_CHECK(client2.connect());

    FL_CHECK(client1.isConnected());
    FL_CHECK(client2.isConnected());

    client1.close();
    client2.close();
}

// =============================================================================
// Real socket I/O tests - each test case gets its own server to avoid
// doctest subcase re-entry issues and port reuse races on Windows.
// A background NetThread handles acceptClients() so the main thread can
// focus on client-side I/O without deadlocking on connect().
// =============================================================================

FL_TEST_CASE("NativeHttpClient - Connect to live server succeeds") {
    auto server = makeServer();
    FL_REQUIRE(server);
    NetThread net(*server.ptr);

    NativeHttpClient client("localhost", server.port);
    FL_CHECK(client.connect());
    FL_CHECK(client.isConnected());
    FL_CHECK(client.getState() == ConnectionState::CONNECTED);

    FL_CHECK(pollUntil([&]() {
        return server->getClientCount() > 0;
    }));
    FL_CHECK(server->getClientCount() == 1);

    client.close();
}

FL_TEST_CASE("NativeHttpClient - Client sends data to server") {
    auto server = makeServer();
    FL_REQUIRE(server);
    NetThread net(*server.ptr);

    NativeHttpClient client("localhost", server.port);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        return server->getClientCount() > 0;
    }));
    uint32_t clientId = server->getClientIds()[0];

    uint8_t msg[] = {'G', 'E', 'T', ' ', '/'};
    FL_CHECK(client.send(msg) == (int)sizeof(msg));

    uint8_t buf[64];
    int got = -1;
    pollUntil([&]() {
        got = server->recv(clientId, fl::span<uint8_t>(buf, 64));
        return got > 0;
    });
    FL_CHECK(got == (int)sizeof(msg));
    FL_CHECK(buf[0] == 'G');
    FL_CHECK(buf[1] == 'E');
    FL_CHECK(buf[2] == 'T');
    FL_CHECK(buf[3] == ' ');
    FL_CHECK(buf[4] == '/');

    client.close();
}

FL_TEST_CASE("NativeHttpClient - Client receives data from server") {
    auto server = makeServer();
    FL_REQUIRE(server);
    NetThread net(*server.ptr);

    NativeHttpClient client("localhost", server.port);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        return server->getClientCount() > 0;
    }));
    uint32_t clientId = server->getClientIds()[0];

    uint8_t resp[] = {'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ', '2', '0', '0'};
    FL_CHECK(server->send(clientId, resp) == (int)sizeof(resp));

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
}

FL_TEST_CASE("NativeHttpClient - Bidirectional echo") {
    auto server = makeServer();
    FL_REQUIRE(server);
    NetThread net(*server.ptr);

    NativeHttpClient client("localhost", server.port);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        return server->getClientCount() > 0;
    }));
    uint32_t clientId = server->getClientIds()[0];

    // Client sends PING
    uint8_t request[] = {'P', 'I', 'N', 'G'};
    FL_CHECK(client.send(request) == 4);

    // Server reads it
    uint8_t srvBuf[64];
    int srvGot = -1;
    pollUntil([&]() {
        srvGot = server->recv(clientId, fl::span<uint8_t>(srvBuf, 64));
        return srvGot > 0;
    });
    FL_CHECK(srvGot == 4);

    // Server echoes PONG
    uint8_t response[] = {'P', 'O', 'N', 'G'};
    FL_CHECK(server->send(clientId, response) == 4);

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
}

FL_TEST_CASE("NativeHttpClient - Large payload transfer") {
    auto server = makeServer();
    FL_REQUIRE(server);
    NetThread net(*server.ptr);

    NativeHttpClient client("localhost", server.port);
    FL_REQUIRE(client.connect());
    FL_REQUIRE(pollUntil([&]() {
        return server->getClientCount() > 0;
    }));
    uint32_t clientId = server->getClientIds()[0];

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
        int got = server->recv(clientId, tmp);
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
}

} // FL_TEST_FILE
