// ok standalone
#include "test.h"
#include "mock_http_server.h"
#include "mock_http_client.h"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/json.h"

using namespace fl;

// Test: MockHttpServer - Construction
FL_TEST_CASE("MockHttpServer - Construction") {
    MockHttpServer server(47701);
    FL_CHECK_FALSE(server.isConnected());
    FL_CHECK_EQ(0, server.getClientCount());
}

// Test: MockHttpServer - Start
FL_TEST_CASE("MockHttpServer - Start server") {
    MockHttpServer server(47701);
    FL_CHECK(server.connect());
    FL_CHECK(server.isConnected());
}

// Test: MockHttpServer - Stop
FL_TEST_CASE("MockHttpServer - Stop server") {
    MockHttpServer server(47701);
    server.connect();
    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());
}

// Test: MockHttpServer - Double start is safe
FL_TEST_CASE("MockHttpServer - Double start is safe") {
    MockHttpServer server(47701);
    FL_CHECK(server.connect());
    FL_CHECK(server.connect());
    FL_CHECK(server.isConnected());
}

// Test: MockHttpServer - Double stop is safe
FL_TEST_CASE("MockHttpServer - Double stop is safe") {
    MockHttpServer server(47701);
    server.connect();
    server.disconnect();
    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());
}

// Test: MockHttpServer - Accept single client
FL_TEST_CASE("MockHttpServer - Accept single client") {
    MockHttpServer server(47701);
    server.connect();

    uint32_t clientId = server.acceptClient();
    FL_CHECK_NE(0, clientId);
    FL_CHECK_EQ(1, server.getClientCount());
}

// Test: MockHttpServer - Accept multiple clients
FL_TEST_CASE("MockHttpServer - Accept multiple clients") {
    MockHttpServer server(47701);
    server.connect();

    uint32_t clientId1 = server.acceptClient();
    uint32_t clientId2 = server.acceptClient();
    uint32_t clientId3 = server.acceptClient();

    FL_CHECK_NE(0, clientId1);
    FL_CHECK_NE(0, clientId2);
    FL_CHECK_NE(0, clientId3);
    FL_CHECK_NE(clientId1, clientId2);
    FL_CHECK_NE(clientId2, clientId3);
    FL_CHECK_EQ(3, server.getClientCount());
}

// Test: MockHttpServer - Cannot accept when not listening
FL_TEST_CASE("MockHttpServer - Cannot accept when not listening") {
    MockHttpServer server(47701);
    server.connect();
    server.disconnect();
    uint32_t clientId = server.acceptClient();
    FL_CHECK_EQ(0, clientId);
    FL_CHECK_EQ(0, server.getClientCount());
}

// Test: MockHttpServer - Disconnect client
FL_TEST_CASE("MockHttpServer - Disconnect client") {
    MockHttpServer server(47701);
    server.connect();

    uint32_t clientId1 = server.acceptClient();
    uint32_t clientId2 = server.acceptClient();
    FL_CHECK_EQ(2, server.getClientCount());

    server.disconnectClient(clientId1);
    FL_CHECK_EQ(1, server.getClientCount());

    server.disconnectClient(clientId2);
    FL_CHECK_EQ(0, server.getClientCount());
}

// Test: MockHttpClient - Construction
FL_TEST_CASE("MockHttpClient - Construction") {
    MockHttpServer server(47701);
    server.connect();

    MockHttpClient client(server);
    FL_CHECK_FALSE(client.isConnected());
}

// Test: MockHttpClient - Connect to server
FL_TEST_CASE("MockHttpClient - Connect to server") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);

    FL_CHECK(client.connect());
    FL_CHECK(client.isConnected());
    FL_CHECK_EQ(1, server.getClientCount());
}

// Test: MockHttpClient - Cannot connect to stopped server
FL_TEST_CASE("MockHttpClient - Cannot connect to stopped server") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);

    server.disconnect();
    FL_CHECK_FALSE(client.connect());
    FL_CHECK_FALSE(client.isConnected());
}

// Test: MockHttpClient - Disconnect from server
FL_TEST_CASE("MockHttpClient - Disconnect from server") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);

    client.connect();
    client.disconnect();
    FL_CHECK_FALSE(client.isConnected());
    FL_CHECK_EQ(0, server.getClientCount());
}

// Test: MockHttpClient - Double connect is safe
FL_TEST_CASE("MockHttpClient - Double connect is safe") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);

    FL_CHECK(client.connect());
    FL_CHECK(client.connect());
    FL_CHECK(client.isConnected());
}

// Test: MockHttpClient - Double disconnect is safe
FL_TEST_CASE("MockHttpClient - Double disconnect is safe") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);

    client.connect();
    client.disconnect();
    client.disconnect();
    FL_CHECK_FALSE(client.isConnected());
}

// Test: Client sends request, server receives
FL_TEST_CASE("MockHttpClient/Server - Client sends request, server receives") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);
    client.connect();

    // Client sends JSON-RPC request
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "add");
    Json params = Json::array();
    params.push_back(5);
    params.push_back(3);
    request.set("params", params);
    request.set("id", 1);

    client.writeResponse(request);

    // Server receives request
    fl::optional<Json> receivedReq = server.readRequest();
    FL_REQUIRE(receivedReq);
    auto methodVal = (*receivedReq)["method"].as_string();
    FL_REQUIRE(methodVal.has_value());
    FL_CHECK_EQ("add", *methodVal);
}

// Test: Server sends response, client receives
FL_TEST_CASE("MockHttpClient/Server - Server sends response, client receives") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);
    client.connect();

    // Server sends JSON-RPC response
    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 42);
    response.set("id", 1);

    server.writeResponse(response);

    // Client receives response
    fl::optional<Json> receivedResp = client.readRequest();
    FL_REQUIRE(receivedResp);
    auto resultVal = (*receivedResp)["result"].as_int();
    FL_REQUIRE(resultVal.has_value());
    FL_CHECK_EQ(42, *resultVal);
}

// Test: Full request-response cycle
FL_TEST_CASE("MockHttpClient/Server - Full request-response cycle") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);
    client.connect();

    // Client sends request
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "add");
    Json params = Json::array();
    params.push_back(10);
    params.push_back(20);
    request.set("params", params);
    request.set("id", 1);

    client.writeResponse(request);

    // Server receives request
    fl::optional<Json> receivedReq = server.readRequest();
    FL_REQUIRE(receivedReq);

    // Server sends response
    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 30);
    response.set("id", 1);

    server.writeResponse(response);

    // Client receives response
    fl::optional<Json> receivedResp = client.readRequest();
    FL_REQUIRE(receivedResp);
    auto resultVal = (*receivedResp)["result"].as_int();
    FL_REQUIRE(resultVal.has_value());
    FL_CHECK_EQ(30, *resultVal);
}

// Test: Broadcast responses to all clients
FL_TEST_CASE("MockHttpServer - Broadcast responses to all clients") {
    MockHttpServer server(47701);
    server.connect();

    MockHttpClient client1(server);
    MockHttpClient client2(server);

    client1.connect();
    client2.connect();

    FL_CHECK_EQ(2, server.getClientCount());

    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 99);
    response.set("id", 1);

    server.writeResponse(response);

    // Both clients should receive the response
    fl::optional<Json> resp1 = client1.readRequest();
    fl::optional<Json> resp2 = client2.readRequest();

    FL_REQUIRE(resp1);
    FL_REQUIRE(resp2);

    auto result1 = (*resp1)["result"].as_int();
    auto result2 = (*resp2)["result"].as_int();
    FL_REQUIRE(result1.has_value());
    FL_REQUIRE(result2.has_value());
    FL_CHECK_EQ(99, *result1);
    FL_CHECK_EQ(99, *result2);
}

// Test: Server receives requests from multiple clients
FL_TEST_CASE("MockHttpServer - Server receives requests from multiple clients") {
    MockHttpServer server(47701);
    server.connect();

    MockHttpClient client1(server);
    MockHttpClient client2(server);

    client1.connect();
    client2.connect();

    // Client 1 sends request
    Json request1 = Json::object();
    request1.set("jsonrpc", "2.0");
    request1.set("method", "test1");
    request1.set("id", 1);
    client1.writeResponse(request1);

    // Client 2 sends request
    Json request2 = Json::object();
    request2.set("jsonrpc", "2.0");
    request2.set("method", "test2");
    request2.set("id", 2);
    client2.writeResponse(request2);

    // Server receives both requests
    fl::optional<Json> req1 = server.readRequest();
    fl::optional<Json> req2 = server.readRequest();

    FL_REQUIRE(req1);
    FL_REQUIRE(req2);

    // Requests may arrive in any order
    bool hasTest1 = false;
    bool hasTest2 = false;

    auto method1 = (*req1)["method"].as_string();
    auto method2 = (*req2)["method"].as_string();
    FL_REQUIRE(method1.has_value());
    FL_REQUIRE(method2.has_value());

    if (*method1 == "test1") hasTest1 = true;
    if (*method1 == "test2") hasTest2 = true;
    if (*method2 == "test1") hasTest1 = true;
    if (*method2 == "test2") hasTest2 = true;

    FL_CHECK(hasTest1);
    FL_CHECK(hasTest2);
}

// Test: MockHttpServer - Set current time
FL_TEST_CASE("MockHttpServer - Set current time") {
    MockHttpServer server(47701);
    server.connect();
    server.setCurrentTime(1000);
    FL_CHECK_EQ(1000, server.getCurrentTimeMs());
}

// Test: MockHttpServer - Advance time
FL_TEST_CASE("MockHttpServer - Advance time") {
    MockHttpServer server(47701);
    server.connect();
    server.setCurrentTime(1000);
    server.advanceTime(500);
    FL_CHECK_EQ(1500, server.getCurrentTimeMs());
}

// Test: MockHttpClient - Set current time
FL_TEST_CASE("MockHttpClient - Set current time") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);
    client.setCurrentTime(2000);
    FL_CHECK_EQ(2000, client.getCurrentTimeMs());
}

// Test: MockHttpClient - Advance time
FL_TEST_CASE("MockHttpClient - Advance time") {
    MockHttpServer server(47701);
    server.connect();
    MockHttpClient client(server);
    client.setCurrentTime(2000);
    client.advanceTime(300);
    FL_CHECK_EQ(2300, client.getCurrentTimeMs());
}

// Test: Multiple sequential requests
FL_TEST_CASE("MockHttpClient/Server - Multiple sequential requests") {
    MockHttpServer server(47701);
    server.connect();

    MockHttpClient client(server);
    client.connect();

    for (int i = 0; i < 5; i++) {
        Json request = Json::object();
        request.set("jsonrpc", "2.0");
        request.set("method", "echo");
        Json params = Json::array();
        params.push_back(i);
        request.set("params", params);
        request.set("id", i);

        client.writeResponse(request);

        fl::optional<Json> receivedReq = server.readRequest();
        FL_REQUIRE(receivedReq);

        auto idVal = (*receivedReq)["id"].as_int();
        FL_REQUIRE(idVal.has_value());
        FL_CHECK_EQ(i, *idVal);
    }
}
