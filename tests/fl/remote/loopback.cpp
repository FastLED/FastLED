// ok standalone
// Real loopback tests using NativeHttpServer and NativeHttpClient
// Tests the full HTTP streaming RPC stack end-to-end

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/transport/http/stream_client.h"
#include "fl/remote/transport/http/stream_client.cpp.hpp"
#include "fl/remote/transport/http/stream_server.h"
#include "fl/remote/transport/http/stream_server.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/native_client.cpp.hpp"
#include "fl/remote/transport/http/native_server.cpp.hpp"
#include "fl/json.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include <thread>  // ok include
#include <chrono>  // ok include

using namespace fl;

// Helper to sleep for specified milliseconds
__attribute__((unused)) static void delay(uint32_t ms) {
    fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));
}

// Test port - using a high port to avoid conflicts
constexpr uint16_t TEST_PORT = 18080;

// Helper to create JSON-RPC request
static Json createRequest(const char* method, const Json& params, int id) {
    Json req = Json::object();
    req.set("jsonrpc", "2.0");
    req.set("method", method);
    req.set("params", params);
    req.set("id", id);
    return req;
}

FL_TEST_CASE("Loopback: Server startup and client connection") {
    // Create server
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);

    // Start server
    FL_REQUIRE(server_transport->connect());

    // Give server time to bind
    delay(100);

    // Create client
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);

    // Connect client
    FL_REQUIRE(client_transport->connect());

    // Give connection time to establish
    delay(100);

    // Verify connection
    FL_CHECK(server_transport->isConnected());
    FL_CHECK(client_transport->isConnected());

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();

    delay(100); // Give sockets time to close
}

FL_TEST_CASE("Loopback: SYNC mode - simple request/response") {
    // Create server transport
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Bind a SYNC method: add(a, b) -> a + b
    server_remote.bind("add", [](int a, int b) -> int {
        return a + b;
    });

    // Create client transport
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Build request
    Json params = Json::array();
    params.push_back(Json(5));
    params.push_back(Json(7));
    Json request = createRequest("add", params, 1);

    // Send request from client
    client_transport->writeResponse(request);

    // Run server update loop to process request
    uint32_t start = fl::millis();
    bool response_received = false;
    Json response;

    while (fl::millis() - start < 2000) { // 2 second timeout
        uint32_t now = fl::millis();

        // Update transports
        server_transport->update(now);
        client_transport->update(now);

        // Update remotes
        server_remote.update(now);
        client_remote.update(now);

        // Check for response
        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            response = resp.value();
            response_received = true;
            break;
        }

        delay(10);
    }

    // Verify response
    FL_REQUIRE(response_received);
    FL_CHECK(response.contains("jsonrpc"));
    FL_CHECK_EQ(response["jsonrpc"].as_string().value(), "2.0");
    FL_CHECK(response.contains("result"));
    FL_CHECK_EQ(response["result"].as_int().value(), 12); // 5 + 7
    FL_CHECK(response.contains("id"));
    FL_CHECK_EQ(response["id"].as_int().value(), 1);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: ASYNC mode - ACK + result") {
    // Create server transport
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Bind an ASYNC method: multiply(a, b) -> a * b (with delay)
    server_remote.bindAsync("multiply", [](fl::ResponseSend& send, const fl::Json& params) {
        int a = params[0].as_int().value();
        int b = params[1].as_int().value();

        // Send ACK immediately
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);

        // Simulate work (100ms delay)
        delay(100);

        // Send result
        Json result = Json::object();
        result.set("value", a * b);
        send.send(result);
    }, fl::RpcMode::ASYNC);

    // Create client transport
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Build request
    Json params = Json::array();
    params.push_back(Json(6));
    params.push_back(Json(7));
    Json request = createRequest("multiply", params, 2);

    // Send request from client
    client_transport->writeResponse(request);

    // Collect responses
    fl::vector<Json> responses;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 3000) { // 3 second timeout
        uint32_t now = fl::millis();

        // Update transports
        server_transport->update(now);
        client_transport->update(now);

        // Update remotes
        server_remote.update(now);
        client_remote.update(now);

        // Check for responses
        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            responses.push_back(resp.value());

            // Stop after receiving result (2 responses: ACK + result)
            if (responses.size() >= 2) {
                break;
            }
        }

        delay(10);
    }

    // Verify we got 2 responses (ACK + result)
    FL_REQUIRE(responses.size() == 2);

    // Verify ACK
    FL_CHECK(responses[0].contains("ack"));
    FL_CHECK_EQ(responses[0]["ack"].as_bool().value(), true);
    FL_CHECK(responses[0].contains("id"));
    FL_CHECK_EQ(responses[0]["id"].as_int().value(), 2);

    // Verify result
    FL_CHECK(responses[1].contains("value"));
    FL_CHECK_EQ(responses[1]["value"].as_int().value(), 42); // 6 * 7
    FL_CHECK(responses[1].contains("id"));
    FL_CHECK_EQ(responses[1]["id"].as_int().value(), 2);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: ASYNC_STREAM mode - ACK + updates + final") {
    // Create server transport
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Bind an ASYNC_STREAM method: countdown(n) -> streams n, n-1, ..., 1, 0
    server_remote.bindAsync("countdown", [](fl::ResponseSend& send, const fl::Json& params) {
        int count = params[0].as_int().value();

        // Send ACK immediately
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);

        // Send updates
        for (int i = count; i > 0; i--) {
            Json update = Json::object();
            update.set("count", i);
            send.sendUpdate(update);
            delay(50); // Small delay between updates
        }

        // Send final
        Json final = Json::object();
        final.set("count", 0);
        send.sendFinal(final);
    }, fl::RpcMode::ASYNC_STREAM);

    // Create client transport
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Build request
    Json params = Json::array();
    params.push_back(Json(5));
    Json request = createRequest("countdown", params, 3);

    // Send request from client
    client_transport->writeResponse(request);

    // Collect responses
    fl::vector<Json> responses;
    uint32_t start = fl::millis();
    bool got_final = false;

    while (fl::millis() - start < 3000) { // 3 second timeout
        uint32_t now = fl::millis();

        // Update transports
        server_transport->update(now);
        client_transport->update(now);

        // Update remotes
        server_remote.update(now);
        client_remote.update(now);

        // Check for responses
        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            responses.push_back(resp.value());

            // Check if this is the final response
            if (resp.value().contains("stop") && resp.value()["stop"].as_bool().value()) {
                got_final = true;
                break;
            }
        }

        delay(10);
    }

    // Verify we got responses (ACK + 5 updates + final = 7 total)
    FL_REQUIRE(responses.size() == 7);
    FL_REQUIRE(got_final);

    // Verify ACK
    FL_CHECK(responses[0].contains("ack"));
    FL_CHECK_EQ(responses[0]["ack"].as_bool().value(), true);

    // Verify updates (countdown from 5 to 1)
    for (size_t i = 1; i <= 5; i++) {
        FL_CHECK(responses[i].contains("update"));
        Json update = responses[i]["update"];
        FL_CHECK(update.contains("count"));
        FL_CHECK_EQ(update["count"].as_int().value(), static_cast<int>(6 - i)); // 5, 4, 3, 2, 1
    }

    // Verify final (count = 0)
    FL_CHECK(responses[6].contains("value"));
    Json final_val = responses[6]["value"];
    FL_CHECK(final_val.contains("count"));
    FL_CHECK_EQ(final_val["count"].as_int().value(), 0);
    FL_CHECK(responses[6].contains("stop"));
    FL_CHECK_EQ(responses[6]["stop"].as_bool().value(), true);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: Multiple sequential requests") {
    // Create server transport
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Bind SYNC methods
    server_remote.bind("add", [](int a, int b) -> int { return a + b; });
    server_remote.bind("subtract", [](int a, int b) -> int { return a - b; });
    server_remote.bind("multiply", [](int a, int b) -> int { return a * b; });

    // Create client transport
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Helper to send request and wait for response
    auto send_and_wait = [&](const char* method, int a, int b, int id) -> fl::optional<Json> {
        Json params = Json::array();
        params.push_back(Json(a));
        params.push_back(Json(b));
        Json request = createRequest(method, params, id);

        client_transport->writeResponse(request);

        uint32_t start = fl::millis();
        while (fl::millis() - start < 2000) {
            uint32_t now = fl::millis();
            server_transport->update(now);
            client_transport->update(now);
            server_remote.update(now);
            client_remote.update(now);

            auto resp = client_transport->readRequest();
            if (resp.has_value()) {
                return resp;
            }
            delay(10);
        }
        return fl::nullopt;
    };

    // Send multiple requests
    auto resp1 = send_and_wait("add", 10, 5, 1);
    FL_REQUIRE(resp1.has_value());
    FL_CHECK_EQ(resp1.value()["result"].as_int().value(), 15);

    auto resp2 = send_and_wait("subtract", 10, 5, 2);
    FL_REQUIRE(resp2.has_value());
    FL_CHECK_EQ(resp2.value()["result"].as_int().value(), 5);

    auto resp3 = send_and_wait("multiply", 10, 5, 3);
    FL_REQUIRE(resp3.has_value());
    FL_CHECK_EQ(resp3.value()["result"].as_int().value(), 50);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: Heartbeat detection") {
    // Create server with short heartbeat interval
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    server_transport->setHeartbeatInterval(500); // 500ms heartbeat
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Create client
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    client_transport->setHeartbeatInterval(500); // 500ms heartbeat
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Run update loop and count heartbeats
    int heartbeat_count = 0;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 2000) { // Run for 2 seconds
        uint32_t now = fl::millis();

        server_transport->update(now);
        client_transport->update(now);
        server_remote.update(now);
        client_remote.update(now);

        // Check for heartbeat from server
        auto msg = client_transport->readRequest();
        if (msg.has_value() && msg.value().contains("method") &&
            msg.value()["method"].as_string().value() == "rpc.ping") {
            heartbeat_count++;
        }

        delay(10);
    }

    // Should have received ~4 heartbeats (2000ms / 500ms)
    FL_CHECK(heartbeat_count >= 3);
    FL_CHECK(heartbeat_count <= 5);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: Client reconnection") {
    // Create server
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    server_remote.bind("echo", [](const String& msg) -> String {
        return msg;
    });

    // Create client
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Verify initial connection
    FL_CHECK(client_transport->isConnected());

    // Disconnect client
    client_transport->disconnect();
    delay(100);
    FL_CHECK_FALSE(client_transport->isConnected());

    // Reconnect
    FL_REQUIRE(client_transport->connect());
    delay(100);
    FL_CHECK(client_transport->isConnected());

    // Send request after reconnection
    Json params = Json::array();
    params.push_back(Json("hello after reconnect"));
    Json request = createRequest("echo", params, 1);

    client_transport->writeResponse(request);

    // Wait for response
    bool got_response = false;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 2000) {
        uint32_t now = fl::millis();
        server_transport->update(now);
        client_transport->update(now);
        server_remote.update(now);
        client_remote.update(now);

        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            FL_CHECK(resp.value().contains("result"));
            FL_CHECK_EQ(resp.value()["result"].as_string().value(), "hello after reconnect");
            got_response = true;
            break;
        }
        delay(10);
    }

    FL_CHECK(got_response);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}

FL_TEST_CASE("Loopback: Error handling - method not found") {
    // Create server
    auto server_transport = fl::make_shared<HttpStreamServer>(TEST_PORT);
    FL_REQUIRE(server_transport->connect());
    delay(100);

    // Create server-side Remote (no methods bound)
    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );

    // Create client
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", TEST_PORT);
    FL_REQUIRE(client_transport->connect());
    delay(100);

    // Create client-side Remote
    fl::Remote client_remote(
        [&client_transport]() { return client_transport->readRequest(); },
        [&client_transport](const fl::Json& r) { client_transport->writeResponse(r); }
    );

    // Send request for non-existent method
    Json params = Json::array();
    Json request = createRequest("nonexistent", params, 1);

    client_transport->writeResponse(request);

    // Wait for error response
    bool got_error = false;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 2000) {
        uint32_t now = fl::millis();
        server_transport->update(now);
        client_transport->update(now);
        server_remote.update(now);
        client_remote.update(now);

        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            FL_CHECK(resp.value().contains("error"));
            FL_CHECK(resp.value()["error"].contains("code"));
            FL_CHECK_EQ(resp.value()["error"]["code"].as_int().value(), -32601); // Method not found
            got_error = true;
            break;
        }
        delay(10);
    }

    FL_CHECK(got_error);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
    delay(100);
}
