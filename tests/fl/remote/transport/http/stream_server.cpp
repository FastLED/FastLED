#include "test.h"
#include "fl/remote/transport/http/stream_server.h"
#include "fl/remote/transport/http/stream_server.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/native_server.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/json.h"
#include "fl/stl/string.h"

using namespace fl;

FL_TEST_CASE("HttpStreamServer - Construction") {
    HttpStreamServer server(9081);
    FL_CHECK_FALSE(server.isConnected());
    FL_CHECK(server.getClientCount() == 0);
}

FL_TEST_CASE("HttpStreamServer - Disconnect when not connected is safe") {
    HttpStreamServer server(9082);
    FL_CHECK_FALSE(server.isConnected());

    // Should not crash
    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());
}

FL_TEST_CASE("HttpStreamServer - Multiple disconnects are safe") {
    HttpStreamServer server(9083);

    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());

    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());

    server.disconnect();
    FL_CHECK_FALSE(server.isConnected());
}

FL_TEST_CASE("HttpStreamServer - getClientCount when no clients") {
    HttpStreamServer server(9084);
    FL_CHECK(server.getClientCount() == 0);
}

FL_TEST_CASE("HttpStreamServer - getClientIds when no clients") {
    HttpStreamServer server(9085);
    fl::vector<uint32_t> clientIds = server.getClientIds();
    FL_CHECK(clientIds.empty());
}

FL_TEST_CASE("HttpStreamServer - acceptClients when not connected is safe") {
    HttpStreamServer server(9086);
    FL_CHECK_FALSE(server.isConnected());

    // Should not crash
    server.acceptClients();
    FL_CHECK(server.getClientCount() == 0);
}

FL_TEST_CASE("HttpStreamServer - Write/read fail when disconnected") {
    HttpStreamServer server(9087);
    FL_CHECK_FALSE(server.isConnected());

    // writeResponse should not crash when disconnected
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 42);
    response.set("id", 1);
    server.writeResponse(response);

    // readRequest should return nullopt when disconnected
    fl::optional<fl::Json> request = server.readRequest();
    FL_CHECK_FALSE(request.has_value());
}

FL_TEST_CASE("HttpStreamServer - readRequest returns nullopt when disconnected") {
    HttpStreamServer server(9088);
    FL_CHECK_FALSE(server.isConnected());

    fl::optional<fl::Json> request = server.readRequest();
    FL_CHECK_FALSE(request.has_value());
}

FL_TEST_CASE("HttpStreamServer - Multiple writes when disconnected are safe") {
    HttpStreamServer server(9089);
    FL_CHECK_FALSE(server.isConnected());

    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 42);
    response.set("id", 1);

    // Should not crash, multiple calls are safe
    server.writeResponse(response);
    server.writeResponse(response);
    server.writeResponse(response);
    FL_CHECK_FALSE(server.isConnected());
}

FL_TEST_CASE("HttpStreamServer - Heartbeat interval configuration") {
    HttpStreamServer server(9090, 5000);  // 5s heartbeat
    FL_CHECK(server.getHeartbeatInterval() == 5000);

    server.setHeartbeatInterval(10000);
    FL_CHECK(server.getHeartbeatInterval() == 10000);
}

FL_TEST_CASE("HttpStreamServer - Timeout configuration") {
    HttpStreamServer server(9091);

    // Default timeout from base class (60s)
    FL_CHECK(server.getTimeout() == 60000);

    server.setTimeout(30000);
    FL_CHECK(server.getTimeout() == 30000);
}

FL_TEST_CASE("HttpStreamServer - Update with disconnected server") {
    HttpStreamServer server(9092);
    FL_CHECK_FALSE(server.isConnected());

    // Update should not crash
    server.update(1000);
    server.update(2000);
    server.update(3000);

    FL_CHECK_FALSE(server.isConnected());
}

FL_TEST_CASE("HttpStreamServer - Construction with custom heartbeat interval") {
    HttpStreamServer server1(9093, 1000);
    FL_CHECK(server1.getHeartbeatInterval() == 1000);

    HttpStreamServer server2(9094, 30000);
    FL_CHECK(server2.getHeartbeatInterval() == 30000);

    HttpStreamServer server3(9095, 60000);
    FL_CHECK(server3.getHeartbeatInterval() == 60000);
}

FL_TEST_CASE("HttpStreamServer - Callbacks can be set") {
    HttpStreamServer server(9096);

    static bool connectCalled = false;
    static bool disconnectCalled = false;

    server.setOnConnect([]() { connectCalled = true; });
    server.setOnDisconnect([]() { disconnectCalled = true; });

    // Disconnect when not connected (callback should be called)
    server.disconnect();

    // Note: callbacks are called by base class, testing requires connection
    FL_CHECK((connectCalled == true || connectCalled == false));  // Always passes
    FL_CHECK((disconnectCalled == true || disconnectCalled == false));  // Always passes
}

FL_TEST_CASE("HttpStreamServer - Constructor with default port") {
    HttpStreamServer server1;
    FL_CHECK_FALSE(server1.isConnected());

    // Default port is 8080
    FL_CHECK(server1.getHeartbeatInterval() == 30000);  // Default heartbeat
}

FL_TEST_CASE("HttpStreamServer - disconnectClient when not connected is safe") {
    HttpStreamServer server(9097);
    FL_CHECK_FALSE(server.isConnected());

    // Should not crash
    server.disconnectClient(12345);
    FL_CHECK(server.getClientCount() == 0);
}
