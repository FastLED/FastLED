// Tests for promise-based rpc()/rpcStream() API on HttpStreamTransport
// Uses MockHttpServer + MockHttpClient + Remote (same pattern as rpc_http_stream.cpp)

#include "test.h"
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/json.h"
#include "fl/promise.h"
#include "fl/stl/vector.h"
#include "fl/net/http/test_utils/mock_http_server.h"
#include "fl/net/http/test_utils/mock_http_client.h"
#include "fl/net/http/stream_transport.cpp.hpp"
#include "fl/net/http/connection.cpp.hpp"
#include "fl/net/http/chunked_encoding.cpp.hpp"

using namespace fl;

//=============================================================================
// TEST CASE: SYNC rpc() resolves with result
//=============================================================================

FL_TEST_CASE("Promise - SYNC rpc() resolves") {
    MockHttpServer server(48001);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("add", [](int a, int b) -> int {
        return a + b;
    });

    MockHttpClient client(server);
    client.connect();

    // Use rpc() API
    Json params = Json::array();
    params.push_back(Json(5));
    params.push_back(Json(7));
    fl::promise<Json> p = client.rpc("add", params);

    FL_CHECK(p.valid());
    FL_CHECK(!p.is_completed());

    // Pump: server reads from client, processes, sends response back
    server.update(0);
    remoteServer.update(0);

    // Client update drains chunks and resolves promise
    client.update(0);

    FL_REQUIRE(p.is_resolved());
    const Json& response = p.value();
    FL_CHECK(response.contains("result"));
    FL_CHECK_EQ(response["result"].as_int().value(), 12);
}

//=============================================================================
// TEST CASE: ASYNC rpc() skips ACK, resolves with final result
//=============================================================================

FL_TEST_CASE("Promise - ASYNC rpc() skips ACK") {
    MockHttpServer server(48002);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bindAsync("longTask", [](ResponseSend& send, const Json& params) {
        Json result = Json::object();
        result.set("value", 42);
        send.send(result);
    }, RpcMode::ASYNC);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    Json params = Json::array();
    fl::promise<Json> p = client.rpc("longTask", params);

    FL_CHECK(!p.is_completed());

    // Pump everything
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    FL_REQUIRE(p.is_resolved());
    const Json& response = p.value();
    FL_CHECK(response.contains("result"));
    // The final result should have "value": 42, not be the ACK
    FL_CHECK(response["result"].contains("value"));
    FL_CHECK_EQ(response["result"]["value"].as_int().value(), 42);
}

//=============================================================================
// TEST CASE: ASYNC_STREAM rpcStream() with updates
//=============================================================================

FL_TEST_CASE("Promise - ASYNC_STREAM rpcStream() updates and final") {
    MockHttpServer server(48003);
    server.connect();
    server.update(0);

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bindAsync("stream", [](ResponseSend& send, const Json& params) {
        for (int i = 0; i < 3; i++) {
            Json update = Json::object();
            update.set("progress", i * 33);
            send.sendUpdate(update);
        }
        Json finalVal = Json::object();
        finalVal.set("done", true);
        send.sendFinal(finalVal);
    }, RpcMode::ASYNC_STREAM);

    MockHttpClient client(server);
    client.connect();
    client.update(0);

    Json params = Json::array();
    fl::vector<Json> updates;
    bool finalResolved = false;
    Json finalValue;

    StreamHandle handle = client.rpcStream("stream", params);
    handle.onData([&updates](const Json& update) {
        updates.push_back(update);
    });
    handle.then([&finalResolved, &finalValue](const Json& val) {
        finalResolved = true;
        finalValue = val;
    });

    FL_CHECK(handle.valid());

    // Pump
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    // Should have received 3 updates
    FL_CHECK_EQ(updates.size(), 3u);
    FL_CHECK_EQ(updates[0]["progress"].as_int().value(), 0);
    FL_CHECK_EQ(updates[1]["progress"].as_int().value(), 33);
    FL_CHECK_EQ(updates[2]["progress"].as_int().value(), 66);

    // Final should be resolved
    FL_CHECK(finalResolved);
    FL_CHECK(finalValue.contains("done"));
    FL_CHECK_EQ(finalValue["done"].as_bool().value(), true);
}

//=============================================================================
// TEST CASE: Error rejects promise
//=============================================================================

FL_TEST_CASE("Promise - Error rejects promise") {
    MockHttpServer server(48004);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    MockHttpClient client(server);
    client.connect();

    Json params = Json::array();
    bool errorCaught = false;
    fl::string errorMsg;

    fl::promise<Json> p = client.rpc("nonexistent", params);
    p.catch_([&errorCaught, &errorMsg](const fl::Error& err) {
        errorCaught = true;
        errorMsg = err.message;
    });

    // Pump
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    FL_CHECK(p.is_rejected());
    FL_CHECK(errorCaught);
    FL_CHECK(!errorMsg.empty());
}

//=============================================================================
// TEST CASE: readRequest() backward compatibility
//=============================================================================

FL_TEST_CASE("Promise - readRequest() backward compat with rpc()") {
    MockHttpServer server(48005);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("add", [](int a, int b) -> int {
        return a + b;
    });

    remoteServer.bind("multiply", [](int a, int b) -> int {
        return a * b;
    });

    MockHttpClient client(server);
    client.connect();

    // Use rpc() for "add"
    Json addParams = Json::array();
    addParams.push_back(Json(3));
    addParams.push_back(Json(4));
    fl::promise<Json> addPromise = client.rpc("add", addParams);

    // Also send a manual request for "multiply" via writeResponse (old API)
    Json mulReq = Json::object();
    mulReq.set("jsonrpc", "2.0");
    mulReq.set("method", "multiply");
    Json mulParams = Json::array();
    mulParams.push_back(Json(5));
    mulParams.push_back(Json(6));
    mulReq.set("params", mulParams);
    mulReq.set("id", "manual-1");
    client.writeResponse(mulReq);

    // Pump
    server.update(0);
    remoteServer.update(0);
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    // The "add" response should be consumed by the promise
    FL_CHECK(addPromise.is_resolved());
    FL_CHECK_EQ(addPromise.value()["result"].as_int().value(), 7);

    // The "multiply" response should be available via readRequest()
    fl::optional<Json> mulResponse = client.readRequest();
    FL_REQUIRE(mulResponse.has_value());
    FL_CHECK(mulResponse->contains("result"));
    FL_CHECK_EQ(mulResponse->operator[]("result").as_int().value(), 30);
}

//=============================================================================
// TEST CASE: Multiple concurrent calls
//=============================================================================

FL_TEST_CASE("Promise - Multiple concurrent calls resolve correctly") {
    MockHttpServer server(48006);
    server.connect();

    Remote remoteServer(
        [&server]() { return server.readRequest(); },
        [&server](const Json& r) { server.writeResponse(r); }
    );

    remoteServer.bind("double", [](int x) -> int {
        return x * 2;
    });

    MockHttpClient client(server);
    client.connect();

    // Fire 3 concurrent calls
    Json p1 = Json::array();
    p1.push_back(Json(10));
    fl::promise<Json> call1 = client.rpc("double", p1);

    Json p2 = Json::array();
    p2.push_back(Json(20));
    fl::promise<Json> call2 = client.rpc("double", p2);

    Json p3 = Json::array();
    p3.push_back(Json(30));
    fl::promise<Json> call3 = client.rpc("double", p3);

    FL_CHECK(!call1.is_completed());
    FL_CHECK(!call2.is_completed());
    FL_CHECK(!call3.is_completed());

    // Pump: server processes all requests, client receives all responses
    server.update(0);
    remoteServer.update(0);
    server.update(0);
    remoteServer.update(0);
    server.update(0);
    remoteServer.update(0);
    client.update(0);

    FL_REQUIRE(call1.is_resolved());
    FL_REQUIRE(call2.is_resolved());
    FL_REQUIRE(call3.is_resolved());

    FL_CHECK_EQ(call1.value()["result"].as_int().value(), 20);
    FL_CHECK_EQ(call2.value()["result"].as_int().value(), 40);
    FL_CHECK_EQ(call3.value()["result"].as_int().value(), 60);
}
