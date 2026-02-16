// ok standalone
// Integration tests for Remote with HttpStreamTransport

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

using namespace fl;

// Test: Remote with HTTP client transport - construction
FL_TEST_CASE("Remote-HTTP - Construct with client transport") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10001);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    FL_CHECK_EQ(0, remote.count());
    FL_CHECK_FALSE(transport->isConnected());
}

// Test: Remote with HTTP server transport - construction
FL_TEST_CASE("Remote-HTTP - Construct with server transport") {
    auto transport = fl::make_shared<HttpStreamServer>(10002);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    FL_CHECK_EQ(0, remote.count());
    FL_CHECK_FALSE(transport->isConnected());
}

// Test: Register methods
FL_TEST_CASE("Remote-HTTP - Register methods") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10003);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bind("test", []() -> int { return 42; });

    FL_CHECK_EQ(1, remote.count());
    FL_CHECK(remote.has("test"));
}

// Test: Update loop
FL_TEST_CASE("Remote-HTTP - Update loop") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10004);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bind("test", []() -> int { return 42; });

    // Call both update methods
    remote.update(1000);
    transport->update(1000);

    FL_CHECK_EQ(1, remote.count());
    FL_CHECK_FALSE(transport->isConnected());
}

// Test: Bind async method
FL_TEST_CASE("Remote-HTTP - Bind async method") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10005);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bindAsync("asyncTest", [](ResponseSend& send, const Json& params) {
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);
        send.send(params);
    }, RpcMode::ASYNC);

    FL_CHECK_EQ(1, remote.count());
    FL_CHECK(remote.has("asyncTest"));
}

// Test: Bind streaming method
FL_TEST_CASE("Remote-HTTP - Bind streaming method") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10006);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bindAsync("streamTest", [](ResponseSend& send, const Json& params) {
        Json ack = Json::object();
        ack.set("ack", true);
        send.send(ack);

        for (int i = 0; i < 3; i++) {
            Json update = Json::object();
            update.set("value", i);
            send.sendUpdate(update);
        }

        Json final = Json::object();
        final.set("done", true);
        send.sendFinal(final);
    }, RpcMode::ASYNC_STREAM);

    FL_CHECK_EQ(1, remote.count());
    FL_CHECK(remote.has("streamTest"));
}

// Test: Schema query
FL_TEST_CASE("Remote-HTTP - Schema query") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10007);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bind("add", [](int a, int b) -> int { return a + b; });
    remote.bind("echo", [](const Json& value) -> Json { return value; });

    Json schema = remote.schema();
    FL_CHECK(schema.contains("schema"));
    FL_CHECK(schema["schema"].is_array());
    FL_CHECK_EQ(2, schema["schema"].size());
}

// Test: Unbind methods
FL_TEST_CASE("Remote-HTTP - Unbind methods") {
    auto transport = fl::make_shared<HttpStreamClient>("localhost", 10008);
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const Json& r) { transport->writeResponse(r); }
    );

    remote.bind("test1", []() -> int { return 1; });
    remote.bind("test2", []() -> int { return 2; });
    FL_CHECK_EQ(2, remote.count());

    bool removed = remote.unbind("test1");
    FL_CHECK(removed);
    FL_CHECK_EQ(1, remote.count());
    FL_CHECK_FALSE(remote.has("test1"));
    FL_CHECK(remote.has("test2"));
}

// Test: Backward compatibility - callback-based construction
FL_TEST_CASE("Remote-HTTP - Backward compatibility") {
    fl::Remote remote(
        []() -> fl::optional<fl::Json> { return fl::nullopt; },
        [](const fl::Json&) {}
    );

    FL_CHECK_EQ(0, remote.count());
}
