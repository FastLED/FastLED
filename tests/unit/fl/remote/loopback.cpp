
// Real loopback test: server startup + client connection + request + response
// Uses a background thread for server-side accept + transport pumping + RPC,
// while the main thread handles the client side. Results are posted to the
// main thread via atomic flags (standard net-thread-to-main pattern).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

#include "fl/remote/remote.h"
#include "fl/net/http/stream_client.h"
#include "fl/net/http/stream_server.h"
#include "fl/stl/json.h"
#include "fl/system/delay.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/chrono.h"
#include "fl/stl/thread.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::net::http;

FL_TEST_CASE("Loopback: connect and sync RPC round-trip") {
    // Use port 0 to let the OS assign an available ephemeral port.
    // This eliminates port collisions entirely.
    auto server_transport = fl::make_shared<HttpStreamServer>(0);
    FL_REQUIRE(server_transport->connect());
    uint16_t PORT = server_transport->port();
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
            fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for - inside thread
        }
    });

    // Client — connect() blocks until the server accepts, but the server
    // thread is already running acceptClients() so this won't deadlock.
    // Use 127.0.0.1 directly instead of "localhost" to avoid DNS resolution
    // overhead and IPv6 ambiguity on Windows (server binds to INADDR_LOOPBACK).
    auto client_transport = fl::make_shared<HttpStreamClient>("127.0.0.1", PORT);

    {
        bool connected = false;
        uint32_t connect_start = fl::millis();
        while (fl::millis() - connect_start < 10000) {
            if (client_transport->connect()) {
                connected = true;
                break;
            }
            fl::delay(10);
        }
        FL_REQUIRE(connected);
    }

    FL_REQUIRE(client_transport->isConnected());

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

        // Yield enough CPU time for the server thread to process under load
        fl::delay(5);
    }

    // Stop server thread
    server_running.store(false);
    server_thread.join();

    FL_REQUIRE(got_response);

    // Verify the response is a valid JSON-RPC response
    FL_REQUIRE(response.is_object());
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response.contains("id"));
    FL_REQUIRE(!response.contains("error"));

    auto result_int = response["result"].as_int();
    auto id_int = response["id"].as_int();
    FL_REQUIRE(result_int.has_value());
    FL_REQUIRE(id_int.has_value());
    FL_REQUIRE_EQ(result_int.value(), 12); // 5 + 7
    FL_REQUIRE_EQ(id_int.value(), 1);

    // Cleanup
    client_transport->disconnect();
    server_transport->disconnect();
}

} // FL_TEST_FILE
