// ok standalone
// Real loopback test: server startup + client connection + request + response
// Uses real sockets on port 47901 (only test that does so - no port conflicts)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

#include "fl/remote/remote.h"
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
#include "fl/remote/transport/http/test_utils/server_thread.h"
#include "fl/remote/transport/http/test_utils/server_thread.cpp.hpp"
#include "fl/json.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include <thread>  // ok include
#include <chrono>  // ok include

using namespace fl;

__attribute__((unused)) static void delay(uint32_t ms) {
    fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));
}

FL_TEST_CASE("Loopback: connect and sync RPC round-trip") {
    constexpr uint16_t PORT = 47901;

    // Server
    auto server_transport = fl::make_shared<HttpStreamServer>(PORT);
    FL_REQUIRE(server_transport->connect());

    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );
    server_remote.bind("add", [](int a, int b) -> int { return a + b; });

    ServerThread server_thread(server_transport);
    FL_REQUIRE(server_thread.start());
    delay(200);

    // Client
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", PORT);
    FL_REQUIRE(client_transport->connect());
    FL_CHECK(client_transport->isConnected());
    delay(100);

    // Send request
    Json params = Json::array();
    params.push_back(Json(5));
    params.push_back(Json(7));
    Json request = Json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "add");
    request.set("params", params);
    request.set("id", 1);
    client_transport->writeResponse(request);

    // Wait for response
    bool got_response = false;
    Json response;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 2000) {
        uint32_t now = fl::millis();
        server_transport->update(now);
        client_transport->update(now);

        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            response = resp.value();
            got_response = true;
            break;
        }

        server_remote.update(now);
        delay(10);
    }

    FL_REQUIRE(got_response);
    FL_CHECK_EQ(response["result"].as_int().value(), 12); // 5 + 7
    FL_CHECK_EQ(response["id"].as_int().value(), 1);

    // Cleanup
    client_transport->disconnect();
    server_thread.stop();
    server_transport->disconnect();
}
