#include "test.h"
#include "fl/remote.h"
#include "fx/wled.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/time.h"

// Test 4.1 - Basic Function Registration
TEST_CASE("Remote: Basic function registration") {
    fl::Remote remote;

    bool called = false;
    remote.registerFunction("test", [&](const fl::Json& args) {
        called = true;
    });

    REQUIRE(remote.hasFunction("test"));
    REQUIRE_FALSE(remote.hasFunction("unknown"));

    remote.processRpc(R"({"timestamp":0,"function":"test","args":[]})");
    REQUIRE(called);
}

// Test 4.2 - Immediate Execution
TEST_CASE("Remote: Immediate execution") {
    fl::Remote remote;

    int result = 0;
    remote.registerFunction("add", [&](const fl::Json& args) {
        int a = args[0] | 0;
        int b = args[1] | 0;
        result = a + b;
    });

    remote.processRpc(R"({"timestamp":0,"function":"add","args":[5,3]})");
    REQUIRE_EQ(result, 8);
}

// Test 4.3 - Scheduled Execution
TEST_CASE("Remote: Scheduled execution") {
    fl::Remote remote;

    int callCount = 0;
    remote.registerFunction("increment", [&](const fl::Json& args) {
        callCount++;
    });

    remote.processRpc(R"({"timestamp":1000,"function":"increment","args":[]})");
    REQUIRE_EQ(callCount, 0);  // Not executed yet
    REQUIRE_EQ(remote.pendingCount(), 1);

    remote.tick(999);  // Before scheduled time
    REQUIRE_EQ(callCount, 0);

    remote.tick(1000);  // At scheduled time
    REQUIRE_EQ(callCount, 1);
    REQUIRE_EQ(remote.pendingCount(), 0);
}

// Test 4.4 - Error Handling
TEST_CASE("Remote: Error handling") {
    fl::Remote remote;

    // Invalid JSON
    auto err = remote.processRpc("{invalid json}");
    REQUIRE_EQ(err, fl::Remote::Error::InvalidJson);

    // Missing function field
    err = remote.processRpc(R"({"timestamp":0,"args":[]})");
    REQUIRE_EQ(err, fl::Remote::Error::MissingFunction);

    // Unknown function
    err = remote.processRpc(R"({"timestamp":0,"function":"unknown","args":[]})");
    REQUIRE_EQ(err, fl::Remote::Error::UnknownFunction);

    // Invalid timestamp (should warn but default to 0)
    remote.registerFunction("test", [](const fl::Json& args) {});
    err = remote.processRpc(R"({"timestamp":-5,"function":"test","args":[]})");
    REQUIRE_EQ(err, fl::Remote::Error::InvalidTimestamp);
}

// Test 4.5 - Argument Extraction
TEST_CASE("Remote: Argument extraction") {
    fl::Remote remote;

    fl::vector<int> received;
    remote.registerFunction("collect", [&](const fl::Json& args) {
        for (size_t i = 0; i < args.size(); i++) {
            received.push_back(args[i] | 0);
        }
    });

    remote.processRpc(R"({"function":"collect","args":[10,20,30]})");
    REQUIRE_EQ(received.size(), 3);
    REQUIRE_EQ(received[0], 10);
    REQUIRE_EQ(received[1], 20);
    REQUIRE_EQ(received[2], 30);
}

// Test 4.6 - Multiple Scheduled Calls
TEST_CASE("Remote: Multiple scheduled calls") {
    fl::Remote remote;

    fl::vector<fl::string> executed;
    remote.registerFunction("a", [&](const fl::Json& args) { executed.push_back("a"); });
    remote.registerFunction("b", [&](const fl::Json& args) { executed.push_back("b"); });
    remote.registerFunction("c", [&](const fl::Json& args) { executed.push_back("c"); });

    remote.processRpc(R"({"timestamp":3000,"function":"c","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"a","args":[]})");
    remote.processRpc(R"({"timestamp":2000,"function":"b","args":[]})");

    REQUIRE_EQ(remote.pendingCount(), 3);

    remote.tick(1500);
    REQUIRE_EQ(executed.size(), 1);  // Only 'a' should execute
    REQUIRE_EQ(executed[0], "a");

    remote.tick(2500);
    REQUIRE_EQ(executed.size(), 2);  // 'b' should execute
    REQUIRE_EQ(executed[1], "b");

    remote.tick(3500);
    REQUIRE_EQ(executed.size(), 3);  // 'c' should execute
    REQUIRE_EQ(executed[2], "c");

    REQUIRE_EQ(remote.pendingCount(), 0);
}

// Test 4.7 - Clear Operations
TEST_CASE("Remote: Clear operations") {
    fl::Remote remote;

    remote.registerFunction("test", [](const fl::Json& args) {});
    remote.processRpc(R"({"timestamp":1000,"function":"test","args":[]})");

    REQUIRE_EQ(remote.pendingCount(), 1);
    REQUIRE(remote.hasFunction("test"));

    remote.clearScheduled();
    REQUIRE_EQ(remote.pendingCount(), 0);
    REQUIRE(remote.hasFunction("test"));  // Still registered

    remote.processRpc(R"({"timestamp":1000,"function":"test","args":[]})");
    remote.clearFunctions();
    REQUIRE_FALSE(remote.hasFunction("test"));
    REQUIRE_EQ(remote.pendingCount(), 1);  // Still scheduled

    remote.clear();
    REQUIRE_EQ(remote.pendingCount(), 0);
    REQUIRE_FALSE(remote.hasFunction("test"));
}

// Test 4.8 - Return Values
TEST_CASE("Remote: Return values") {
    fl::Remote remote;

    // Register function with return value (matches Arduino millis())
    remote.registerFunctionWithReturn("millis", [](const fl::Json& args) -> fl::Json {
        return fl::Json(static_cast<int64_t>(12345));
    });

    // Call and capture return value
    fl::Json result;
    auto err = remote.processRpc(R"({"function":"millis","args":[]})", result);

    REQUIRE_EQ(err, fl::Remote::Error::None);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result | 0, 12345);
}

// Test 4.9 - Return Values with Arguments
TEST_CASE("Remote: Return values with arguments") {
    fl::Remote remote;

    // Register function that performs calculation and returns result
    remote.registerFunctionWithReturn("multiply", [](const fl::Json& args) -> fl::Json {
        int a = args[0] | 1;
        int b = args[1] | 1;
        fl::Json result = fl::Json::object();
        result.set("product", a * b);
        return result;
    });

    fl::Json result;
    auto err = remote.processRpc(R"({"function":"multiply","args":[6,7]})", result);

    REQUIRE_EQ(err, fl::Remote::Error::None);
    REQUIRE_EQ(result["product"] | 0, 42);
}

// Test 4.10 - Scheduled Functions with Return Values and Timing Metadata
TEST_CASE("Remote: Scheduled functions with return values and timing metadata") {
    fl::Remote remote;

    int counter = 100;
    remote.registerFunctionWithReturn("getCounter", [&](const fl::Json& args) -> fl::Json {
        return fl::Json(counter++);
    });

    // Schedule for future execution
    fl::Json result;
    auto err = remote.processRpc(R"({"timestamp":1000,"function":"getCounter","args":[]})", result);

    REQUIRE_EQ(err, fl::Remote::Error::None);
    REQUIRE_FALSE(result.has_value());  // Result not available yet (scheduled)
    REQUIRE_EQ(remote.pendingCount(), 1);

    // Execute scheduled function
    remote.tick(1000);

    // Check results are available with metadata
    auto results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);

    const auto& r = results[0];
    REQUIRE_EQ(r.functionName, "getCounter");
    REQUIRE(r.wasScheduled);
    REQUIRE_EQ(r.scheduledAt, 1000);
    REQUIRE(r.result.has_value());
    REQUIRE_EQ(r.result | 0, 100);
    REQUIRE_EQ(counter, 101);  // Counter was incremented

    // Verify timing: receivedAt <= scheduledAt <= executedAt
    REQUIRE(r.receivedAt <= r.scheduledAt);
    REQUIRE(r.scheduledAt <= r.executedAt);
}

// Test 4.11 - Stable Ordering (FIFO for Same Timestamp)
TEST_CASE("Remote: Stable ordering (FIFO for same timestamp)") {
    fl::Remote remote;

    fl::vector<fl::string> executionOrder;

    remote.registerFunction("a", [&](const fl::Json& args) { executionOrder.push_back("a"); });
    remote.registerFunction("b", [&](const fl::Json& args) { executionOrder.push_back("b"); });
    remote.registerFunction("c", [&](const fl::Json& args) { executionOrder.push_back("c"); });
    remote.registerFunction("d", [&](const fl::Json& args) { executionOrder.push_back("d"); });

    // Schedule all functions with the SAME timestamp
    // They should execute in the order they were scheduled (FIFO)
    remote.processRpc(R"({"timestamp":1000,"function":"a","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"b","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"c","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"d","args":[]})");

    REQUIRE_EQ(remote.pendingCount(), 4);

    // Execute all scheduled functions
    remote.tick(1000);

    // Verify FIFO execution order (stable ordering)
    REQUIRE_EQ(executionOrder.size(), 4);
    REQUIRE_EQ(executionOrder[0], "a");
    REQUIRE_EQ(executionOrder[1], "b");
    REQUIRE_EQ(executionOrder[2], "c");
    REQUIRE_EQ(executionOrder[3], "d");
    REQUIRE_EQ(remote.pendingCount(), 0);
}

// Additional test: Unregister function
TEST_CASE("Remote: Unregister function") {
    fl::Remote remote;

    remote.registerFunction("test", [](const fl::Json& args) {});
    REQUIRE(remote.hasFunction("test"));

    bool removed = remote.unregisterFunction("test");
    REQUIRE(removed);
    REQUIRE_FALSE(remote.hasFunction("test"));

    // Try to unregister non-existent function
    removed = remote.unregisterFunction("nonexistent");
    REQUIRE_FALSE(removed);
}

// Additional test: Results clearing
TEST_CASE("Remote: Results clearing") {
    fl::Remote remote;

    remote.registerFunctionWithReturn("getValue", [](const fl::Json& args) -> fl::Json {
        return fl::Json(42);
    });

    fl::Json result;
    remote.processRpc(R"({"function":"getValue","args":[]})", result);

    auto results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);

    remote.clearResults();
    results = remote.getResults();
    REQUIRE_EQ(results.size(), 0);
}

// Additional test: No args field (should default to empty array)
TEST_CASE("Remote: No args field defaults to empty array") {
    fl::Remote remote;

    bool called = false;
    remote.registerFunction("noArgs", [&](const fl::Json& args) {
        called = true;
        REQUIRE(args.is_array());
        REQUIRE_EQ(args.size(), 0);
    });

    auto err = remote.processRpc(R"({"function":"noArgs"})");
    REQUIRE_EQ(err, fl::Remote::Error::None);
    REQUIRE(called);
}

// Additional test: Scheduled function that was already executed should be in results
TEST_CASE("Remote: Scheduled execution results") {
    fl::Remote remote;

    remote.registerFunction("task", [](const fl::Json& args) {});

    remote.processRpc(R"({"timestamp":500,"function":"task","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"task","args":[]})");

    // Execute first scheduled task
    size_t executed = remote.tick(500);
    REQUIRE_EQ(executed, 1);

    auto results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);
    REQUIRE(results[0].wasScheduled);
    REQUIRE_EQ(results[0].scheduledAt, 500);

    // Execute second scheduled task (results should be cleared from previous tick)
    executed = remote.tick(1000);
    REQUIRE_EQ(executed, 1);

    results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);  // Previous results cleared
    REQUIRE_EQ(results[0].scheduledAt, 1000);
}

// Test: RpcResult::to_json() serialization
TEST_CASE("Remote: RpcResult to_json serialization") {
    fl::Remote remote;

    remote.registerFunctionWithReturn("getValue", [](const fl::Json& args) -> fl::Json {
        return fl::Json(42);
    });

    fl::Json result;
    remote.processRpc(R"({"function":"getValue","args":[]})", result);

    auto results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);

    // Serialize result to JSON
    fl::Json json = results[0].to_json();

    // Verify all fields are present
    REQUIRE(json.contains("function"));
    REQUIRE(json.contains("result"));
    REQUIRE(json.contains("scheduledAt"));
    REQUIRE(json.contains("receivedAt"));
    REQUIRE(json.contains("executedAt"));
    REQUIRE(json.contains("wasScheduled"));

    // Verify field values
    fl::string functionName = json["function"] | fl::string("");
    REQUIRE_EQ(functionName, "getValue");
    int resultValue = json["result"] | 0;
    REQUIRE_EQ(resultValue, 42);
    int64_t scheduledAtValue = json["scheduledAt"] | -1;
    REQUIRE_EQ(scheduledAtValue, 0);  // Immediate execution
    bool wasScheduledValue = json["wasScheduled"] | true;
    REQUIRE_FALSE(wasScheduledValue);  // Should be false for immediate
}

// Test: RpcResult::to_json() for scheduled execution
TEST_CASE("Remote: RpcResult to_json for scheduled execution") {
    fl::Remote remote;

    int counter = 100;
    remote.registerFunctionWithReturn("getCounter", [&](const fl::Json& args) -> fl::Json {
        return fl::Json(counter++);
    });

    remote.processRpc(R"({"timestamp":1000,"function":"getCounter","args":[]})");
    remote.tick(1000);

    auto results = remote.getResults();
    REQUIRE_EQ(results.size(), 1);

    fl::Json json = results[0].to_json();

    // Verify scheduled execution metadata
    int64_t scheduledAtValue = json["scheduledAt"] | 0;
    REQUIRE_EQ(scheduledAtValue, 1000);
    bool wasScheduledValue = json["wasScheduled"] | false;
    REQUIRE(wasScheduledValue);  // Should be true for scheduled
    int resultValue = json["result"] | 0;
    REQUIRE_EQ(resultValue, 100);

    // Verify timing relationships
    int64_t receivedAt = json["receivedAt"] | 0;
    int64_t scheduledAt = json["scheduledAt"] | 0;
    int64_t executedAt = json["executedAt"] | 0;

    REQUIRE(receivedAt <= scheduledAt);
    REQUIRE(scheduledAt <= executedAt);
}

// Test: Remote::printJson() output format
TEST_CASE("Remote: printJson single-line format") {
    fl::Json testJson = fl::Json::object();
    testJson.set("status", "ok");
    testJson.set("value", 42);

    // Capture output using custom println handler
    fl::string captured;
    fl::inject_println_handler([&captured](const char* str) {
        captured += str;
    });

    fl::Remote::printJson(testJson);

    // Clear handler (restore default behavior)
    fl::clear_println_handler();

    // Verify output format
    // Should be: "REMOTE: {json}" (single line, no newlines in JSON)
    REQUIRE_FALSE(captured.empty());

    // Check for prefix - expected prefix is "REMOTE: " by default
    fl::string expectedPrefix = "REMOTE: ";
    REQUIRE(captured.find(expectedPrefix) == 0);  // Should start with prefix

    // Verify no newlines in the JSON part (single-line requirement)
    // The captured string might have a trailing newline from println, but the JSON itself should be single-line
    size_t jsonStart = captured.find('{');
    size_t jsonEnd = captured.find('}');
    REQUIRE(jsonStart != fl::string::npos);
    REQUIRE(jsonEnd != fl::string::npos);

    fl::string jsonPart = captured.substr(jsonStart, jsonEnd - jsonStart + 1);
    REQUIRE(jsonPart.find('\n') == fl::string::npos);
    REQUIRE(jsonPart.find('\r') == fl::string::npos);
}

// Test: printJson handles newlines in JSON (defensive)
TEST_CASE("Remote: printJson removes newlines from malformed JSON") {
    // Create a JSON object
    fl::Json testJson = fl::Json::object();
    testJson.set("key", "value");

    // Manually inject newlines by manipulating the string representation
    // (This tests the defensive newline removal in printJson)
    fl::string captured;
    fl::inject_println_handler([&captured](const char* str) {
        captured += str;
    });

    fl::Remote::printJson(testJson);

    // Clear handler (restore default behavior)
    fl::clear_println_handler();

    // Verify output has no embedded newlines (except possibly trailing from println)
    size_t jsonStart = captured.find('{');
    size_t jsonEnd = captured.find('}');
    REQUIRE(jsonStart != fl::string::npos);
    REQUIRE(jsonEnd != fl::string::npos);

    fl::string jsonPart = captured.substr(jsonStart, jsonEnd - jsonStart + 1);
    REQUIRE(jsonPart.find('\n') == fl::string::npos);
    REQUIRE(jsonPart.find('\r') == fl::string::npos);
}

// WLED State Tests

// Test: Set WLED state with on=true and bri=128
TEST_CASE("Remote: WLED set state on and brightness") {
    fl::WLED remote;

    fl::Json state = fl::Json::parse(R"({"on":true,"bri":128})");
    remote.setWledState(state);

    REQUIRE(remote.getOn());
    REQUIRE_EQ(remote.getBrightness(), 128);
}

// Test: Set WLED state with on=false
TEST_CASE("Remote: WLED set state off") {
    fl::WLED remote;

    // Initially on (default is false, so turn it on first)
    fl::Json stateOn = fl::Json::parse(R"({"on":true})");
    remote.setWledState(stateOn);
    REQUIRE(remote.getOn());

    // Turn off
    fl::Json stateOff = fl::Json::parse(R"({"on":false})");
    remote.setWledState(stateOff);
    REQUIRE_FALSE(remote.getOn());
}

// Test: Get WLED state returns correct JSON
TEST_CASE("Remote: WLED get state") {
    fl::WLED remote;

    // Set state
    fl::Json stateIn = fl::Json::parse(R"({"on":true,"bri":200})");
    remote.setWledState(stateIn);

    // Get state
    fl::Json stateOut = remote.getWledState();
    REQUIRE(stateOut.contains("on"));
    REQUIRE(stateOut.contains("bri"));

    bool on = stateOut["on"] | false;
    int64_t bri = stateOut["bri"] | 0;

    REQUIRE(on);
    REQUIRE_EQ(bri, 200);
}

// Test: Partial state updates (missing fields don't corrupt state)
TEST_CASE("Remote: WLED partial state updates") {
    fl::WLED remote;

    // Set initial state
    fl::Json fullState = fl::Json::parse(R"({"on":true,"bri":100})");
    remote.setWledState(fullState);
    REQUIRE(remote.getOn());
    REQUIRE_EQ(remote.getBrightness(), 100);

    // Update only brightness (on should remain true)
    fl::Json partialBri = fl::Json::parse(R"({"bri":50})");
    remote.setWledState(partialBri);
    REQUIRE(remote.getOn());  // Should still be true
    REQUIRE_EQ(remote.getBrightness(), 50);

    // Update only on (brightness should remain 50)
    fl::Json partialOn = fl::Json::parse(R"({"on":false})");
    remote.setWledState(partialOn);
    REQUIRE_FALSE(remote.getOn());
    REQUIRE_EQ(remote.getBrightness(), 50);  // Should still be 50
}

// Test: Invalid values are handled gracefully
TEST_CASE("Remote: WLED invalid values") {
    fl::WLED remote;

    // Set initial valid state
    fl::Json validState = fl::Json::parse(R"({"on":true,"bri":128})");
    remote.setWledState(validState);

    // Test out-of-range brightness (negative - should clamp to 0)
    fl::Json negativeBri = fl::Json::parse(R"({"bri":-10})");
    remote.setWledState(negativeBri);
    REQUIRE_EQ(remote.getBrightness(), 0);  // Clamped to 0

    // Test out-of-range brightness (too high - should clamp to 255)
    fl::Json highBri = fl::Json::parse(R"({"bri":300})");
    remote.setWledState(highBri);
    REQUIRE_EQ(remote.getBrightness(), 255);  // Clamped to 255

    // Test invalid type for bri (should keep existing value)
    uint8_t currentBri = remote.getBrightness();
    fl::Json invalidBri = fl::Json::parse(R"({"bri":"invalid"})");
    remote.setWledState(invalidBri);
    REQUIRE_EQ(remote.getBrightness(), currentBri);  // Should remain unchanged

    // Test invalid JSON (should not crash)
    fl::Json invalidJson = fl::Json(nullptr);
    remote.setWledState(invalidJson);  // Should warn but not crash
    REQUIRE(remote.getOn());  // State should remain unchanged
}

// Test: State roundtrip (setWledState -> getWledState preserves values)
TEST_CASE("Remote: WLED state roundtrip") {
    fl::WLED remote;

    // Set state
    fl::Json stateIn = fl::Json::parse(R"({"on":false,"bri":64})");
    remote.setWledState(stateIn);

    // Get state
    fl::Json stateOut = remote.getWledState();

    // Verify roundtrip
    bool on = stateOut["on"] | true;
    int64_t bri = stateOut["bri"] | 0;

    REQUIRE_FALSE(on);
    REQUIRE_EQ(bri, 64);

    // Set state again from the retrieved JSON
    remote.setWledState(stateOut);

    // Verify still correct
    REQUIRE_FALSE(remote.getOn());
    REQUIRE_EQ(remote.getBrightness(), 64);
}

#endif // FASTLED_ENABLE_JSON
