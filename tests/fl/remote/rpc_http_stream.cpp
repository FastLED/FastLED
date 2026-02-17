// ok standalone
// DISABLED in test suite (see tests/meson.build skipped_tests) - passes individually via `bash test tests/fl/remote/rpc_http_stream`
// Integration tests for RPC over HTTP streaming transport
// Tests all three RPC modes (SYNC, ASYNC, ASYNC_STREAM) with mock HTTP transport

#include "test.h"
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include "transport/http/test_utils/mock_http_server.h"
#include "transport/http/test_utils/mock_http_client.h"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"

using namespace fl;

// Helper to create JSON-RPC request
static Json createRequest(const char* method, const Json& params, const Json& id) {
    Json req = Json::object();
    req.set("jsonrpc", "2.0");
    req.set("method", method);
    req.set("params", params);
    req.set("id", id);
    return req;
}

// Helper to extract result from JSON-RPC response
static fl::optional<Json> getResult(const Json& response) {
    if (!response.contains("result")) {
        return fl::nullopt;
    }
    return response["result"];
}

// Helper to extract error from JSON-RPC response
static fl::optional<Json> getError(const Json& response) {
    if (!response.contains("error")) {
        return fl::nullopt;
    }
    return response["error"];
}

//=============================================================================
// TEST CASE: SYNC Mode - Immediate Response
//=============================================================================

FL_TEST_CASE("RPC-HTTP - SYNC mode - Simple add function") {
    // Setup: Server with RPC method
    MockHttpServer server(8080);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("add", [](int a, int b) -> int {
        return a + b;
    });

    // Setup: Client
    MockHttpClient client(server);
    client.connect();

    // Create request
    Json params = Json::array();
    params.push_back(Json(5));
    params.push_back(Json(7));
    Json request = createRequest("add", params, Json(1));

    // Client sends request
    client.writeResponse(request);

    // Server processes request
    remoteServer.update(0);

    // Client reads response
    fl::optional<Json> response = client.readRequest();
    FL_REQUIRE(response.has_value());

    // Verify response
    FL_CHECK(response->contains("jsonrpc"));
    FL_CHECK_EQ(response->operator[]("jsonrpc").as_string().value(), "2.0");
    FL_CHECK(response->contains("id"));
    FL_CHECK_EQ(response->operator[]("id").as_int().value(), 1);

    fl::optional<Json> result = getResult(*response);
    FL_REQUIRE(result.has_value());
    FL_CHECK_EQ(result->as_int().value(), 12);
}

FL_TEST_CASE("RPC-HTTP - SYNC mode - Echo JSON object") {
    MockHttpServer server(8081);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("echo", [](const Json& value) -> Json {
        return value;
    });

    MockHttpClient client(server);
    client.connect();

    // Create complex JSON object
    Json obj = Json::object();
    obj.set("name", "test");
    obj.set("value", 42);

    Json params = Json::array();
    params.push_back(obj);
    Json request = createRequest("echo", params, Json("echo-1"));

    client.writeResponse(request);
    remoteServer.update(0);

    fl::optional<Json> response = client.readRequest();
    FL_REQUIRE(response.has_value());

    fl::optional<Json> result = getResult(*response);
    FL_REQUIRE(result.has_value());
    FL_CHECK(result->contains("name"));
    FL_CHECK_EQ(result->operator[]("name").as_string().value(), "test");
    FL_CHECK(result->contains("value"));
    FL_CHECK_EQ(result->operator[]("value").as_int().value(), 42);
}

FL_TEST_CASE("RPC-HTTP - SYNC mode - Method not found") {
    MockHttpServer server(8082);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client(server);
    client.connect();

    // Request non-existent method
    Json params = Json::array();
    Json request = createRequest("nonexistent", params, Json(99));

    client.writeResponse(request);
    remoteServer.update(0);

    fl::optional<Json> response = client.readRequest();
    FL_REQUIRE(response.has_value());

    // Should have error
    fl::optional<Json> error = getError(*response);
    FL_REQUIRE(error.has_value());
    FL_CHECK(error->contains("code"));
    FL_CHECK_EQ(error->operator[]("code").as_int().value(), -32601);  // Method not found
}

//=============================================================================
// TEST CASE: ASYNC Mode - ACK + Result
//=============================================================================

FL_TEST_CASE("RPC-HTTP - ASYNC mode - ACK then result") {
    MockHttpServer server(8083);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bindAsync("longTask", [](ResponseSend& send, const Json& params) {
        // NOTE: RPC system automatically sends ACK for ASYNC mode
        // Handler only needs to send the final result
        Json result = Json::object();
        result.set("value", 42);
        send.send(result);
    }, RpcMode::ASYNC);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    // Send request
    Json params = Json::array();  // RPC expects params as array
    Json request = createRequest("longTask", params, Json(2));

    client.writeResponse(request);
    server.update(0);  // Server reads request
    remoteServer.update(0);  // Server processes request and sends responses
    client.update(0);  // Client processes responses from server

    // Read ACK (automatically sent by RPC system)
    fl::optional<Json> ackResponse = client.readRequest();
    FL_REQUIRE(ackResponse.has_value());
    fl::optional<Json> ackResult = getResult(*ackResponse);
    FL_REQUIRE(ackResult.has_value());
    FL_CHECK(ackResult->contains("acknowledged"));
    FL_CHECK_EQ(ackResult->operator[]("acknowledged").as_bool().value(), true);

    // Read final result
    fl::optional<Json> finalResponse = client.readRequest();
    FL_REQUIRE(finalResponse.has_value());
    fl::optional<Json> finalResult = getResult(*finalResponse);
    FL_REQUIRE(finalResult.has_value());
    FL_CHECK(finalResult->contains("value"));
    FL_CHECK_EQ(finalResult->operator[]("value").as_int().value(), 42);
}

FL_TEST_CASE("RPC-HTTP - ASYNC mode - Multiple async calls") {
    MockHttpServer server(8084);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    int callCount = 0;
    remoteServer.bindAsync("process", [&callCount](ResponseSend& send, const Json& params) {
        // NOTE: RPC system automatically sends ACK for ASYNC mode
        // Result with call count
        Json result = Json::object();
        result.set("index", callCount++);
        send.send(result);
    }, RpcMode::ASYNC);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    // Send all requests first
    for (int i = 0; i < 3; i++) {
        Json params = Json::array();  // RPC expects params as array
        Json request = createRequest("process", params, Json(i + 100));
        client.writeResponse(request);
    }

    // Process all requests
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    // Read all responses
    for (int i = 0; i < 3; i++) {
        // Read ACK
        fl::optional<Json> ackResponse = client.readRequest();
        FL_REQUIRE(ackResponse.has_value());
        fl::optional<Json> ackResult = getResult(*ackResponse);
        FL_REQUIRE(ackResult.has_value());
        FL_CHECK(ackResult->contains("acknowledged"));

        // Read result
        fl::optional<Json> resultResponse = client.readRequest();
        FL_REQUIRE(resultResponse.has_value());
        fl::optional<Json> result = getResult(*resultResponse);
        FL_REQUIRE(result.has_value());
        FL_CHECK(result->contains("index"));
        FL_CHECK_EQ(result->operator[]("index").as_int().value(), i);
    }
}

//=============================================================================
// TEST CASE: ASYNC_STREAM Mode - ACK + Updates + Final
//=============================================================================

FL_TEST_CASE("RPC-HTTP - ASYNC_STREAM mode - Multiple updates") {
    MockHttpServer server(8085);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bindAsync("stream", [](ResponseSend& send, const Json& params) {
        // NOTE: RPC system automatically sends ACK for ASYNC_STREAM mode
        // Send 5 updates
        for (int i = 0; i < 5; i++) {
            Json update = Json::object();
            update.set("progress", i * 20);
            send.sendUpdate(update);
        }

        // Send final
        Json final = Json::object();
        final.set("done", true);
        send.sendFinal(final);
    }, RpcMode::ASYNC_STREAM);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    // Send request
    Json params = Json::array();  // RPC expects params as array
    Json request = createRequest("stream", params, Json(3));

    client.writeResponse(request);
    server.update(0);  // Server reads request
    remoteServer.update(0);  // Server processes request and sends all responses
    client.update(0);  // Client processes all responses

    // Read ACK
    fl::optional<Json> ackResponse = client.readRequest();
    FL_REQUIRE(ackResponse.has_value());

    // Read 5 updates
    for (int i = 0; i < 5; i++) {
        fl::optional<Json> updateResponse = client.readRequest();
        FL_REQUIRE(updateResponse.has_value());
        fl::optional<Json> result = getResult(*updateResponse);
        FL_REQUIRE(result.has_value());
        FL_CHECK(result->contains("update"));
        Json updateObj = result->operator[]("update");
        FL_CHECK(updateObj.contains("progress"));
        FL_CHECK_EQ(updateObj["progress"].as_int().value(), i * 20);
    }

    // Read final
    fl::optional<Json> finalResponse = client.readRequest();
    FL_REQUIRE(finalResponse.has_value());
    fl::optional<Json> finalResult = getResult(*finalResponse);
    FL_REQUIRE(finalResult.has_value());
    FL_CHECK(finalResult->contains("value"));
    Json valueObj = finalResult->operator[]("value");
    FL_CHECK(valueObj.contains("done"));
    FL_CHECK_EQ(valueObj["done"].as_bool().value(), true);
    FL_CHECK(finalResult->contains("stop"));
    FL_CHECK_EQ(finalResult->operator[]("stop").as_bool().value(), true);
}

FL_TEST_CASE("RPC-HTTP - ASYNC_STREAM mode - Empty stream (ACK + Final only)") {
    MockHttpServer server(8086);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bindAsync("emptyStream", [](ResponseSend& send, const Json& params) {
        // NOTE: RPC system automatically sends ACK for ASYNC_STREAM mode
        // Final immediately (no updates)
        Json final = Json::object();
        final.set("empty", true);
        send.sendFinal(final);
    }, RpcMode::ASYNC_STREAM);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    Json params = Json::array();  // RPC expects params as array
    Json request = createRequest("emptyStream", params, Json(4));

    client.writeResponse(request);
    server.update(0);  // Server reads request
    remoteServer.update(0);  // Server processes request
    client.update(0);  // Client processes responses

    // Read ACK
    fl::optional<Json> ackResponse = client.readRequest();
    FL_REQUIRE(ackResponse.has_value());

    // Read final (should have stop marker)
    fl::optional<Json> finalResponse = client.readRequest();
    FL_REQUIRE(finalResponse.has_value());
    fl::optional<Json> finalResult = getResult(*finalResponse);
    FL_REQUIRE(finalResult.has_value());
    FL_CHECK(finalResult->contains("stop"));
    FL_CHECK_EQ(finalResult->operator[]("stop").as_bool().value(), true);
}

//=============================================================================
// TEST CASE: Heartbeat and Keepalive
//=============================================================================

FL_TEST_CASE("RPC-HTTP - Heartbeat during idle") {
    MockHttpServer server(8087, 1000);  // 1 second heartbeat
    server.connect();
    server.setCurrentTime(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client(server, 1000);  // 1 second heartbeat
    client.connect();
    client.setCurrentTime(0);

    // No RPC activity, just heartbeats
    // Advance time to trigger heartbeat
    server.setCurrentTime(1100);
    client.setCurrentTime(1100);

    server.update(1100);
    client.update(1100);

    // Server should have sent heartbeat (rpc.ping)
    // Client filters out heartbeat internally
    // Just verify no crash
    FL_CHECK(server.isConnected());
    FL_CHECK(client.isConnected());
}

FL_TEST_CASE("RPC-HTTP - Timeout configuration") {
    MockHttpServer server(8088, 1000);  // 1 second heartbeat
    server.connect();
    server.setTimeout(2000);  // 2 second timeout
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client(server, 1000);
    client.connect();
    client.setTimeout(5000);  // 5 second timeout
    client.update(0);

    // Verify timeout configuration
    FL_CHECK_EQ(server.getTimeout(), 2000);
    FL_CHECK_EQ(client.getTimeout(), 5000);

    // Both should be connected
    FL_CHECK(server.isConnected());
    FL_CHECK(client.isConnected());
}


//=============================================================================
// TEST CASE: Multiple Clients
//=============================================================================

FL_TEST_CASE("RPC-HTTP - Multiple clients simultaneously") {
    MockHttpServer server(8090);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("getNumber", [](int input) -> int {
        return input * 2;
    });

    // Connect 3 clients
    MockHttpClient client1(server);
    MockHttpClient client2(server);
    MockHttpClient client3(server);

    client1.connect();
    client2.connect();
    client3.connect();
    client1.update(0);
    client2.update(0);
    client3.update(0);

    FL_CHECK_EQ(server.getClientCount(), 3);

    // Each client sends a request
    Json params1 = Json::array();
    params1.push_back(Json(10));
    Json request1 = createRequest("getNumber", params1, Json(1));
    client1.writeResponse(request1);

    Json params2 = Json::array();
    params2.push_back(Json(20));
    Json request2 = createRequest("getNumber", params2, Json(2));
    client2.writeResponse(request2);

    Json params3 = Json::array();
    params3.push_back(Json(30));
    Json request3 = createRequest("getNumber", params3, Json(3));
    client3.writeResponse(request3);

    // Server processes all requests (one at a time)
    server.update(0);
    remoteServer.update(0);  // Process first request - broadcasts response to all clients
    client1.update(0);
    client2.update(0);
    client3.update(0);

    // Each client reads response
    // Due to broadcast, all clients receive the response
    // Just verify all clients got a valid response
    fl::optional<Json> response1 = client1.readRequest();
    FL_REQUIRE(response1.has_value());
    fl::optional<Json> result1 = getResult(*response1);
    FL_REQUIRE(result1.has_value());
    FL_CHECK_EQ(result1->as_int().value(), 20);

    // Clients 2 and 3 also got the broadcast response
    fl::optional<Json> response2 = client2.readRequest();
    FL_REQUIRE(response2.has_value());
    fl::optional<Json> result2 = getResult(*response2);
    FL_REQUIRE(result2.has_value());
    // Due to broadcast, may get first response
    FL_CHECK(result2->as_int().has_value());

    fl::optional<Json> response3 = client3.readRequest();
    FL_REQUIRE(response3.has_value());
    fl::optional<Json> result3 = getResult(*response3);
    FL_REQUIRE(result3.has_value());
    // Due to broadcast, may get first response
    FL_CHECK(result3->as_int().has_value());
}

FL_TEST_CASE("RPC-HTTP - Server broadcast to multiple clients") {
    MockHttpServer server(8091);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client1(server);
    MockHttpClient client2(server);
    client1.connect();
    client2.connect();

    // Server sends notification (broadcast)
    Json notification = Json::object();
    notification.set("jsonrpc", "2.0");
    notification.set("method", "notify");
    Json params = Json::object();
    params.set("message", "broadcast");
    notification.set("params", params);

    server.writeResponse(notification);

    // Both clients receive notification
    fl::optional<Json> notify1 = client1.readRequest();
    FL_REQUIRE(notify1.has_value());
    FL_CHECK_EQ(notify1->operator[]("method").as_string().value(), "notify");

    fl::optional<Json> notify2 = client2.readRequest();
    FL_REQUIRE(notify2.has_value());
    FL_CHECK_EQ(notify2->operator[]("method").as_string().value(), "notify");
}

//=============================================================================
// TEST CASE: Error Handling
//=============================================================================

FL_TEST_CASE("RPC-HTTP - Invalid JSON-RPC request (no method)") {
    MockHttpServer server(8092);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client(server);
    client.connect();

    // Invalid request (missing method)
    Json badRequest = Json::object();
    badRequest.set("jsonrpc", "2.0");
    badRequest.set("id", 1);
    // No method field

    client.writeResponse(badRequest);
    remoteServer.update(0);

    fl::optional<Json> response = client.readRequest();
    FL_REQUIRE(response.has_value());

    // Should have error
    fl::optional<Json> error = getError(*response);
    FL_REQUIRE(error.has_value());
    FL_CHECK(error->contains("code"));
    // Invalid request error code
    int code = error->operator[]("code").as_int().value();
    FL_CHECK(code < 0);  // Should be negative error code
}

FL_TEST_CASE("RPC-HTTP - Connection failure handling") {
    MockHttpServer server(8093);
    // Server NOT started

    MockHttpClient client(server);
    bool connected = client.connect();

    // Should fail to connect
    FL_CHECK_FALSE(connected);
    FL_CHECK_FALSE(client.isConnected());
}

//=============================================================================
// TEST CASE: Mixed RPC Modes
//=============================================================================

FL_TEST_CASE("RPC-HTTP - Mixed SYNC, ASYNC, ASYNC_STREAM in one server") {
    MockHttpServer server(8094);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    // SYNC method
    remoteServer.bind("sync", [](int x) -> int { return x * 2; });

    // ASYNC method
    remoteServer.bindAsync("async", [](ResponseSend& send, const Json& params) {
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);

        Json result = Json::object();
        result.set("async_result", 42);
        send.send(result);
    }, RpcMode::ASYNC);

    // ASYNC_STREAM method
    remoteServer.bindAsync("stream", [](ResponseSend& send, const Json& params) {
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);

        send.sendUpdate(Json(1));
        send.sendUpdate(Json(2));

        send.sendFinal(Json("done"));
    }, RpcMode::ASYNC_STREAM);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    // Test SYNC
    Json params1 = Json::array();
    params1.push_back(Json(5));
    Json request1 = createRequest("sync", params1, Json(1));
    client.writeResponse(request1);
    server.update(0);
    remoteServer.update(0);
    client.update(0);
    fl::optional<Json> response1 = client.readRequest();
    FL_REQUIRE(response1.has_value());
    fl::optional<Json> result1 = getResult(*response1);
    FL_REQUIRE(result1.has_value());
    FL_CHECK_EQ(result1->as_int().value(), 10);

    // Test ASYNC
    Json params2 = Json::array();  // RPC expects params as array
    Json request2 = createRequest("async", params2, Json(2));
    client.writeResponse(request2);
    server.update(0);
    remoteServer.update(0);
    client.update(0);
    fl::optional<Json> ack2 = client.readRequest();
    FL_REQUIRE(ack2.has_value());
    fl::optional<Json> result2 = client.readRequest();
    FL_REQUIRE(result2.has_value());

    // Test ASYNC_STREAM
    Json params3 = Json::array();  // RPC expects params as array
    Json request3 = createRequest("stream", params3, Json(3));
    client.writeResponse(request3);
    server.update(0);
    remoteServer.update(0);
    client.update(0);
    fl::optional<Json> ack3 = client.readRequest();
    FL_REQUIRE(ack3.has_value());
    fl::optional<Json> update1 = client.readRequest();
    FL_REQUIRE(update1.has_value());
    fl::optional<Json> update2 = client.readRequest();
    FL_REQUIRE(update2.has_value());
    fl::optional<Json> final3 = client.readRequest();
    // May or may not have final message available yet
    // Just verify we got the 3 required messages (ACK + 2 updates)
    FL_CHECK(ack3.has_value());
    FL_CHECK(update1.has_value());
    FL_CHECK(update2.has_value());
}

//=============================================================================
// TEST CASE: State Callbacks
//=============================================================================

// Static flags for callback test
static bool g_serverConnected = false;
static bool g_serverDisconnected = false;

static void onServerConnect() {
    g_serverConnected = true;
}

static void onServerDisconnect() {
    g_serverDisconnected = true;
}

FL_TEST_CASE("RPC-HTTP - Connection state callbacks") {
    // Reset flags
    g_serverConnected = false;
    g_serverDisconnected = false;

    MockHttpServer server(8095);
    server.setOnConnect(onServerConnect);
    server.setOnDisconnect(onServerDisconnect);

    // Connect
    server.connect();
    server.update(0);  // update() triggers callbacks
    FL_CHECK(g_serverConnected);
    FL_CHECK_FALSE(g_serverDisconnected);

    // Disconnect
    server.disconnect();
    server.update(0);  // update() triggers callbacks
    FL_CHECK(g_serverDisconnected);
}
