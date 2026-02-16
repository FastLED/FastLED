// Basic compilation test for HTTP server

#define FASTLED_STUB_IMPL

#include "fl/net/http/server.h"
#include "fl/warn.h"

int main() {
#ifdef FASTLED_HAS_NETWORKING
    fl::HttpServer server;

    // Test route registration
    server.get("/test", [](const fl::http_request& req) {
        return fl::http_response::ok("test response");
    });

    server.post("/data", [](const fl::http_request& req) {
        fl::Json response = fl::Json::object();
        response.set("status", "ok");
        return fl::http_response::ok().json(response);
    });

    FL_WARN("HTTP server API test passed");
#else
    FL_WARN("Networking not available - test skipped");
#endif

    return 0;
}
