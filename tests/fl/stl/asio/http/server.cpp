// Basic compilation test for HTTP server

#define FASTLED_STUB_IMPL

#include "fl/stl/asio/http/server.h"
#include "fl/log/log.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("HTTP server registers routes") {
#ifdef FASTLED_HAS_NETWORKING
    fl::HttpServer server;

    // Test route registration
    server.get("/test", [](const fl::http_request& req) {
        return fl::http_response::ok("test response");
    });

    server.post("/data", [](const fl::http_request& req) {
        fl::json response = fl::json::object();
        response.set("status", "ok");
        return fl::http_response::ok().json(response);
    });

#else
    FL_WARN("Networking not available - test skipped");
#endif
}

} // FL_TEST_FILE
