#include "fl/remote/remote.h"

#if FASTLED_ENABLE_JSON

#include "test.h"

// =============================================================================
// Test Fixtures and Helpers
// =============================================================================

// Test helper to create JSON-RPC request
fl::Json makeRequest(const char* method, fl::Json params = fl::Json::array(), int id = 1, fl::u32 timestamp = 0) {
    fl::Json req = fl::Json::object();
    req.set("method", method);
    req.set("params", params);
    req.set("id", id);
    if (timestamp > 0) {
        req.set("timestamp", static_cast<fl::i64>(timestamp));
    }
    return req;
}

// Request/Response queues for testing
struct TestIO {
    fl::vector<fl::Json> requests;
    fl::vector<fl::Json> responses;
    size_t requestIndex = 0;

    fl::optional<fl::Json> pullRequest() {
        if (requestIndex >= requests.size()) {
            return fl::nullopt;
        }
        return requests[requestIndex++];
    }

    void pushResponse(const fl::Json& response) {
        responses.push_back(response);
    }

    void reset() {
        requests.clear();
        responses.clear();
        requestIndex = 0;
    }
};

// =============================================================================
// Construction Tests
// =============================================================================

FL_TEST_CASE("Remote: Construction with callbacks") {
    TestIO io;

    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    FL_REQUIRE(remote.count() == 0);  // No methods registered yet
}

// =============================================================================
// Method Registration Tests
// =============================================================================

FL_TEST_CASE("Remote: Bind method - void return") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    int called = 0;
    remote.bind("test", [&called]() { called++; });

    FL_REQUIRE(remote.count() == 1);
    FL_REQUIRE(remote.has("test"));
}

FL_TEST_CASE("Remote: Bind method - with return value") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("add", [](int a, int b) { return a + b; });

    FL_REQUIRE(remote.count() == 1);
    FL_REQUIRE(remote.has("add"));
}

FL_TEST_CASE("Remote: Bind method - with config") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    auto multiplyFn = [](int a, int b) { return a * b; };
    remote.bind(fl::Rpc::Config<decltype(multiplyFn)>{
        "multiply",
        multiplyFn,
        {"a", "b"},
        "Multiplies two integers"
    });

    FL_REQUIRE(remote.count() == 1);
    FL_REQUIRE(remote.has("multiply"));
}

FL_TEST_CASE("Remote: Unbind method") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() {});
    FL_REQUIRE(remote.has("test"));

    bool removed = remote.unbind("test");
    FL_REQUIRE(removed);
    FL_REQUIRE(!remote.has("test"));
}

FL_TEST_CASE("Remote: Get method by signature") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("add", [](int a, int b) { return a + b; });

    auto addFn = remote.get<int(int, int)>("add");
    FL_REQUIRE(addFn.ok());
    FL_REQUIRE(addFn(5, 7) == 12);
}

// =============================================================================
// Immediate Execution Tests
// =============================================================================

FL_TEST_CASE("Remote: Process immediate RPC - void return") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    int called = 0;
    remote.bind("test", [&called]() { called++; });

    fl::Json request = makeRequest("test");
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(called == 1);
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["id"].as_int().value() == 1);
}

FL_TEST_CASE("Remote: Process immediate RPC - with return value") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("add", [](int a, int b) { return a + b; });

    fl::Json params = fl::Json::array();
    params.push_back(5);
    params.push_back(7);
    fl::Json request = makeRequest("add", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].as_int().value() == 12);
}

FL_TEST_CASE("Remote: Process RPC - unknown method") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    fl::Json request = makeRequest("unknown");
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("error"));
}

// =============================================================================
// Scheduled Execution Tests
// =============================================================================

FL_TEST_CASE("Remote: Schedule RPC for future execution") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    int called = 0;
    remote.bind("test", [&called]() { called++; });

    // Schedule for timestamp 1000
    fl::Json request = makeRequest("test", fl::Json::array(), 1, 1000);
    fl::Json response = remote.processRpc(request);

    // Should not execute yet
    FL_REQUIRE(called == 0);
    FL_REQUIRE(remote.pendingCount() == 1);

    // Response for scheduled call is null
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].is_null());
}

FL_TEST_CASE("Remote: Tick executes scheduled RPCs") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    int called = 0;
    remote.bind("test", [&called]() { called++; });

    // Schedule for timestamp 1000
    fl::Json request = makeRequest("test", fl::Json::array(), 1, 1000);
    remote.processRpc(request);
    FL_REQUIRE(called == 0);

    // Tick at time 999 - not ready yet
    size_t executed = remote.tick(999);
    FL_REQUIRE(executed == 0);
    FL_REQUIRE(called == 0);

    // Tick at time 1000 - should execute
    executed = remote.tick(1000);
    FL_REQUIRE(executed == 1);
    FL_REQUIRE(called == 1);
    FL_REQUIRE(remote.pendingCount() == 0);
}

FL_TEST_CASE("Remote: Multiple scheduled RPCs execute in order") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    fl::vector<int> callOrder;
    remote.bind("task1", [&callOrder]() { callOrder.push_back(1); });
    remote.bind("task2", [&callOrder]() { callOrder.push_back(2); });
    remote.bind("task3", [&callOrder]() { callOrder.push_back(3); });

    // Schedule out of order
    remote.processRpc(makeRequest("task2", fl::Json::array(), 2, 2000));
    remote.processRpc(makeRequest("task1", fl::Json::array(), 1, 1000));
    remote.processRpc(makeRequest("task3", fl::Json::array(), 3, 3000));

    FL_REQUIRE(remote.pendingCount() == 3);

    // Execute all
    remote.tick(3000);

    // Should execute in timestamp order
    FL_REQUIRE(callOrder.size() == 3);
    FL_REQUIRE(callOrder[0] == 1);
    FL_REQUIRE(callOrder[1] == 2);
    FL_REQUIRE(callOrder[2] == 3);
}

// =============================================================================
// I/O Coordination Tests
// =============================================================================

FL_TEST_CASE("Remote: Pull requests from source") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() { return 42; });

    // Queue requests
    io.requests.push_back(makeRequest("test"));
    io.requests.push_back(makeRequest("test"));

    size_t processed = remote.pull();
    FL_REQUIRE(processed == 2);
}

FL_TEST_CASE("Remote: Push responses to sink") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() { return 42; });

    // Pull request
    io.requests.push_back(makeRequest("test"));
    remote.pull();

    // Push response
    size_t sent = remote.push();
    FL_REQUIRE(sent == 1);
    FL_REQUIRE(io.responses.size() == 1);
    FL_REQUIRE(io.responses[0].contains("result"));
}

FL_TEST_CASE("Remote: Update combines pull + tick + push") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    int immediate = 0;
    int scheduled = 0;
    remote.bind("immediate", [&immediate]() { immediate++; });
    remote.bind("scheduled", [&scheduled]() { scheduled++; });

    // Queue immediate and scheduled requests
    io.requests.push_back(makeRequest("immediate"));
    io.requests.push_back(makeRequest("scheduled", fl::Json::array(), 2, 1000));

    // Update at time 1000
    size_t total = remote.update(1000);
    FL_REQUIRE(total >= 2);  // pull + tick + push

    // Both should execute
    FL_REQUIRE(immediate == 1);
    FL_REQUIRE(scheduled == 1);

    // Responses should be pushed
    FL_REQUIRE(io.responses.size() == 2);
}

// =============================================================================
// State Clearing Tests
// =============================================================================

FL_TEST_CASE("Remote: Clear scheduled tasks") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() {});

    // Schedule tasks
    remote.processRpc(makeRequest("test", fl::Json::array(), 1, 1000));
    remote.processRpc(makeRequest("test", fl::Json::array(), 2, 2000));
    FL_REQUIRE(remote.pendingCount() == 2);

    // Clear scheduled
    remote.clear(fl::Remote::ClearFlags::Scheduled);
    FL_REQUIRE(remote.pendingCount() == 0);
}

FL_TEST_CASE("Remote: Clear functions") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test1", []() {});
    remote.bind("test2", []() {});
    FL_REQUIRE(remote.count() == 2);

    // Clear functions
    remote.clear(fl::Remote::ClearFlags::Functions);
    FL_REQUIRE(remote.count() == 0);
}

FL_TEST_CASE("Remote: Clear multiple flags with OR") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() {});
    remote.processRpc(makeRequest("test", fl::Json::array(), 1, 1000));

    FL_REQUIRE(remote.count() == 1);
    FL_REQUIRE(remote.pendingCount() == 1);

    // Clear both
    remote.clear(fl::Remote::ClearFlags::Functions | fl::Remote::ClearFlags::Scheduled);
    FL_REQUIRE(remote.count() == 0);
    FL_REQUIRE(remote.pendingCount() == 0);
}

// =============================================================================
// Schema Tests
// =============================================================================

FL_TEST_CASE("Remote: Methods returns schema info") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    auto addFn = [](int a, int b) { return a + b; };
    remote.bind(fl::Rpc::Config<decltype(addFn)>{
        "add",
        addFn,
        {"a", "b"},
        "Adds two integers"
    });

    auto methods = remote.methods();
    FL_REQUIRE(methods.size() == 1);
    FL_REQUIRE(methods[0].name == "add");
    FL_REQUIRE(methods[0].params.size() == 2);
    FL_REQUIRE(methods[0].params[0].name == "a");
    FL_REQUIRE(methods[0].params[1].name == "b");
    FL_REQUIRE(methods[0].description == "Adds two integers");
}

FL_TEST_CASE("Remote: Count returns number of methods") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    FL_REQUIRE(remote.count() == 0);

    remote.bind("test1", []() {});
    FL_REQUIRE(remote.count() == 1);

    remote.bind("test2", []() {});
    FL_REQUIRE(remote.count() == 2);
}

#endif // FASTLED_ENABLE_JSON
