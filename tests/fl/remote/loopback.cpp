// ok standalone
// Real loopback test: server startup + client connection + request + response
// Uses a background thread for server-side accept + transport pumping + RPC,
// while the main thread handles the client side. Results are posted to the
// main thread via atomic flags (standard net-thread-to-main pattern).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

#include "fl/remote/remote.h"
#include "fl/stl/asio/http/stream_client.h"
#include "fl/stl/asio/http/stream_client.cpp.hpp"
#include "fl/stl/asio/http/stream_server.h"
#include "fl/stl/asio/http/stream_server.cpp.hpp"
#include "fl/stl/asio/http/stream_transport.cpp.hpp"
#include "fl/stl/asio/http/connection.cpp.hpp"
#include "fl/stl/asio/http/chunked_encoding.cpp.hpp"
#include "fl/stl/asio/http/http_parser.cpp.hpp"
#include "fl/stl/asio/http/native_client.cpp.hpp"
#include "fl/stl/asio/http/native_server.cpp.hpp"
#include "fl/stl/json.h"
#include "fl/delay.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include "fl/stl/thread.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"

using namespace fl;

// Pick a random ephemeral port seeded with the current time to avoid
// collisions with other test processes or TIME_WAIT sockets.
static uint16_t randomPort() {
    uint32_t seed = static_cast<uint32_t>(fl::millis());
    // Simple LCG — only need a reasonable spread, not crypto quality.
    seed = seed * 1103515245u + 12345u;
    // Ephemeral range 49152-65535 (IANA dynamic/private ports)
    return static_cast<uint16_t>(49152 + (seed % (65535 - 49152 + 1)));
}

FL_TEST_CASE("Loopback: connect and sync RPC round-trip") {
    // Try random ports seeded with current time until one binds.
    uint16_t PORT = 0;
    fl::shared_ptr<HttpStreamServer> server_transport;
    for (int attempt = 0; attempt < 20; ++attempt) {
        uint16_t candidate = randomPort();
        auto st = fl::make_shared<HttpStreamServer>(candidate);
        if (st->connect()) {
            PORT = st->port();
            server_transport = st;
            break;
        }
    }
    FL_REQUIRE(server_transport);
    FL_REQUIRE(PORT != 0);

    fl::Remote server_remote(
        [&server_transport]() { return server_transport->readRequest(); },
        [&server_transport](const fl::json& r) { server_transport->writeResponse(r); }
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
    json params = json::array();
    params.push_back(json(5));
    params.push_back(json(7));
    json request = json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", "add");
    request.set("params", params);
    request.set("id", 1);
    client_transport->writeResponse(request);

    // Wait for response — the server thread handles accept + update + RPC,
    // so we only need to pump the client transport on the main thread.
    bool got_response = false;
    json response;
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
