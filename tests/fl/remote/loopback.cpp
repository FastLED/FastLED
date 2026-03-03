// ok standalone
// Real loopback test: server startup + client connection + request + response
// Uses a background thread for server-side accept + transport pumping + RPC,
// while the main thread handles the client side. Results are posted to the
// main thread via atomic flags (standard net-thread-to-main pattern).

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
#include "fl/delay.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include "fl/stl/thread.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"

using namespace fl;

// Try binding on several ports to avoid TIME_WAIT collisions from parallel tests.
static uint16_t findOpenPort(fl::shared_ptr<HttpStreamServer>& server) {
    // Spread across a wide range to minimize collision probability.
    static constexpr uint16_t kPorts[] = {59901, 59911, 59921, 59931, 59941,
                                          59951, 59961, 59971, 59981, 59991};
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

    // Background thread: handles acceptClients + server transport update + RPC.
    // This is the standard net-thread pattern — the server side runs entirely
    // in a dedicated thread, and results (the RPC response) flow back through
    // the TCP connection to the client on the main thread.
    fl::atomic<bool> server_running{true};
    fl::thread server_thread([&]() {
        while (server_running.load()) {
            uint32_t now = fl::millis();
            server_transport->acceptClients();
            server_transport->update(now);
            server_remote.update(now);
            fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for
        }
    });

    // Client — connect() blocks until the server accepts, but the server
    // thread is already running acceptClients() so this won't deadlock.
    auto client_transport = fl::make_shared<HttpStreamClient>("localhost", PORT);

    {
        bool connected = false;
        uint32_t connect_start = fl::millis();
        while (fl::millis() - connect_start < 15000) {
            if (client_transport->connect()) {
                connected = true;
                break;
            }
            fl::this_thread::sleep_for(fl::chrono::milliseconds(10));  // ok sleep for
        }
        FL_REQUIRE(connected);
    }

    FL_CHECK(client_transport->isConnected());

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

    // Wait for response — the server thread handles accept + update + RPC,
    // so we only need to pump the client transport on the main thread.
    bool got_response = false;
    Json response;
    uint32_t start = fl::millis();

    while (fl::millis() - start < 10000) {
        uint32_t now = fl::millis();
        client_transport->update(now);

        auto resp = client_transport->readRequest();
        if (resp.has_value()) {
            response = resp.value();
            got_response = true;
            break;
        }

        fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for
    }

    // Stop server thread
    server_running.store(false);
    server_thread.join();

    FL_REQUIRE(got_response);
    FL_CHECK_EQ(response["result"].as_int().value(), 12); // 5 + 7
    FL_CHECK_EQ(response["id"].as_int().value(), 1);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
}
