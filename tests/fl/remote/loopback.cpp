// ok standalone
// Real loopback test: server startup + client connection + request + response
// Uses a background accept thread for the blocking connect() phase, then
// single-threaded polling for everything else (acceptClients + update + RPC).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

#include "fl/remote/remote.h"
#include "fl/net/http/stream_client.h"
#include "fl/net/http/stream_client.cpp.hpp"
#include "fl/net/http/stream_server.h"
#include "fl/net/http/stream_server.cpp.hpp"
#include "fl/net/http/stream_transport.cpp.hpp"
#include "fl/net/http/connection.cpp.hpp"
#include "fl/net/http/chunked_encoding.cpp.hpp"
#include "fl/net/http/http_parser.cpp.hpp"
#include "fl/net/http/native_client.cpp.hpp"
#include "fl/net/http/native_server.cpp.hpp"
#include "fl/json.h"
#include "fl/task.h"
#include "fl/delay.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include "fl/stl/thread.h"
#include "fl/delay.h"

using namespace fl;

// Try binding on several ports to avoid TIME_WAIT collisions from parallel tests.
static uint16_t findOpenPort(fl::shared_ptr<HttpStreamServer>& server) {
    // Spread across a wide range to minimize collision probability.
    static constexpr uint16_t kPorts[] = {59901, 59911, 59921, 59931, 59941};
    for (uint16_t port : kPorts) {
        server = fl::make_shared<HttpStreamServer>(port);
        if (server->connect()) {
            return port;
        }
    }
    return 0;
}


FL_TEST_CASE("Loopback: connect and sync RPC round-trip") {
    fl::shared_ptr<HttpStreamServer> server_transport;
    uint16_t PORT = findOpenPort(server_transport);
    FL_REQUIRE(PORT != 0);

    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::Json& r) { server_transport->writeResponse(r); }
    );
    server_remote.bind("add", [](int a, int b) -> int { return a + b; });

    // Client — connect() blocks, so run acceptClients() in a background thread
    // during the connect phase only.
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", PORT);

    fl::atomic<bool> accepting{true};
    fl::thread accept_thread([&server_transport, &accepting]() {
        while (accepting.load()) {
            server_transport->acceptClients();
            fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for
        }
    });

    {
        bool connected = false;
        uint32_t connect_start = fl::millis();
        while (fl::millis() - connect_start < 5000) {
            if (client_transport->connect()) {
                connected = true;
                break;
            }
            fl::delay(10);
        }
        FL_REQUIRE(connected);
    }

    // Stop the accept thread now that TCP connection is established.
    accepting.store(false);
    accept_thread.join();

    FL_CHECK(client_transport->isConnected());

    // Register a task to pump transports and RPC via fl::Scheduler.
    // fl::delay() automatically calls async_yield() → Scheduler::update(),
    // so this task runs every 1ms during the delay loop below.
    auto pump_task = fl::task::every_ms(1).then([&]() {
        uint32_t now = fl::millis();
        server_transport->update(now);
        client_transport->update(now);
        server_remote.update(now);
    });

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

    // Wait for response — single-threaded: acceptClients() finishes HTTP
    // handshake, update() processes chunked data, Remote handles RPC.
    bool got_response = false;
    Json response;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 10000) {
        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            response = resp.value();
            got_response = true;
            break;
        }

        fl::delay(10);
    }

    pump_task.cancel();

    FL_REQUIRE(got_response);
    FL_CHECK_EQ(response["result"].as_int().value(), 12); // 5 + 7
    FL_CHECK_EQ(response["id"].as_int().value(), 1);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
}
