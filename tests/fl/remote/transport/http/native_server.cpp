#include "test.h"
#include "fl/remote/transport/http/connection.h"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/native_server.h"
#include "fl/remote/transport/http/native_server.cpp.hpp"
#include "fl/remote/transport/http/native_client.h"
#include "fl/remote/transport/http/native_client.cpp.hpp"

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

FL_TEST_CASE("NativeHttpServer - Non-blocking mode") {
    NativeHttpServer server(kTestPort);

    // Set non-blocking mode before starting
    server.setNonBlocking(true);

    bool result = server.start();
    FL_CHECK(result);
    FL_CHECK(server.isListening());

    // Accept should return immediately (non-blocking)
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
    int sendResult = server.send(999, sendData, sizeof(sendData));
    FL_CHECK(sendResult == -1);

    uint8_t recvBuffer[64];
    int recvResult = server.recv(999, recvBuffer, sizeof(recvBuffer));
    FL_CHECK(recvResult == -1);

    server.stop();
}

FL_TEST_CASE("NativeHttpServer - Broadcast with no clients") {
    NativeHttpServer server(kTestPort);
    server.start();

    uint8_t data[] = {'t', 'e', 's', 't'};
    server.broadcast(data, sizeof(data));  // Should not crash

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
    server.setNonBlocking(true);
    server.start();

    // Create client and connect
    NativeHttpClient client("localhost", kSocketPort1);
    client.setNonBlocking(true);
    client.connect();

    // Give connection time to establish (blocking wait, but should be fast)
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);  // 10ms
        #else
        usleep(10000);  // 10ms
        #endif
    }

    // Server should have accepted the client
    FL_CHECK(server.getClientCount() == 1);

    // Cleanup
    server.stop();
    client.close();
}

FL_TEST_CASE("NativeHttpServer - Client ID tracking") {
    NativeHttpServer server(kSocketPort2);
    server.setNonBlocking(true);
    server.start();

    // Connect first client
    NativeHttpClient client1("localhost", kSocketPort2);
    client1.setNonBlocking(true);
    client1.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 1);

    // Get client IDs
    fl::vector<uint32_t> ids = server.getClientIds();
    FL_CHECK(ids.size() == 1);
    uint32_t clientId1 = ids[0];
    FL_CHECK(server.hasClient(clientId1));

    // Connect second client
    NativeHttpClient client2("localhost", kSocketPort2);
    client2.setNonBlocking(true);
    client2.connect();

    // Accept second client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 1) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 2);

    // Get updated client IDs
    ids = server.getClientIds();
    FL_CHECK(ids.size() == 2);

    // Both clients should have unique IDs
    FL_CHECK(ids[0] != ids[1]);

    // Cleanup
    server.stop();
    client1.close();
    client2.close();
}

FL_TEST_CASE("NativeHttpServer - Send and recv with real client") {
    NativeHttpServer server(kSocketPort3);
    server.setNonBlocking(true);
    server.start();

    // Connect client
    NativeHttpClient client("localhost", kSocketPort3);
    client.setNonBlocking(true);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 1);
    uint32_t clientId = server.getClientIds()[0];

    // Send data from server to client
    uint8_t serverData[] = {'H', 'e', 'l', 'l', 'o'};
    int sendResult = server.send(clientId, serverData, sizeof(serverData));
    FL_CHECK(sendResult == sizeof(serverData));

    // Receive data on client
    uint8_t clientBuffer[64];
    int recvResult = -1;
    for (int i = 0; i < 10; ++i) {
        recvResult = client.recv(clientBuffer, sizeof(clientBuffer));
        if (recvResult > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(recvResult == sizeof(serverData));
    FL_CHECK(clientBuffer[0] == 'H');
    FL_CHECK(clientBuffer[1] == 'e');
    FL_CHECK(clientBuffer[2] == 'l');
    FL_CHECK(clientBuffer[3] == 'l');
    FL_CHECK(clientBuffer[4] == 'o');

    // Send data from client to server
    uint8_t clientData[] = {'W', 'o', 'r', 'l', 'd'};
    sendResult = client.send(clientData, sizeof(clientData));
    FL_CHECK(sendResult == sizeof(clientData));

    // Receive data on server
    uint8_t serverBuffer[64];
    recvResult = -1;
    for (int i = 0; i < 10; ++i) {
        recvResult = server.recv(clientId, serverBuffer, sizeof(serverBuffer));
        if (recvResult > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
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
    server.setNonBlocking(true);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort4);
    NativeHttpClient client2("localhost", kSocketPort4);
    client1.setNonBlocking(true);
    client2.setNonBlocking(true);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 2);

    // Broadcast message
    uint8_t broadcastData[] = {'B', 'R', 'O', 'A', 'D', 'C', 'A', 'S', 'T'};
    server.broadcast(broadcastData, sizeof(broadcastData));

    // Both clients should receive the message
    uint8_t buffer1[64];
    uint8_t buffer2[64];
    int recv1 = -1;
    int recv2 = -1;

    for (int i = 0; i < 10; ++i) {
        if (recv1 <= 0) {
            recv1 = client1.recv(buffer1, sizeof(buffer1));
        }
        if (recv2 <= 0) {
            recv2 = client2.recv(buffer2, sizeof(buffer2));
        }
        if (recv1 > 0 && recv2 > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
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
    server.setNonBlocking(true);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort5);
    NativeHttpClient client2("localhost", kSocketPort5);
    client1.setNonBlocking(true);
    client2.setNonBlocking(true);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 2);

    // Get client IDs
    fl::vector<uint32_t> ids = server.getClientIds();
    FL_CHECK(ids.size() == 2);

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
    server.setNonBlocking(true);
    server.start();

    // Connect two clients
    NativeHttpClient client1("localhost", kSocketPort6);
    NativeHttpClient client2("localhost", kSocketPort6);
    client1.setNonBlocking(true);
    client2.setNonBlocking(true);
    client1.connect();
    client2.connect();

    // Accept both clients
    for (int i = 0; i < 20; ++i) {
        server.acceptClients();
        if (server.getClientCount() >= 2) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
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
    server.setNonBlocking(true);
    server.start();

    // Connect client
    NativeHttpClient client("localhost", kSocketPort7);
    client.setNonBlocking(true);
    client.connect();

    // Accept client
    for (int i = 0; i < 10; ++i) {
        server.acceptClients();
        if (server.getClientCount() > 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    FL_CHECK(server.getClientCount() == 1);

    // Close client connection
    client.close();

    // Give server time to detect disconnection
    #ifdef _WIN32
    Sleep(50);
    #else
    usleep(50000);
    #endif

    // Update should remove disconnected client
    server.update(0);

    // Client count should eventually become 0 after update detects disconnection
    // Note: This may take multiple updates depending on socket state
    for (int i = 0; i < 10; ++i) {
        server.update(i * 100);
        if (server.getClientCount() == 0) {
            break;
        }
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    // Cleanup
    server.stop();
}
