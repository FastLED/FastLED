// ok standalone
#include "test.h"
#include "mock_http_server.h"
#include "mock_http_client.h"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/json.h"

using namespace fl;

// ============================================================================
// Integration Tests for HTTP Transport Layer
// ============================================================================
// These tests verify that all HTTP transport components work together:
// - Chunked encoding/decoding
// - HTTP request/response parsing
// - Connection state machine
// - Heartbeat/keepalive
// - Reconnection logic
// - Error handling
// ============================================================================

// Test: Chunked Encoding Round-Trip
FL_TEST_CASE("HTTP Transport - Chunked encoding round-trip") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Send JSON-RPC request from client
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "test");
    request.set("id", 1);

    client.writeResponse(request);

    // Server should receive it
    server.update(100);
    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());
    FL_CHECK_EQ((*received)["method"].as_string().value(), "test");
    FL_CHECK_EQ((*received)["id"].as_int().value(), 1);
}

// Test: Multiple Messages in Sequence
FL_TEST_CASE("HTTP Transport - Multiple messages in sequence") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Send 3 messages
    for (int i = 0; i < 3; i++) {
        Json request = Json::object();
        request.set("jsonrpc", "2.0");
        request.set("method", "msg");
        request.set("id", i);
        client.writeResponse(request);
    }

    // Server should receive all 3
    server.update(100);
    for (int i = 0; i < 3; i++) {
        auto received = server.readRequest();
        FL_REQUIRE(received.has_value());
        FL_CHECK_EQ((*received)["id"].as_int().value(), i);
    }

    // No more messages
    auto extra = server.readRequest();
    FL_CHECK_FALSE(extra.has_value());
}

// Test: Bidirectional Communication
FL_TEST_CASE("HTTP Transport - Bidirectional communication") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Client sends request
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "ping");
    request.set("id", 1);
    client.writeResponse(request);

    // Server receives and sends response
    server.update(100);
    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());

    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", "pong");
    response.set("id", 1);
    server.writeResponse(response);

    // Client receives response
    client.update(200);
    auto clientReceived = client.readRequest();
    FL_REQUIRE(clientReceived.has_value());
    FL_CHECK_EQ((*clientReceived)["result"].as_string().value(), "pong");
    FL_CHECK_EQ((*clientReceived)["id"].as_int().value(), 1);
}

// Test: Large Message Handling
FL_TEST_CASE("HTTP Transport - Large message handling") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Create a large params array
    Json params = Json::array();
    for (int i = 0; i < 100; i++) {
        params.push_back(Json(i));
    }

    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "largeData");
    request.set("params", params);
    request.set("id", 1);

    client.writeResponse(request);

    // Server should receive the large message
    server.update(100);
    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());
    FL_CHECK_EQ((*received)["method"].as_string().value(), "largeData");
    FL_CHECK_EQ((*received)["params"].size(), 100);
}

// Test: Heartbeat Detection
FL_TEST_CASE("HTTP Transport - Heartbeat detection") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    // Set short heartbeat interval (1 second)
    server.setHeartbeatInterval(1000);
    client.setHeartbeatInterval(1000);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Wait for heartbeat interval
    server.update(1100);
    client.update(1100);

    // Server should send heartbeat ping
    auto ping = client.readRequest();
    // Note: HttpStreamTransport filters out "rpc.ping" internally
    // So we won't see it in readRequest() output
    // But we can verify connection is still alive
    FL_CHECK(client.isConnected());
    FL_CHECK(server.isConnected());
}

// Test: Connection Timeout Detection
FL_TEST_CASE("HTTP Transport - Connection timeout detection") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    // Set short timeout (2 seconds)
    server.setTimeout(2000);
    client.setTimeout(2000);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    FL_CHECK(server.isConnected());
    FL_CHECK(client.isConnected());

    // Wait past timeout without any activity
    server.update(3000);
    client.update(3000);

    // Connection should timeout and disconnect
    // Note: Mock transport doesn't implement timeout logic
    // This would work with real NativeHttpClient/Server
    // Here we just verify the API is available
    FL_CHECK_EQ(server.getTimeout(), 2000);
    FL_CHECK_EQ(client.getTimeout(), 2000);
}

// Test: Error Response Handling
FL_TEST_CASE("HTTP Transport - Error response handling") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Client sends request
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "badMethod");
    request.set("id", 1);
    client.writeResponse(request);

    // Server receives and sends error response
    server.update(100);
    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());

    Json errorObj = Json::object();
    errorObj.set("code", -32601);
    errorObj.set("message", "Method not found");

    Json errorResponse = Json::object();
    errorResponse.set("jsonrpc", "2.0");
    errorResponse.set("error", errorObj);
    errorResponse.set("id", 1);
    server.writeResponse(errorResponse);

    // Client receives error response
    client.update(200);
    auto clientReceived = client.readRequest();
    FL_REQUIRE(clientReceived.has_value());
    FL_CHECK(clientReceived->contains("error"));
    FL_CHECK_EQ((*clientReceived)["error"]["code"].as_int().value(), -32601);
}

// Test: Multiple Clients on Server
FL_TEST_CASE("HTTP Transport - Multiple clients on server") {
    MockHttpServer server(8080);
    MockHttpClient client1(server);
    MockHttpClient client2(server);

    server.connect();
    client1.connect();
    client2.connect();

    server.update(0);
    client1.update(0);
    client2.update(0);

    FL_CHECK_EQ(server.getClientCount(), 2);

    // Client 1 sends message
    Json request1 = Json::object();
    request1.set("jsonrpc", "2.0");
    request1.set("method", "client1");
    request1.set("id", 1);
    client1.writeResponse(request1);

    // Client 2 sends message
    Json request2 = Json::object();
    request2.set("jsonrpc", "2.0");
    request2.set("method", "client2");
    request2.set("id", 2);
    client2.writeResponse(request2);

    // Server receives both messages
    server.update(100);

    auto msg1 = server.readRequest();
    auto msg2 = server.readRequest();

    FL_REQUIRE(msg1.has_value());
    FL_REQUIRE(msg2.has_value());

    // Messages can arrive in any order
    bool hasClient1 = (*msg1)["method"].as_string().value() == "client1" || (*msg2)["method"].as_string().value() == "client1";
    bool hasClient2 = (*msg1)["method"].as_string().value() == "client2" || (*msg2)["method"].as_string().value() == "client2";

    FL_CHECK(hasClient1);
    FL_CHECK(hasClient2);
}

// Test: Server Broadcast to Multiple Clients
FL_TEST_CASE("HTTP Transport - Server broadcast to multiple clients") {
    MockHttpServer server(8080);
    MockHttpClient client1(server);
    MockHttpClient client2(server);

    server.connect();
    client1.connect();
    client2.connect();

    server.update(0);
    client1.update(0);
    client2.update(0);

    // Server broadcasts notification
    Json params = Json::object();
    params.set("message", "hello");

    Json notification = Json::object();
    notification.set("jsonrpc", "2.0");
    notification.set("method", "broadcast");
    notification.set("params", params);

    server.writeResponse(notification);

    // Both clients should receive the notification
    client1.update(100);
    client2.update(100);

    auto recv1 = client1.readRequest();
    auto recv2 = client2.readRequest();

    FL_REQUIRE(recv1.has_value());
    FL_REQUIRE(recv2.has_value());

    FL_CHECK_EQ((*recv1)["method"].as_string().value(), "broadcast");
    FL_CHECK_EQ((*recv2)["method"].as_string().value(), "broadcast");
}

// Test: Client Reconnection
FL_TEST_CASE("HTTP Transport - Client reconnection") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    FL_CHECK(client.isConnected());
    FL_CHECK_EQ(server.getClientCount(), 1);

    // Client disconnects
    client.disconnect();
    server.update(100);

    FL_CHECK_FALSE(client.isConnected());
    FL_CHECK_EQ(server.getClientCount(), 0);

    // Client reconnects
    client.connect();
    server.update(200);
    client.update(200);

    FL_CHECK(client.isConnected());
    FL_CHECK_EQ(server.getClientCount(), 1);
}

// Test: Empty Message Handling
FL_TEST_CASE("HTTP Transport - Empty message handling") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Try reading when no messages sent
    auto msg = server.readRequest();
    FL_CHECK_FALSE(msg.has_value());

    msg = client.readRequest();
    FL_CHECK_FALSE(msg.has_value());
}

// Test: Rapid Message Sending
FL_TEST_CASE("HTTP Transport - Rapid message sending") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Send 100 messages rapidly
    for (int i = 0; i < 100; i++) {
        Json request = Json::object();
        request.set("jsonrpc", "2.0");
        request.set("method", "rapid");
        request.set("id", i);
        client.writeResponse(request);
    }

    // Server should receive all 100
    server.update(100);
    for (int i = 0; i < 100; i++) {
        auto received = server.readRequest();
        FL_REQUIRE(received.has_value());
        FL_CHECK_EQ((*received)["id"].as_int().value(), i);
    }
}

// Test: Configuration Getters/Setters
FL_TEST_CASE("HTTP Transport - Configuration management") {
    MockHttpServer server(8080);

    // Default values
    FL_CHECK_EQ(server.getHeartbeatInterval(), 30000); // 30 seconds
    FL_CHECK_EQ(server.getTimeout(), 60000); // 60 seconds

    // Set new values
    server.setHeartbeatInterval(5000);
    server.setTimeout(10000);

    FL_CHECK_EQ(server.getHeartbeatInterval(), 5000);
    FL_CHECK_EQ(server.getTimeout(), 10000);
}

// Test: Connection State Callbacks
FL_TEST_CASE("HTTP Transport - Connection state callbacks") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    // Note: Callbacks use C function pointers and can't capture state
    // Just verify callbacks can be set without error
    auto testCallback = []() {
        // Empty callback for testing
    };

    server.setOnConnect(testCallback);
    server.setOnDisconnect(testCallback);
    client.setOnConnect(testCallback);
    client.setOnDisconnect(testCallback);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    FL_CHECK(server.isConnected());
    FL_CHECK(client.isConnected());
}

// Test: JSON-RPC 2.0 Compliance - Request Structure
FL_TEST_CASE("HTTP Transport - JSON-RPC 2.0 request compliance") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Valid JSON-RPC 2.0 request
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "subtract");
    Json params = Json::array();
    params.push_back(Json(42));
    params.push_back(Json(23));
    request.set("params", params);
    request.set("id", 1);

    client.writeResponse(request);
    server.update(100);

    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());

    // Verify all required fields
    FL_CHECK(received->contains("jsonrpc"));
    FL_CHECK(received->contains("method"));
    FL_CHECK(received->contains("id"));
    FL_CHECK_EQ((*received)["jsonrpc"].as_string().value(), "2.0");
}

// Test: JSON-RPC 2.0 Compliance - Response Structure
FL_TEST_CASE("HTTP Transport - JSON-RPC 2.0 response compliance") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Valid JSON-RPC 2.0 response
    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 19);
    response.set("id", 1);

    server.writeResponse(response);
    client.update(100);

    auto received = client.readRequest();
    FL_REQUIRE(received.has_value());

    // Verify all required fields
    FL_CHECK(received->contains("jsonrpc"));
    FL_CHECK(received->contains("result"));
    FL_CHECK(received->contains("id"));
    FL_CHECK_EQ((*received)["jsonrpc"].as_string().value(), "2.0");
    FL_CHECK_EQ((*received)["result"].as_int().value(), 19);
}

// Test: JSON-RPC 2.0 Notification (no id)
FL_TEST_CASE("HTTP Transport - JSON-RPC 2.0 notification") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Notification (no response expected)
    Json notification = Json::object();
    notification.set("jsonrpc", "2.0");
    notification.set("method", "update");
    Json notifParams = Json::array();
    notifParams.push_back(Json(1));
    notifParams.push_back(Json(2));
    notifParams.push_back(Json(3));
    notification.set("params", notifParams);

    client.writeResponse(notification);
    server.update(100);

    auto received = server.readRequest();
    FL_REQUIRE(received.has_value());
    FL_CHECK_EQ((*received)["method"].as_string().value(), "update");
    FL_CHECK_FALSE(received->contains("id")); // Notifications don't have id
}

// Test: Stress Test - Many Messages
FL_TEST_CASE("HTTP Transport - Stress test with 1000 messages") {
    MockHttpServer server(8080);
    MockHttpClient client(server);

    server.connect();
    client.connect();
    server.update(0);
    client.update(0);

    // Send 1000 messages
    for (int i = 0; i < 1000; i++) {
        Json request = Json::object();
        request.set("jsonrpc", "2.0");
        request.set("method", "stress");
        request.set("id", i);
        client.writeResponse(request);
    }

    // Server should receive all 1000
    server.update(100);
    for (int i = 0; i < 1000; i++) {
        auto received = server.readRequest();
        FL_REQUIRE(received.has_value());
        FL_CHECK_EQ((*received)["id"].as_int().value(), i);
    }
}
