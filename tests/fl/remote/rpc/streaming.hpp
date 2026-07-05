#include "fl/remote/remote.h"
#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("Rpc: bindStreaming writes a JSON-RPC result envelope") {
    fl::string streamed;
    fl::vector<fl::json> queued;

    Remote remote(
        []() { return fl::optional<fl::json>(); },
        [&queued](const fl::json& response) { queued.push_back(response); },
        [&streamed](fl::JsonStreamCallback writeJson) {
            fl::JsonStreamWriter writer([&streamed](const char* data, fl::size len) {
                streamed.append(data, len);
            });
            writeJson(writer);
            writer.flush();
        });

    remote.bindStreaming("help", [](fl::JsonStreamWriter& writer,
                                    const fl::json& params,
                                    const fl::json& requestId) {
        FL_CHECK(params.is_array());
        FL_CHECK_EQ(requestId.as_int().value_or(0), 77);
        writer.beginObject();
        writer.member("success", true);
        writer.member("totalFunctions", 2);
        writer.key("functions");
        writer.beginArray();
        writer.beginObject();
        writer.member("name", "a");
        writer.endObject();
        writer.beginObject();
        writer.member("name", "b");
        writer.endObject();
        writer.endArray();
        writer.endObject();
    });

    fl::json request = fl::json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "help");
    request.set("params", fl::json::array());
    request.set("id", 77);

    fl::json response = remote.processRpc(request);
    FL_CHECK(response.contains("noEnqueue"));
    FL_CHECK(response["noEnqueue"].as_bool().value_or(false));
    FL_CHECK(queued.empty());

    fl::json parsed = fl::json::parse(streamed);
    FL_CHECK_EQ(parsed["jsonrpc"].as_string().value_or(""), "2.0");
    FL_CHECK_EQ(parsed["id"].as_int().value_or(0), 77);
    FL_CHECK(parsed["result"]["success"].as_bool().value_or(false));
    FL_CHECK_EQ(parsed["result"]["totalFunctions"].as_int().value_or(0), 2);
    FL_CHECK_EQ(parsed["result"]["functions"].size(), 2);
    FL_CHECK_EQ(parsed["result"]["functions"][1]["name"].as_string().value_or(""), "b");
}

FL_TEST_CASE("Rpc: streaming method without stream sink returns an error") {
    Rpc rpc;
    rpc.bindStreaming("large", [](fl::JsonStreamWriter& writer,
                                  const fl::json& params,
                                  const fl::json& requestId) {
        (void)params;
        (void)requestId;
        writer.valueNull();
    });

    fl::json request = fl::json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "large");
    request.set("params", fl::json::array());
    request.set("id", 9);

    fl::json response = rpc.handle(request);
    FL_REQUIRE(response.contains("error"));
    FL_CHECK_EQ(response["error"]["code"].as_int().value_or(0), -32603);
    FL_CHECK_EQ(response["id"].as_int().value_or(0), 9);
}
