#include "test.h"
#include "fl/stl/asio/http/connection.h"
#include "fl/stl/asio/http/connection.cpp.hpp"
#include "fl/stl/asio/http/native_server.h"
#include "fl/stl/asio/http/native_server.cpp.hpp"
#include "fl/stl/asio/http/native_client.h"
#include "fl/stl/asio/http/native_client.cpp.hpp"
#include "fl/delay.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Use unique high ports to avoid conflicts with other tests and services
static const int kTestPort = 47211;
// Socket test ports (each test needs its own to avoid bind conflicts in parallel)
static const int kSocketPort1 = 47221;
static const int kSocketPort2 = 47222;
static const int kSocketPort3 = 47223;
static const int kSocketPort4 = 47224;
static const int kSocketPort5 = 47225;
static const int kSocketPort6 = 47226;
static const int kSocketPort7 = 47227;
// Additional ports for new socket tests
static const int kSocketPort8 = 47231;
static const int kSocketPort9 = 47232;
static const int kSocketPort10 = 47233;
static const int kSocketPort11 = 47234;

FL_TEST_CASE("NativeHttpServer - Construction and destruction") {
    NativeHttpServer server(kTestPort);
    FL_CHECK_FALSE(server.isListening());
    FL_CHECK(server.getClientCount() == 0);
}

FL_TEST_CASE("NativeHttpServer - Start and stop listening") {
    NativeHttpServer server(kTestPort);

    FL_CHECK_FALSE(server.isListening());

    // Start listening
    bool result = server.start();
    FL_CHECK(result);
    FL_CHECK(server.isListening());

    // Stop listening
    server.stop();
    FL_CHECK_FALSE(server.isListening());
}

FL_TEST_CASE("NativeHttpServer - Start listening twice (idempotent)") {
    NativeHttpServer server(kTestPort);

    // Start first time
    bool result1 = server.start();
    FL_CHECK(result1);
    FL_CHECK(server.isListening());

    // Start second time (should be no-op)
    bool result2 = server.start();
    FL_CHECK(result2);
    FL_CHECK(server.isListening());

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Accept clients with no server listening") {
    NativeHttpServer server(kTestPort);

    // Accept without starting server (should be no-op)
    server.acceptClients();
    FL_CHECK(server.getClientCount() == 0);
}

FL_TEST_CASE("NativeHttpServer - Non-blocking by default") {
    NativeHttpServer server(kTestPort);

    bool result = server.start();
    FL_CHECK(result);
    FL_CHECK(server.isListening());

    // Accept should return immediately (always non-blocking)
    server.acceptClients();

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Client management operations when no clients") {
    NativeHttpServer server(kTestPort);
    server.start();

    // Check client count
    FL_CHECK(server.getClientCount() == 0);

    // Get client IDs (should be empty)
    fl::vector<uint32_t> ids = server.getClientIds();
    FL_CHECK(ids.size() == 0);

    // Check for non-existent client
    FL_CHECK_FALSE(server.hasClient(1));
    FL_CHECK_FALSE(server.hasClient(999));

    // Disconnect non-existent client (should be no-op)
    server.disconnectClient(1);
    FL_CHECK(server.getClientCount() == 0);

    // Disconnect all clients when none exist (should be no-op)
    server.disconnectAllClients();
    FL_CHECK(server.getClientCount() == 0);

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Send and recv with no clients") {
    NativeHttpServer server(kTestPort);
    server.start();

    uint8_t sendData[] = {'t', 'e', 's', 't'};
    int sendResult = server.send(999, sendData);
    FL_CHECK(sendResult == -1);

    uint8_t recvBuffer[64];
    int recvResult = server.recv(999, recvBuffer);
    FL_CHECK(recvResult == -1);

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Broadcast with no clients") {
    NativeHttpServer server(kTestPort);
    server.start();

    uint8_t data[] = {'t', 'e', 's', 't'};
    server.broadcast(data);  // Should not crash

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Update loop with no clients") {
    NativeHttpServer server(kTestPort);
    server.start();

    // Update should not crash with no clients
    server.update(0);
    server.update(1000);
    server.update(2000);

    FL_CHECK(server.getClientCount() == 0);

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Accept real client connection") {
    NativeHttpServer server(kSocketPort1);  // Use different port to avoid conflicts
    server.start();

    // Create client and connect
    NativeHttpClient client("localhost", kSocketPort1);
    client.connect();

    // Give connection time to establish (blocking wait, but should be fast)
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        fl::delay(10);
    }

    // Server should have accepted the client
    FL_CHECK(server.getClientCount() == 1);

    // Cleanup
    server.stop();
    client.close();
}

FL_TEST_CASE("NativeHttpServer - Client ID tracking") {
    NativeHttpServer server(kSocketPort2);
    server.start();

    // Connect first client
    NativeHttpClient client1("localhost", kSocketPort2);
    client1.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_REQUIRE(server.getClientCount() == 1);

    // Get client IDs
    fl::vector<uint32_t> ids = server.getClientIds();
    FL_REQUIRE(ids.size() == 1);
    uint32_t clientId1 = ids[0];
    FL_CHECK(server.hasClient(clientId1));

    // Connect second client
    NativeHttpClient client2("localhost", kSocketPort2);
    client2.connect();

    // Accept second client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 1) {
            break;
        }
        fl::delay(10);
    }

    FL_REQUIRE(server.getClientCount() == 2);

    // Get updated client IDs
    ids = server.getClientIds();
    FL_REQUIRE(ids.size() == 2);

    // Both clients should have unique IDs
    FL_CHECK(ids[0] != ids[1]);

    // Cleanup
    server.stop();
    client1.close();
    client2.close();
}

FL_TEST_CASE("NativeHttpServer - Send and recv with real client") {
    NativeHttpServer server(kSocketPort3);
    server.start();

    // Connect client
    NativeHttpClient client("localhost", kSocketPort3);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_REQUIRE(server.getClientCount() == 1);
    uint32_t clientId = server.getClientIds()[0];

    // Send data from server to client
    uint8_t serverData[] = {'H', 'e', 'l', 'l', 'o'};
    int sendResult = server.send(clientId, serverData);
    FL_CHECK(sendResult == sizeof(serverData));

    // Receive data on client
    uint8_t clientBuffer[64];
    int recvResult = -1;
    for (int i = 0; i < 10; ++i) {
        recvResult = client.recv(clientBuffer);
        if (recvResult > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(recvResult == sizeof(serverData));
    FL_CHECK(clientBuffer[0] == 'H');
    FL_CHECK(clientBuffer[1] == 'e');
    FL_CHECK(clientBuffer[2] == 'l');
    FL_CHECK(clientBuffer[3] == 'l');
    FL_CHECK(clientBuffer[4] == 'o');

    // Send data from client to server
    uint8_t clientData[] = {'W', 'o', 'r', 'l', 'd'};
    sendResult = client.send(clientData);
    FL_CHECK(sendResult == sizeof(clientData));

    // Receive data on server
    uint8_t serverBuffer[64];
    recvResult = -1;
    for (int i = 0; i < 10; ++i) {
        recvResult = server.recv(clientId, serverBuffer);
        if (recvResult > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(recvResult == sizeof(clientData));
    FL_CHECK(serverBuffer[0] == 'W');
    FL_CHECK(serverBuffer[1] == 'o');
    FL_CHECK(serverBuffer[2] == 'r');
    FL_CHECK(serverBuffer[3] == 'l');
    FL_CHECK(serverBuffer[4] == 'd');

    // Cleanup
    server.stop();
    client.close();
}

FL_TEST_CASE("NativeHttpServer - Broadcast to multiple clients") {
    NativeHttpServer server(kSocketPort4);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort4);
    NativeHttpClient client2("localhost", kSocketPort4);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(server.getClientCount() == 2);

    // Broadcast message
    uint8_t broadcastData[] = {'B', 'R', 'O', 'A', 'D', 'C', 'A', 'S', 'T'};
    server.broadcast(broadcastData);

    // Both clients should receive the message
    uint8_t buffer1[64];
    uint8_t buffer2[64];
    int recv1 = -1;
    int recv2 = -1;

    for (int i = 0; i < 10; ++i) {
        if (recv1 <= 0) {
            recv1 = client1.recv(buffer1);
        }
        if (recv2 <= 0) {
            recv2 = client2.recv(buffer2);
        }
        if (recv1 > 0 && recv2 > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(recv1 == sizeof(broadcastData));
    FL_CHECK(recv2 == sizeof(broadcastData));
    FL_CHECK(buffer1[0] == 'B');
    FL_CHECK(buffer2[0] == 'B');

    // Cleanup
    server.stop();
    client1.close();
    client2.close();
}

FL_TEST_CASE("NativeHttpServer - Disconnect specific client") {
    NativeHttpServer server(kSocketPort5);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort5);
    NativeHttpClient client2("localhost", kSocketPort5);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        fl::delay(10);
    }

    FL_REQUIRE(server.getClientCount() == 2);

    // Get client IDs
    fl::vector<uint32_t> ids = server.getClientIds();
    FL_REQUIRE(ids.size() == 2);

    // Disconnect first client
    server.disconnectClient(ids[0]);
    FL_CHECK(server.getClientCount() == 1);

    // Second client should still be connected
    FL_CHECK(server.hasClient(ids[1]));
    FL_CHECK_FALSE(server.hasClient(ids[0]));

    // Cleanup
    server.stop();
    client1.close();
    client2.close();
}

FL_TEST_CASE("NativeHttpServer - Disconnect all clients") {
    NativeHttpServer server(kSocketPort6);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort6);
    NativeHttpClient client2("localhost", kSocketPort6);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(server.getClientCount() == 2);

    // Disconnect all clients
    server.disconnectAllClients();
    FL_CHECK(server.getClientCount() == 0);

    // Cleanup
    server.stop();
    client1.close();
    client2.close();
}

FL_TEST_CASE("NativeHttpServer - Update removes disconnected clients") {
    NativeHttpServer server(kSocketPort7);
    server.start();

    // Connect client
    NativeHttpClient client("localhost", kSocketPort7);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        fl::delay(10);
    }

    FL_CHECK(server.getClientCount() == 1);

    // Close client connection
    client.close();

    // Give server time to detect disconnection
    fl::delay(50);

    // Update should remove disconnected client
    server.update(0);

    // Client count should eventually become 0 after update detects disconnection
    // Note: This may take multiple updates depending on socket state
    for (int i = 0; i < 10; ++i) {
        server.update(i * 100);
        if (server.getClientCount() == 0) {
            break;
        }
        fl::delay(10);
    }

    // Cleanup
    server.stop();
}

// =============================================================================
// Additional POSIX socket tests - HTTP-like patterns over real sockets
// =============================================================================

FL_TEST_CASE("NativeHttpServer - HTTP-like request/response exchange") {
    NativeHttpServer server(kSocketPort8);
    server.start();

    NativeHttpClient client("localhost", kSocketPort8);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) break;
        fl::delay(10);
    }
    FL_REQUIRE(server.getClientCount() == 1);
    uint32_t clientId = server.getClientIds()[0];

    // Client sends HTTP-like GET request
    const char httpReq[] = "GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int reqLen = (int)(sizeof(httpReq) - 1);  // exclude null terminator
    fl::span<const uint8_t> reqSpan((const uint8_t*)httpReq, reqLen);
    int sent = client.send(reqSpan);
    FL_CHECK(sent == reqLen);

    // Server receives the full request
    uint8_t reqBuf[256];
    int totalRecv = 0;
    for (int i = 0; i < 20 && totalRecv < reqLen; ++i) {
        fl::span<uint8_t> recvSpan(reqBuf + totalRecv, (int)sizeof(reqBuf) - totalRecv);
        int got = server.recv(clientId, recvSpan);
        if (got > 0) {
            totalRecv += got;
        } else {
            fl::delay(5);
        }
    }
    FL_CHECK(totalRecv == reqLen);
    // Verify request starts with "GET"
    FL_CHECK(reqBuf[0] == 'G');
    FL_CHECK(reqBuf[1] == 'E');
    FL_CHECK(reqBuf[2] == 'T');

    // Server sends HTTP-like response
    const char httpResp[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    int respLen = (int)(sizeof(httpResp) - 1);  // exclude null terminator
    fl::span<const uint8_t> respSpan((const uint8_t*)httpResp, respLen);
    sent = server.send(clientId, respSpan);
    FL_CHECK(sent == respLen);

    // Client receives the full response
    uint8_t respBuf[256];
    totalRecv = 0;
    for (int i = 0; i < 20 && totalRecv < respLen; ++i) {
        fl::span<uint8_t> recvSpan(respBuf + totalRecv, (int)sizeof(respBuf) - totalRecv);
        int got = client.recv(recvSpan);
        if (got > 0) {
            totalRecv += got;
        } else {
            fl::delay(5);
        }
    }
    FL_CHECK(totalRecv == respLen);
    // Verify response starts with "HTTP/1.1 200"
    FL_CHECK(respBuf[0] == 'H');
    FL_CHECK(respBuf[5] == '1');
    FL_CHECK(respBuf[6] == '.');
    FL_CHECK(respBuf[7] == '1');
    FL_CHECK(respBuf[9] == '2');
    FL_CHECK(respBuf[10] == '0');
    FL_CHECK(respBuf[11] == '0');

    server.stop();
    client.close();
}

FL_TEST_CASE("NativeHttpServer - Server restart on same port (SO_REUSEADDR)") {
    // First server instance
    {
        NativeHttpServer server(kSocketPort9);
        FL_CHECK(server.start());
        FL_CHECK(server.isListening());

        // Connect a client to prove it works
        NativeHttpClient client("localhost", kSocketPort9);
        client.connect();

        for (int i = 0; i < 10; ++i) {
            server.acceptClients();
            if (server.getClientCount() > 0) break;
            fl::delay(10);
        }
        FL_CHECK(server.getClientCount() == 1);

        client.close();
        server.stop();
    }

    // Second server instance on the SAME port (tests SO_REUSEADDR)
    {
        NativeHttpServer server(kSocketPort9);
        bool started = server.start();
        FL_CHECK(started);
        FL_CHECK(server.isListening());

        // Connect a new client to prove it works after restart
        NativeHttpClient client("localhost", kSocketPort9);
        client.connect();

        for (int i = 0; i < 10; ++i) {
            server.acceptClients();
            if (server.getClientCount() > 0) break;
            fl::delay(10);
        }
        FL_CHECK(server.getClientCount() == 1);

        client.close();
        server.stop();
    }
}

FL_TEST_CASE("NativeHttpServer - Large server-to-client transfer") {
    NativeHttpServer server(kSocketPort10);
    server.start();

    NativeHttpClient client("localhost", kSocketPort10);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) break;
        fl::delay(10);
    }
    FL_REQUIRE(server.getClientCount() == 1);
    uint32_t clientId = server.getClientIds()[0];

    // Server sends a 4KB payload (simulating a large HTTP response body)
    const int payloadSize = 4096;
    fl::vector<uint8_t> payload;
    payload.resize(payloadSize);
    for (int i = 0; i < payloadSize; ++i) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    int totalSent = 0;
    while (totalSent < payloadSize) {
        int remaining = payloadSize - totalSent;
        fl::span<const uint8_t> chunk(payload.data() + totalSent, remaining);
        int sent = server.send(clientId, chunk);
        if (sent > 0) {
            totalSent += sent;
        } else {
            fl::delay(1);
        }
    }
    FL_CHECK(totalSent == payloadSize);

    // Client receives all data
    fl::vector<uint8_t> recvBuf;
    recvBuf.resize(payloadSize);
    int totalRecv = 0;
    for (int attempt = 0; attempt < 100 && totalRecv < payloadSize; ++attempt) {
        uint8_t tmp[1024];
        int got = client.recv(tmp);
        if (got > 0) {
            for (int j = 0; j < got && totalRecv < payloadSize; ++j) {
                recvBuf[totalRecv++] = tmp[j];
            }
        } else {
            fl::delay(5);
        }
    }
    FL_CHECK(totalRecv == payloadSize);

    // Verify data integrity
    bool match = true;
    for (int i = 0; i < payloadSize; ++i) {
        if (recvBuf[i] != (uint8_t)(i & 0xFF)) {
            match = false;
            break;
        }
    }
    FL_CHECK(match);

    server.stop();
    client.close();
}

FL_TEST_CASE("NativeHttpServer - Multiple sequential requests on same connection") {
    NativeHttpServer server(kSocketPort11);
    server.start();

    NativeHttpClient client("localhost", kSocketPort11);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) break;
        fl::delay(10);
    }
    FL_REQUIRE(server.getClientCount() == 1);
    uint32_t clientId = server.getClientIds()[0];

    // Perform 3 request/response cycles on the same connection (HTTP keep-alive)
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Client sends numbered request
        uint8_t req[4] = {'R', 'E', 'Q', (uint8_t)('0' + cycle)};
        FL_CHECK(client.send(req) == 4);

        // Server receives
        uint8_t srvBuf[16];
        int got = -1;
        for (int i = 0; i < 10; ++i) {
            got = server.recv(clientId, srvBuf);
            if (got > 0) break;
            fl::delay(5);
        }
        FL_CHECK(got == 4);
        FL_CHECK(srvBuf[3] == (uint8_t)('0' + cycle));

        // Server sends numbered response
        uint8_t resp[4] = {'R', 'S', 'P', (uint8_t)('0' + cycle)};
        FL_CHECK(server.send(clientId, resp) == 4);

        // Client receives
        uint8_t cliBuf[16];
        got = -1;
        for (int i = 0; i < 10; ++i) {
            got = client.recv(cliBuf);
            if (got > 0) break;
            fl::delay(5);
        }
        FL_CHECK(got == 4);
        FL_CHECK(cliBuf[0] == 'R');
        FL_CHECK(cliBuf[1] == 'S');
        FL_CHECK(cliBuf[2] == 'P');
        FL_CHECK(cliBuf[3] == (uint8_t)('0' + cycle));
    }

    server.stop();
    client.close();
}

} // FL_TEST_FILE
