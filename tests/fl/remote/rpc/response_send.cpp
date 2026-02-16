#include "fl/remote/rpc/response_send.h"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include "test.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

FL_TEST_CASE("ResponseSend: send() creates proper JSON-RPC response") {
    vector<Json> sentResponses;
    auto sink = [&sentResponses](const Json& response) {
        sentResponses.push_back(response);
    };

    Json requestId = Json(42);
    ResponseSend responseSend(requestId, sink);

    // Send a simple result
    Json result = Json::object();
    result.set("value", 100);
    responseSend.send(result);

    FL_REQUIRE(sentResponses.size() == 1);

    const Json& response = sentResponses[0];
    FL_REQUIRE(response.contains("jsonrpc"));
    FL_REQUIRE(response["jsonrpc"].as_string().value() == "2.0");
    FL_REQUIRE(response.contains("id"));
    FL_REQUIRE(response["id"].as_int().value() == 42);
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].contains("value"));
    FL_REQUIRE(response["result"]["value"].as_int().value() == 100);
}

FL_TEST_CASE("ResponseSend: sendUpdate() creates update response") {
    vector<Json> sentResponses;
    auto sink = [&sentResponses](const Json& response) {
        sentResponses.push_back(response);
    };

    Json requestId = Json("test-id");
    ResponseSend responseSend(requestId, sink);

    // Send an update
    Json update = Json(50);
    responseSend.sendUpdate(update);

    FL_REQUIRE(sentResponses.size() == 1);

    const Json& response = sentResponses[0];
    FL_REQUIRE(response.contains("jsonrpc"));
    FL_REQUIRE(response["jsonrpc"].as_string().value() == "2.0");
    FL_REQUIRE(response.contains("id"));
    FL_REQUIRE(response["id"].as_string().value() == "test-id");
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].contains("update"));
    FL_REQUIRE(response["result"]["update"].as_int().value() == 50);
}

FL_TEST_CASE("ResponseSend: sendFinal() creates final response with stop marker") {
    vector<Json> sentResponses;
    auto sink = [&sentResponses](const Json& response) {
        sentResponses.push_back(response);
    };

    Json requestId = Json(99);
    ResponseSend responseSend(requestId, sink);

    // Send final result
    Json finalResult = Json("done");
    responseSend.sendFinal(finalResult);

    FL_REQUIRE(sentResponses.size() == 1);
    FL_REQUIRE(responseSend.isFinal());

    const Json& response = sentResponses[0];
    FL_REQUIRE(response.contains("jsonrpc"));
    FL_REQUIRE(response["jsonrpc"].as_string().value() == "2.0");
    FL_REQUIRE(response.contains("id"));
    FL_REQUIRE(response["id"].as_int().value() == 99);
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].contains("value"));
    FL_REQUIRE(response["result"]["value"].as_string().value() == "done");
    FL_REQUIRE(response["result"].contains("stop"));
    FL_REQUIRE(response["result"]["stop"].as_bool().value() == true);
}

FL_TEST_CASE("ResponseSend: multiple sendUpdate() calls work") {
    vector<Json> sentResponses;
    auto sink = [&sentResponses](const Json& response) {
        sentResponses.push_back(response);
    };

    Json requestId = Json(123);
    ResponseSend responseSend(requestId, sink);

    // Send multiple updates
    for (int i = 0; i < 3; i++) {
        responseSend.sendUpdate(Json(i * 10));
    }

    FL_REQUIRE(sentResponses.size() == 3);

    // Verify each update
    for (int i = 0; i < 3; i++) {
        const Json& response = sentResponses[i];
        FL_REQUIRE(response["result"]["update"].as_int().value() == i * 10);
    }
}

FL_TEST_CASE("ResponseSend: after sendFinal() no more responses sent") {
    vector<Json> sentResponses;
    auto sink = [&sentResponses](const Json& response) {
        sentResponses.push_back(response);
    };

    Json requestId = Json(456);
    ResponseSend responseSend(requestId, sink);

    // Send final
    responseSend.sendFinal(Json("final"));
    FL_REQUIRE(sentResponses.size() == 1);

    // Try to send more (should be ignored)
    responseSend.send(Json("ignored"));
    responseSend.sendUpdate(Json("also-ignored"));
    responseSend.sendFinal(Json("still-ignored"));

    // Should still be only 1 response
    FL_REQUIRE(sentResponses.size() == 1);
}

FL_TEST_CASE("ResponseSend: requestId() returns correct ID") {
    auto sink = [](const Json&) {};

    Json requestId = Json("my-id");
    ResponseSend responseSend(requestId, sink);

    FL_REQUIRE(responseSend.requestId().as_string().value() == "my-id");
}

#endif // FASTLED_ENABLE_JSON
