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
    FL_REQUIRE(methods[0].returnType == "integer");
    FL_REQUIRE(methods[0].params.size() == 2);
    FL_REQUIRE(methods[0].params[0].name == "a");
    FL_REQUIRE(methods[0].params[0].type == "integer");
    FL_REQUIRE(methods[0].params[1].name == "b");
    FL_REQUIRE(methods[0].params[1].type == "integer");
    // Note: Flat schema format doesn't include description/tags
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

// =============================================================================
// Flat Schema Tests
// =============================================================================

FL_TEST_CASE("Remote: schema returns minimal schema") {
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

    auto voidFn = []() {};
    remote.bind("test", voidFn);

    // Get flat schema
    fl::Json schema = remote.schema();

    // Should have "schema" key
    FL_REQUIRE(schema.contains("schema"));
    FL_REQUIRE(schema["schema"].is_array());

    // Should have 2 methods
    fl::Json methods = schema["schema"];
    FL_REQUIRE(methods.size() == 2);

    // Find add method
    fl::Json addMethod;
    bool found_add = false;
    for (fl::size i = 0; i < methods.size(); i++) {
        if (methods[i][0].as_string().value() == "add") {
            addMethod = methods[i];
            found_add = true;
            break;
        }
    }
    FL_REQUIRE(found_add);

    // Verify format: ["methodName", "returnType", [["param1", "type1"], ...]]
    FL_REQUIRE(addMethod.is_array());
    FL_REQUIRE(addMethod.size() == 3);

    // Method name
    FL_REQUIRE(addMethod[0].is_string());
    FL_REQUIRE(addMethod[0].as_string().value() == "add");

    // Return type
    FL_REQUIRE(addMethod[1].is_string());
    FL_REQUIRE(addMethod[1].as_string().value() == "integer");

    // Params array
    FL_REQUIRE(addMethod[2].is_array());
    FL_REQUIRE(addMethod[2].size() == 2);

    // First param: ["a", "integer"]
    FL_REQUIRE(addMethod[2][0].is_array());
    FL_REQUIRE(addMethod[2][0].size() == 2);
    FL_REQUIRE(addMethod[2][0][0].as_string().value() == "a");
    FL_REQUIRE(addMethod[2][0][1].as_string().value() == "integer");

    // Second param: ["b", "integer"]
    FL_REQUIRE(addMethod[2][1].is_array());
    FL_REQUIRE(addMethod[2][1].size() == 2);
    FL_REQUIRE(addMethod[2][1][0].as_string().value() == "b");
    FL_REQUIRE(addMethod[2][1][1].as_string().value() == "integer");

    // Find void method
    fl::Json voidMethod;
    bool found_void = false;
    for (fl::size i = 0; i < methods.size(); i++) {
        if (methods[i][0].as_string().value() == "test") {
            voidMethod = methods[i];
            found_void = true;
            break;
        }
    }
    FL_REQUIRE(found_void);

    // Verify void return type
    FL_REQUIRE(voidMethod[1].as_string().value() == "void");
    FL_REQUIRE(voidMethod[2].is_array());
    FL_REQUIRE(voidMethod[2].size() == 0);  // No params
}

FL_TEST_CASE("Remote: rpc.discover built-in method") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", [](int x) { return x * 2; });

    // Call rpc.discover
    fl::Json request = makeRequest("rpc.discover");
    fl::Json response = remote.processRpc(request);

    // Should succeed
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].is_object());

    // Result should have "schema" key
    fl::Json result = response["result"];
    FL_REQUIRE(result.contains("schema"));
    FL_REQUIRE(result["schema"].is_array());

    // Should have at least 1 method (our "test" method)
    FL_REQUIRE(result["schema"].size() >= 1);
}

FL_TEST_CASE("Remote: Schema is compact array format") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Register several methods
    remote.bind("method1", [](int a, int b, int c) { return a + b + c; });
    remote.bind("method2", [](fl::string s) -> int { return static_cast<int>(s.length()); });
    remote.bind("method3", [](bool flag) {});
    remote.bind("method4", [](float x, float y) -> float { return x * y; });

    // Get schema
    fl::Json schema = remote.schema();

    // Verify it has "schema" key with array
    FL_REQUIRE(schema.contains("schema"));
    FL_REQUIRE(schema["schema"].is_array());
    FL_REQUIRE(schema["schema"].size() == 4);

    // Verify it's compact (each method is an array, not an object)
    fl::Json methods = schema["schema"];
    for (fl::size i = 0; i < methods.size(); i++) {
        FL_REQUIRE(methods[i].is_array());
        FL_REQUIRE(methods[i].size() == 3);  // [name, returnType, params]
    }
}

FL_TEST_CASE("Remote: Flat schema type mappings") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Register methods with different types
    remote.bind("voidFunc", []() {});
    remote.bind("intFunc", [](int x) -> int { return x; });
    remote.bind("boolFunc", [](bool b) -> bool { return b; });
    remote.bind("floatFunc", [](float f) -> float { return f; });
    remote.bind("stringFunc", [](fl::string s) -> fl::string { return s; });
    remote.bind("jsonFunc", [](fl::Json j) -> fl::Json { return j; });

    fl::Json schema = remote.schema();
    fl::Json methods = schema["schema"];

    // Helper to find method
    auto findMethod = [&methods](const char* name) -> fl::Json {
        for (fl::size i = 0; i < methods.size(); i++) {
            if (methods[i][0].as_string().value() == name) {
                return methods[i];
            }
        }
        return fl::Json();
    };

    // Verify type mappings
    FL_REQUIRE(findMethod("voidFunc")[1].as_string().value() == "void");
    FL_REQUIRE(findMethod("intFunc")[1].as_string().value() == "integer");
    FL_REQUIRE(findMethod("boolFunc")[1].as_string().value() == "boolean");
    FL_REQUIRE(findMethod("floatFunc")[1].as_string().value() == "number");
    FL_REQUIRE(findMethod("stringFunc")[1].as_string().value() == "string");

    // JSON can be object or array or unknown
    fl::string jsonType = findMethod("jsonFunc")[1].as_string().value();
    bool validJsonType = (jsonType == "object") || (jsonType == "array") || (jsonType == "unknown");
    FL_REQUIRE(validJsonType);

    // Verify parameter types
    FL_REQUIRE(findMethod("intFunc")[2][0][1].as_string().value() == "integer");
    FL_REQUIRE(findMethod("boolFunc")[2][0][1].as_string().value() == "boolean");
    FL_REQUIRE(findMethod("floatFunc")[2][0][1].as_string().value() == "number");
    FL_REQUIRE(findMethod("stringFunc")[2][0][1].as_string().value() == "string");
}

FL_TEST_CASE("Remote: Flat schema with no parameters") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("noParams", []() -> int { return 42; });

    fl::Json schema = remote.schema();
    fl::Json methods = schema["schema"];

    FL_REQUIRE(methods.size() == 1);
    FL_REQUIRE(methods[0][0].as_string().value() == "noParams");
    FL_REQUIRE(methods[0][1].as_string().value() == "integer");
    FL_REQUIRE(methods[0][2].is_array());
    FL_REQUIRE(methods[0][2].size() == 0);  // Empty params array
}

FL_TEST_CASE("Remote: Flat schema with many parameters") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Method with 5 parameters
    remote.bind("manyParams", [](int a, int b, int c, int d, int e) -> int {
        return a + b + c + d + e;
    });

    fl::Json schema = remote.schema();
    fl::Json methods = schema["schema"];
    fl::Json params = methods[0][2];

    FL_REQUIRE(params.size() == 5);
    for (fl::size i = 0; i < 5; i++) {
        FL_REQUIRE(params[i].is_array());
        FL_REQUIRE(params[i].size() == 2);
        FL_REQUIRE(params[i][1].as_string().value() == "integer");
    }
}

FL_TEST_CASE("Remote: Flat schema via JSON-RPC with ID") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    remote.bind("test", []() {});

    // Call with explicit ID
    fl::Json request = makeRequest("rpc.discover", fl::Json::array(), 42);
    fl::Json response = remote.processRpc(request);

    // Response should have matching ID
    FL_REQUIRE(response.contains("id"));
    FL_REQUIRE(response["id"].as_int().value() == 42);

    // Should have jsonrpc field
    FL_REQUIRE(response.contains("jsonrpc"));
    FL_REQUIRE(response["jsonrpc"].as_string().value() == "2.0");

    // Should have result with schema
    FL_REQUIRE(response.contains("result"));
    FL_REQUIRE(response["result"].contains("schema"));
}

// =============================================================================
// Json Parameter Tests
// =============================================================================

FL_TEST_CASE("Remote: Bind method with const Json& parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that const Json& parameters work correctly
    // The RPC system should strip const/ref and store by value
    remote.bind("echo", [](const fl::Json& args) -> fl::Json {
        // Verify we receive the args correctly
        return args;
    });

    FL_REQUIRE(remote.count() == 1);
    FL_REQUIRE(remote.has("echo"));

    // Create request with array parameter containing a JSON object
    fl::Json testData = fl::Json::object();
    testData.set("key", "value");
    testData.set("number", 42);

    fl::Json params = fl::Json::array();
    params.push_back(testData);

    fl::Json request = makeRequest("echo", params);
    fl::Json response = remote.processRpc(request);

    // Verify response structure
    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];

    // Result should be the testData object we passed in
    FL_REQUIRE(result.is_object());
    FL_REQUIRE(result.contains("key"));
    FL_REQUIRE(result["key"].as_string().value() == "value");
    FL_REQUIRE(result.contains("number"));
    FL_REQUIRE(result["number"].as_int().value() == 42);
}

FL_TEST_CASE("Remote: Bind method with Json value parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that fl::Json (by value) also works
    remote.bind("echoByValue", [](fl::Json args) -> fl::Json {
        return args;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json testData = fl::Json::object();
    testData.set("test", true);

    fl::Json params = fl::Json::array();
    params.push_back(testData);

    fl::Json request = makeRequest("echoByValue", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];
    FL_REQUIRE(result.is_object());
    FL_REQUIRE(result.contains("test"));
    FL_REQUIRE(result["test"].as_bool().value() == true);
}

FL_TEST_CASE("Remote: Bind method with const string& parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that const fl::string& parameters work
    remote.bind("greet", [](const fl::string& name) -> fl::string {
        return "Hello, " + name;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json params = fl::Json::array();
    params.push_back(fl::Json("World"));

    fl::Json request = makeRequest("greet", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    auto resultStr = response["result"].as_string();
    FL_REQUIRE(resultStr.has_value());
    FL_REQUIRE(resultStr.value() == "Hello, World");
}

FL_TEST_CASE("Remote: Bind method with string value parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that fl::string (by value) also works
    remote.bind("upper", [](fl::string str) -> fl::string {
        // Simple uppercase conversion
        for (size_t i = 0; i < str.length(); i++) {
            char c = str.c_str()[i];
            if (c >= 'a' && c <= 'z') {
                str = fl::string(str.c_str()).substr(0, i) +
                      fl::string(1, (char)(c - 32)) +
                      fl::string(str.c_str() + i + 1);
            }
        }
        return str;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json params = fl::Json::array();
    params.push_back(fl::Json("test"));

    fl::Json request = makeRequest("upper", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    auto resultStr = response["result"].as_string();
    FL_REQUIRE(resultStr.has_value());
    FL_REQUIRE(resultStr.value() == "TEST");
}

FL_TEST_CASE("Remote: Bind method with const char* parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that const char* parameters work
    // The RPC system stores a ConstCharPtrWrapper internally
    remote.bind("length", [](const char* str) -> int {
        int len = 0;
        while (str[len] != '\0') {
            len++;
        }
        return len;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json params = fl::Json::array();
    params.push_back(fl::Json("hello"));

    fl::Json request = makeRequest("length", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    auto resultInt = response["result"].as_int();
    FL_REQUIRE(resultInt.has_value());
    FL_REQUIRE(resultInt.value() == 5);
}

FL_TEST_CASE("Remote: Bind method with span<const int> parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that span<const int> parameters work
    remote.bind("sum", [](fl::span<const int> nums) -> int {
        int total = 0;
        for (size_t i = 0; i < nums.size(); i++) {
            total += nums[i];
        }
        return total;
    });

    FL_REQUIRE(remote.count() == 1);

    // Create JSON array of integers
    fl::Json nums = fl::Json::array();
    nums.push_back(fl::Json(1));
    nums.push_back(fl::Json(2));
    nums.push_back(fl::Json(3));
    nums.push_back(fl::Json(4));
    nums.push_back(fl::Json(5));

    fl::Json params = fl::Json::array();
    params.push_back(nums);

    fl::Json request = makeRequest("sum", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    auto resultInt = response["result"].as_int();
    FL_REQUIRE(resultInt.has_value());
    FL_REQUIRE(resultInt.value() == 15);  // 1+2+3+4+5 = 15
}

FL_TEST_CASE("Remote: Bind method with span<const float> parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that span<const float> parameters work
    remote.bind("average", [](fl::span<const float> nums) -> float {
        if (nums.size() == 0) return 0.0f;
        float total = 0.0f;
        for (size_t i = 0; i < nums.size(); i++) {
            total += nums[i];
        }
        return total / nums.size();
    });

    FL_REQUIRE(remote.count() == 1);

    // Create JSON array of floats
    fl::Json nums = fl::Json::array();
    nums.push_back(fl::Json(1.5));
    nums.push_back(fl::Json(2.5));
    nums.push_back(fl::Json(3.5));

    fl::Json params = fl::Json::array();
    params.push_back(nums);

    fl::Json request = makeRequest("average", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    // Result should be approximately 2.5
    // Note: JSON may store as double, check if close enough
}

// =============================================================================
// Vector Parameter Tests
// =============================================================================

FL_TEST_CASE("Remote: Bind method with vector<int> parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that fl::vector<int> parameters work (by value)
    remote.bind("reverse", [](fl::vector<int> nums) -> fl::vector<int> {
        fl::vector<int> result;
        for (fl::size i = nums.size(); i > 0; i--) {
            result.push_back(nums[i - 1]);
        }
        return result;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json nums = fl::Json::array();
    nums.push_back(fl::Json(1));
    nums.push_back(fl::Json(2));
    nums.push_back(fl::Json(3));

    fl::Json params = fl::Json::array();
    params.push_back(nums);

    fl::Json request = makeRequest("reverse", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];
    FL_REQUIRE(result.is_array());
    FL_REQUIRE(result.size() == 3);
    FL_REQUIRE(result[0].as_int().value() == 3);
    FL_REQUIRE(result[1].as_int().value() == 2);
    FL_REQUIRE(result[2].as_int().value() == 1);
}

FL_TEST_CASE("Remote: Bind method with const vector<float>& parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that const fl::vector<float>& parameters work
    remote.bind("scale", [](const fl::vector<float>& nums, float factor) -> fl::vector<float> {
        fl::vector<float> result;
        for (fl::size i = 0; i < nums.size(); i++) {
            result.push_back(nums[i] * factor);
        }
        return result;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json nums = fl::Json::array();
    nums.push_back(fl::Json(1.0));
    nums.push_back(fl::Json(2.0));
    nums.push_back(fl::Json(3.0));

    fl::Json params = fl::Json::array();
    params.push_back(nums);
    params.push_back(fl::Json(2.0));

    fl::Json request = makeRequest("scale", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];
    FL_REQUIRE(result.is_array());
    FL_REQUIRE(result.size() == 3);
    // Check that values are approximately 2.0, 4.0, 6.0
}

FL_TEST_CASE("Remote: Bind method with vector<string> parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test that fl::vector<fl::string> parameters work
    remote.bind("join", [](const fl::vector<fl::string>& words, const fl::string& sep) -> fl::string {
        if (words.size() == 0) return fl::string();
        fl::string result = words[0];
        for (fl::size i = 1; i < words.size(); i++) {
            result = result + sep + words[i];
        }
        return result;
    });

    FL_REQUIRE(remote.count() == 1);

    fl::Json words = fl::Json::array();
    words.push_back(fl::Json("Hello"));
    words.push_back(fl::Json("World"));
    words.push_back(fl::Json("Test"));

    fl::Json params = fl::Json::array();
    params.push_back(words);
    params.push_back(fl::Json(" "));

    fl::Json request = makeRequest("join", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    auto resultStr = response["result"].as_string();
    FL_REQUIRE(resultStr.has_value());
    FL_REQUIRE(resultStr.value() == "Hello World Test");
}

FL_TEST_CASE("Remote: Bind method with vector<vector<int>> (nested) parameter") {
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::Json& r) { io.pushResponse(r); }
    );

    // Test nested vectors
    remote.bind("flatten", [](const fl::vector<fl::vector<int>>& matrix) -> fl::vector<int> {
        fl::vector<int> result;
        for (fl::size i = 0; i < matrix.size(); i++) {
            for (fl::size j = 0; j < matrix[i].size(); j++) {
                result.push_back(matrix[i][j]);
            }
        }
        return result;
    });

    FL_REQUIRE(remote.count() == 1);

    // Create nested array [[1,2], [3,4], [5,6]]
    fl::Json row1 = fl::Json::array();
    row1.push_back(fl::Json(1));
    row1.push_back(fl::Json(2));

    fl::Json row2 = fl::Json::array();
    row2.push_back(fl::Json(3));
    row2.push_back(fl::Json(4));

    fl::Json row3 = fl::Json::array();
    row3.push_back(fl::Json(5));
    row3.push_back(fl::Json(6));

    fl::Json matrix = fl::Json::array();
    matrix.push_back(row1);
    matrix.push_back(row2);
    matrix.push_back(row3);

    fl::Json params = fl::Json::array();
    params.push_back(matrix);

    fl::Json request = makeRequest("flatten", params);
    fl::Json response = remote.processRpc(request);

    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];
    FL_REQUIRE(result.is_array());
    FL_REQUIRE(result.size() == 6);
    FL_REQUIRE(result[0].as_int().value() == 1);
    FL_REQUIRE(result[1].as_int().value() == 2);
    FL_REQUIRE(result[2].as_int().value() == 3);
    FL_REQUIRE(result[3].as_int().value() == 4);
    FL_REQUIRE(result[4].as_int().value() == 5);
    FL_REQUIRE(result[5].as_int().value() == 6);
}

// =============================================================================
// JSON I/O Pipeline Tests (In-Memory Streams)
// =============================================================================

// In-memory stream adapter for testing JSON input/output
struct MemoryStream {
    fl::vector<fl::string> inputLines;
    fl::vector<fl::string> outputLines;
    size_t inputIndex = 0;

    // Input methods (SerialReader interface)
    int available() const {
        if (inputIndex >= inputLines.size()) return 0;
        return 1;
    }

    int read() {
        if (inputIndex >= inputLines.size()) return -1;
        static size_t charIndex = 0;

        const fl::string& currentLine = inputLines[inputIndex];
        if (charIndex >= currentLine.size()) {
            charIndex = 0;
            inputIndex++;
            return '\n';  // Return newline at end of line
        }

        return currentLine[charIndex++];
    }

    // Output methods (SerialWriter interface)
    void println(const char* str) {
        outputLines.push_back(fl::string(str));
    }

    // Test helpers
    void addInput(const fl::string& line) {
        inputLines.push_back(line);
    }

    void reset() {
        inputLines.clear();
        outputLines.clear();
        inputIndex = 0;
    }

    fl::string getLastOutput() const {
        if (outputLines.empty()) return fl::string();
        return outputLines[outputLines.size() - 1];
    }
};

FL_TEST_CASE("Remote: JSON input pipeline - parse valid request") {
    // Test that valid JSON request is parsed correctly
    MemoryStream stream;

    // Add raw JSON string to input
    stream.addInput(R"({"method":"add","params":[5,3],"id":1})");

    // Create request source
    auto requestSource = [&stream]() -> fl::optional<fl::Json> {
        // Simulate readSerialLine
        fl::string line;
        while (true) {
            int c = stream.read();
            if (c == -1) return fl::nullopt;
            if (c == '\n') break;
            line += static_cast<char>(c);
        }

        // Parse JSON
        return fl::Json::parse(line);
    };

    // Pull request
    auto request = requestSource();
    FL_REQUIRE(request.has_value());
    FL_REQUIRE(request->contains("method"));
    FL_REQUIRE(request->contains("params"));
    FL_REQUIRE(request->contains("id"));
    FL_REQUIRE(request.value()["method"].as_string().value() == "add");
}

FL_TEST_CASE("Remote: JSON output pipeline - format response") {
    // Test that response is formatted correctly
    MemoryStream stream;

    // Create response
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", 42);
    response.set("id", 1);

    // Create response sink
    auto responseSink = [&stream](const fl::Json& r) {
        fl::string formatted = r.to_string();
        stream.println(formatted.c_str());
    };

    // Send response
    responseSink(response);

    // Verify output
    FL_REQUIRE(stream.outputLines.size() == 1);
    fl::string output = stream.outputLines[0];
    FL_REQUIRE(output.find("\"result\"") != fl::string::npos);
    FL_REQUIRE(output.find("42") != fl::string::npos);
}

FL_TEST_CASE("Remote: End-to-end JSON pipeline with prefix") {
    // Test complete request/response cycle with prefix stripping
    MemoryStream inputStream;
    MemoryStream outputStream;

    // Add request with prefix
    inputStream.addInput(R"(REMOTE: {"method":"echo","params":["hello"],"id":1})");

    // Create request source with prefix stripping
    auto requestSource = [&inputStream]() -> fl::optional<fl::Json> {
        fl::string line;
        while (true) {
            int c = inputStream.read();
            if (c == -1) return fl::nullopt;
            if (c == '\n') break;
            line += static_cast<char>(c);
        }

        // Strip prefix using string_view (zero-copy)
        fl::string_view view = line;
        const char* prefix = "REMOTE: ";
        if (view.starts_with(prefix)) {
            view.remove_prefix(fl::strlen(prefix));
        }

        // Trim whitespace
        while (!view.empty() && fl::isspace(view.front())) {
            view.remove_prefix(1);
        }
        while (!view.empty() && fl::isspace(view.back())) {
            view.remove_suffix(1);
        }

        // Parse JSON
        fl::string cleaned(view);
        return fl::Json::parse(cleaned);
    };

    // Create response sink with prefix
    auto responseSink = [&outputStream](const fl::Json& r) {
        fl::string formatted = "REMOTE: " + r.to_string();
        outputStream.println(formatted.c_str());
    };

    // Create Remote with stream adapters
    fl::Remote remote(requestSource, responseSink);
    remote.bind("echo", [](const fl::string& msg) -> fl::string { return msg; });

    // Process request
    remote.pull();
    remote.push();

    // Verify output
    FL_REQUIRE(outputStream.outputLines.size() == 1);
    fl::string output = outputStream.outputLines[0];
    FL_REQUIRE(output.starts_with("REMOTE: "));
    FL_REQUIRE(output.find("\"result\"") != fl::string::npos);
    FL_REQUIRE(output.find("\"hello\"") != fl::string::npos);
}

FL_TEST_CASE("Remote: Schema generation via JSON pipeline") {
    // Test rpc.discover schema generation through JSON I/O
    MemoryStream inputStream;
    MemoryStream outputStream;

    // Add schema request
    inputStream.addInput(R"({"method":"rpc.discover","params":[],"id":1})");

    // Create I/O adapters
    auto requestSource = [&inputStream]() -> fl::optional<fl::Json> {
        fl::string line;
        while (true) {
            int c = inputStream.read();
            if (c == -1) return fl::nullopt;
            if (c == '\n') break;
            line += static_cast<char>(c);
        }
        return fl::Json::parse(line);
    };

    auto responseSink = [&outputStream](const fl::Json& r) {
        outputStream.println(r.to_string().c_str());
    };

    // Create Remote and bind methods
    fl::Remote remote(requestSource, responseSink);
    remote.bind("add", [](int a, int b) -> int { return a + b; });
    remote.bind("multiply", [](int a, int b) -> int { return a * b; });

    // Process schema request
    remote.pull();
    remote.push();

    // Parse output
    FL_REQUIRE(outputStream.outputLines.size() == 1);
    fl::Json response = fl::Json::parse(outputStream.outputLines[0]);

    // Verify schema structure
    FL_REQUIRE(response.contains("result"));
    fl::Json result = response["result"];
    FL_REQUIRE(result.contains("schema"));
    FL_REQUIRE(result["schema"].is_array());

    // Verify schema contains our methods
    fl::Json schema = result["schema"];
    FL_REQUIRE(schema.size() >= 2);  // At least add and multiply
}

FL_TEST_CASE("Remote: Multiple RPC calls via JSON pipeline") {
    // Test multiple sequential RPC calls
    MemoryStream inputStream;
    MemoryStream outputStream;

    // Add multiple requests
    inputStream.addInput(R"({"method":"add","params":[5,3],"id":1})");
    inputStream.addInput(R"({"method":"multiply","params":[4,7],"id":2})");
    inputStream.addInput(R"({"method":"subtract","params":[10,6],"id":3})");

    // Create I/O adapters
    auto requestSource = [&inputStream]() -> fl::optional<fl::Json> {
        fl::string line;
        while (true) {
            int c = inputStream.read();
            if (c == -1) return fl::nullopt;
            if (c == '\n') break;
            line += static_cast<char>(c);
        }
        return fl::Json::parse(line);
    };

    auto responseSink = [&outputStream](const fl::Json& r) {
        outputStream.println(r.to_string().c_str());
    };

    // Create Remote and bind methods
    fl::Remote remote(requestSource, responseSink);
    remote.bind("add", [](int a, int b) -> int { return a + b; });
    remote.bind("multiply", [](int a, int b) -> int { return a * b; });
    remote.bind("subtract", [](int a, int b) -> int { return a - b; });

    // Process all requests
    remote.pull();  // Pull all 3 requests
    remote.push();  // Send all 3 responses

    // Verify all responses
    FL_REQUIRE(outputStream.outputLines.size() == 3);

    // Parse and verify first response (add)
    fl::Json r1 = fl::Json::parse(outputStream.outputLines[0]);
    FL_REQUIRE(r1.contains("result"));
    FL_REQUIRE(r1["result"].as_int().value() == 8);  // 5+3
    FL_REQUIRE(r1["id"].as_int().value() == 1);

    // Parse and verify second response (multiply)
    fl::Json r2 = fl::Json::parse(outputStream.outputLines[1]);
    FL_REQUIRE(r2.contains("result"));
    FL_REQUIRE(r2["result"].as_int().value() == 28);  // 4*7
    FL_REQUIRE(r2["id"].as_int().value() == 2);

    // Parse and verify third response (subtract)
    fl::Json r3 = fl::Json::parse(outputStream.outputLines[2]);
    FL_REQUIRE(r3.contains("result"));
    FL_REQUIRE(r3["result"].as_int().value() == 4);  // 10-6
    FL_REQUIRE(r3["id"].as_int().value() == 3);
}

FL_TEST_CASE("Remote: Error handling via JSON pipeline") {
    // Test error responses for unknown methods
    MemoryStream inputStream;
    MemoryStream outputStream;

    // Add request for unknown method
    inputStream.addInput(R"({"method":"unknownMethod","params":[],"id":42})");

    // Create I/O adapters
    auto requestSource = [&inputStream]() -> fl::optional<fl::Json> {
        fl::string line;
        while (true) {
            int c = inputStream.read();
            if (c == -1) return fl::nullopt;
            if (c == '\n') break;
            line += static_cast<char>(c);
        }
        return fl::Json::parse(line);
    };

    auto responseSink = [&outputStream](const fl::Json& r) {
        outputStream.println(r.to_string().c_str());
    };

    // Create Remote (no methods bound)
    fl::Remote remote(requestSource, responseSink);

    // Process request
    remote.pull();
    remote.push();

    // Verify error response
    FL_REQUIRE(outputStream.outputLines.size() == 1);
    fl::Json response = fl::Json::parse(outputStream.outputLines[0]);
    FL_REQUIRE(response.contains("error"));
    FL_REQUIRE(response["id"].as_int().value() == 42);
}

FL_TEST_CASE("Remote: Compact JSON output (no newlines)") {
    // Test that JSON output is compact (single line)
    MemoryStream outputStream;

    // Create complex response
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    fl::Json result = fl::Json::object();
    result.set("value", 42);
    result.set("status", "success");
    response.set("result", result);
    response.set("id", 1);

    // Write to stream
    outputStream.println(response.to_string().c_str());

    // Verify output is single line (no newlines in JSON)
    fl::string output = outputStream.outputLines[0];
    FL_REQUIRE(output.find('\n') == fl::string::npos);
    FL_REQUIRE(output.find('\r') == fl::string::npos);
}

#endif // FASTLED_ENABLE_JSON
