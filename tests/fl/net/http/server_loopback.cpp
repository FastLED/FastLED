// ok standalone
/// Phase 3: Internal Loopback Test
/// Tests HTTP server by making client requests from within the test

#define FASTLED_STUB_IMPL
#define FASTLED_HAS_NETWORKING

#include "fl/net/http/server.h"
#include "fl/net/fetch.h"
#include "fl/warn.h"
#include "fl/async.h"

int main() {
    FL_WARN("Phase 3: Internal Loopback Test");
    FL_WARN("=================================");

    // Create HTTP server
    fl::HttpServer server;

    // Register routes
    server.get("/", [](const fl::http_request& req) {
        return fl::http_response::ok("Hello from loopback!\n");
    });

    server.get("/ping", [](const fl::http_request& req) {
        return fl::http_response::ok("pong\n");
    });

    server.get("/test", [](const fl::http_request& req) {
        fl::Json data = fl::Json::object();
        data.set("test", true);
        data.set("value", 42);
        return fl::http_response::ok().json(data);
    });

    // Start server
    if (!server.start(8080)) {
        FL_WARN("✗ FAILED: Could not start server");
        FL_WARN("Error: " << server.last_error());
        return 1;
    }

    FL_WARN("✓ Server started on port 8080");

    // Give server a moment to bind
    fl::delay(100);

    int passed = 0;
    int failed = 0;

    // Test 1: GET /
    {
        FL_WARN("\nTest 1: GET /");
        server.update();  // Process any pending

        fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8080/");
        fl::result<fl::response> result = fl::await_top_level(promise);

        if (!result.ok()) {
            FL_WARN("  ✗ FAILED: " << result.error_message());
            failed++;
        } else {
            const fl::response& response = result.value();
            if (response.status() != 200) {
                FL_WARN("  ✗ FAILED: Status code " << response.status());
                failed++;
            } else if (response.text() != "Hello from loopback!\n") {
                FL_WARN("  ✗ FAILED: Unexpected response: " << response.text());
                failed++;
            } else {
                FL_WARN("  ✓ PASSED");
                passed++;
            }
        }

        server.update();  // Process request
    }

    // Test 2: GET /ping
    {
        FL_WARN("\nTest 2: GET /ping");
        server.update();

        fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8080/ping");
        fl::result<fl::response> result = fl::await_top_level(promise);

        if (!result.ok()) {
            FL_WARN("  ✗ FAILED: " << result.error_message());
            failed++;
        } else {
            const fl::response& response = result.value();
            if (response.status() != 200) {
                FL_WARN("  ✗ FAILED: Status code " << response.status());
                failed++;
            } else if (response.text() != "pong\n") {
                FL_WARN("  ✗ FAILED: Unexpected response: " << response.text());
                failed++;
            } else {
                FL_WARN("  ✓ PASSED");
                passed++;
            }
        }

        server.update();
    }

    // Test 3: GET /test (JSON response)
    {
        FL_WARN("\nTest 3: GET /test (JSON)");
        server.update();

        fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8080/test");
        fl::result<fl::response> result = fl::await_top_level(promise);

        if (!result.ok()) {
            FL_WARN("  ✗ FAILED: " << result.error_message());
            failed++;
        } else {
            const fl::response& response = result.value();
            if (response.status() != 200) {
                FL_WARN("  ✗ FAILED: Status code " << response.status());
                failed++;
            } else if (!response.is_json()) {
                FL_WARN("  ✗ FAILED: Response is not JSON");
                failed++;
            } else {
                fl::Json data = response.json();
                bool test_val = data["test"] | false;
                int value_val = data["value"] | 0;
                if (!test_val || value_val != 42) {
                    FL_WARN("  ✗ FAILED: Invalid JSON response (test=" << test_val << ", value=" << value_val << ")");
                    failed++;
                } else {
                    FL_WARN("  ✓ PASSED");
                    passed++;
                }
            }
        }

        server.update();
    }

    // Cleanup
    server.stop();

    // Results
    FL_WARN("\n=================================");
    FL_WARN("Results: " << passed << "/" << (passed + failed) << " passed");
    FL_WARN("=================================");

    if (failed > 0) {
        FL_WARN("✗ Phase 3 FAILED");
        return 1;
    }

    FL_WARN("✓ Phase 3 PASSED - All loopback tests successful");
    return 0;
}
