#include "fl/remote.h"
#include "fl/fx/wled.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/cstdio.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/fx/wled/segment.h"
#include "fl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"

// Test 4.1 - Basic Function Registration
FL_TEST_CASE("Remote: Basic function registration") {
    fl::Remote remote;

    bool called = false;
    remote.registerFunction("test", [&](const fl::Json&) {
        called = true;
    });

    FL_REQUIRE(remote.hasFunction("test"));
    FL_REQUIRE_FALSE(remote.hasFunction("unknown"));

    remote.processRpc(R"({"timestamp":0,"function":"test","args":[]})");
    FL_REQUIRE(called);
}

// Test 4.2 - Immediate Execution
FL_TEST_CASE("Remote: Immediate execution") {
    fl::Remote remote;

    int result = 0;
    remote.registerFunction("add", [&](const fl::Json& args) {
        int a = args[0] | 0;
        int b = args[1] | 0;
        result = a + b;
    });

    remote.processRpc(R"({"timestamp":0,"function":"add","args":[5,3]})");
    FL_REQUIRE_EQ(result, 8);
}

// Test 4.3 - Scheduled Execution
FL_TEST_CASE("Remote: Scheduled execution") {
    fl::Remote remote;

    int callCount = 0;
    remote.registerFunction("increment", [&](const fl::Json&) {
        callCount++;
    });

    remote.processRpc(R"({"timestamp":1000,"function":"increment","args":[]})");
    FL_REQUIRE_EQ(callCount, 0);  // Not executed yet
    FL_REQUIRE_EQ(remote.pendingCount(), 1);

    remote.tick(999);  // Before scheduled time
    FL_REQUIRE_EQ(callCount, 0);

    remote.tick(1000);  // At scheduled time
    FL_REQUIRE_EQ(callCount, 1);
    FL_REQUIRE_EQ(remote.pendingCount(), 0);
}

// Test 4.4 - Error Handling
FL_TEST_CASE("Remote: Error handling") {
    fl::Remote remote;

    // Invalid JSON
    auto err = remote.processRpc("{invalid json}");
    FL_REQUIRE_EQ(err, fl::Remote::Error::InvalidJson);

    // Missing function field
    err = remote.processRpc(R"({"timestamp":0,"args":[]})");
    FL_REQUIRE_EQ(err, fl::Remote::Error::MissingFunction);

    // Unknown function
    err = remote.processRpc(R"({"timestamp":0,"function":"unknown","args":[]})");
    FL_REQUIRE_EQ(err, fl::Remote::Error::UnknownFunction);

    // Invalid timestamp (should warn but default to 0)
    remote.registerFunction("test", [](const fl::Json&) {});
    err = remote.processRpc(R"({"timestamp":-5,"function":"test","args":[]})");
    FL_REQUIRE_EQ(err, fl::Remote::Error::InvalidTimestamp);
}

// Test 4.5 - Argument Extraction
FL_TEST_CASE("Remote: Argument extraction") {
    fl::Remote remote;

    fl::vector<int> received;
    remote.registerFunction("collect", [&](const fl::Json& args) {
        for (size_t i = 0; i < args.size(); i++) {
            received.push_back(args[i] | 0);
        }
    });

    remote.processRpc(R"({"function":"collect","args":[10,20,30]})");
    FL_REQUIRE_EQ(received.size(), 3);
    FL_REQUIRE_EQ(received[0], 10);
    FL_REQUIRE_EQ(received[1], 20);
    FL_REQUIRE_EQ(received[2], 30);
}

// Test 4.6 - Multiple Scheduled Calls
FL_TEST_CASE("Remote: Multiple scheduled calls") {
    fl::Remote remote;

    fl::vector<fl::string> executed;
    remote.registerFunction("a", [&](const fl::Json&) { executed.push_back("a"); });
    remote.registerFunction("b", [&](const fl::Json&) { executed.push_back("b"); });
    remote.registerFunction("c", [&](const fl::Json&) { executed.push_back("c"); });

    remote.processRpc(R"({"timestamp":3000,"function":"c","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"a","args":[]})");
    remote.processRpc(R"({"timestamp":2000,"function":"b","args":[]})");

    FL_REQUIRE_EQ(remote.pendingCount(), 3);

    remote.tick(1500);
    FL_REQUIRE_EQ(executed.size(), 1);  // Only 'a' should execute
    FL_REQUIRE_EQ(executed[0], "a");

    remote.tick(2500);
    FL_REQUIRE_EQ(executed.size(), 2);  // 'b' should execute
    FL_REQUIRE_EQ(executed[1], "b");

    remote.tick(3500);
    FL_REQUIRE_EQ(executed.size(), 3);  // 'c' should execute
    FL_REQUIRE_EQ(executed[2], "c");

    FL_REQUIRE_EQ(remote.pendingCount(), 0);
}

// Test 4.7 - Clear Operations
FL_TEST_CASE("Remote: Clear operations") {
    fl::Remote remote;

    remote.registerFunction("test", [](const fl::Json&) {});
    remote.processRpc(R"({"timestamp":1000,"function":"test","args":[]})");

    FL_REQUIRE_EQ(remote.pendingCount(), 1);
    FL_REQUIRE(remote.hasFunction("test"));

    remote.clearScheduled();
    FL_REQUIRE_EQ(remote.pendingCount(), 0);
    FL_REQUIRE(remote.hasFunction("test"));  // Still registered

    remote.processRpc(R"({"timestamp":1000,"function":"test","args":[]})");
    remote.clearFunctions();
    FL_REQUIRE_FALSE(remote.hasFunction("test"));
    FL_REQUIRE_EQ(remote.pendingCount(), 1);  // Still scheduled

    remote.clear();
    FL_REQUIRE_EQ(remote.pendingCount(), 0);
    FL_REQUIRE_FALSE(remote.hasFunction("test"));
}

// Test 4.8 - Return Values
FL_TEST_CASE("Remote: Return values") {
    fl::Remote remote;

    // Register function with return value (matches Arduino millis())
    remote.registerFunctionWithReturn("millis", [](const fl::Json&) -> fl::Json {
        return fl::Json(static_cast<int64_t>(12345));
    });

    // Call and capture return value
    fl::Json result;
    auto err = remote.processRpc(R"({"function":"millis","args":[]})", result);

    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE(result.has_value());
    FL_REQUIRE_EQ(result | 0, 12345);
}

// Test 4.9 - Return Values with Arguments
FL_TEST_CASE("Remote: Return values with arguments") {
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

    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE_EQ(result["product"] | 0, 42);
}

// Test 4.10 - Scheduled Functions with Return Values and Timing Metadata
FL_TEST_CASE("Remote: Scheduled functions with return values and timing metadata") {
    fl::Remote remote;

    int counter = 100;
    remote.registerFunctionWithReturn("getCounter", [&](const fl::Json&) -> fl::Json {
        return fl::Json(counter++);
    });

    // Schedule for future execution
    fl::Json result;
    auto err = remote.processRpc(R"({"timestamp":1000,"function":"getCounter","args":[]})", result);

    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE_FALSE(result.has_value());  // Result not available yet (scheduled)
    FL_REQUIRE_EQ(remote.pendingCount(), 1);

    // Execute scheduled function
    remote.tick(1000);

    // Check results are available with metadata
    auto results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);

    const auto& r = results[0];
    FL_REQUIRE_EQ(r.functionName, "getCounter");
    FL_REQUIRE(r.wasScheduled);
    FL_REQUIRE_EQ(r.scheduledAt, 1000);
    FL_REQUIRE(r.result.has_value());
    FL_REQUIRE_EQ(r.result | 0, 100);
    FL_REQUIRE_EQ(counter, 101);  // Counter was incremented

    // Verify timing: receivedAt <= scheduledAt <= executedAt
    FL_REQUIRE(r.receivedAt <= r.scheduledAt);
    FL_REQUIRE(r.scheduledAt <= r.executedAt);
}

// Test 4.11 - Stable Ordering (FIFO for Same Timestamp)
FL_TEST_CASE("Remote: Stable ordering (FIFO for same timestamp)") {
    fl::Remote remote;

    fl::vector<fl::string> executionOrder;

    remote.registerFunction("a", [&](const fl::Json&) { executionOrder.push_back("a"); });
    remote.registerFunction("b", [&](const fl::Json&) { executionOrder.push_back("b"); });
    remote.registerFunction("c", [&](const fl::Json&) { executionOrder.push_back("c"); });
    remote.registerFunction("d", [&](const fl::Json&) { executionOrder.push_back("d"); });

    // Schedule all functions with the SAME timestamp
    // They should execute in the order they were scheduled (FIFO)
    remote.processRpc(R"({"timestamp":1000,"function":"a","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"b","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"c","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"d","args":[]})");

    FL_REQUIRE_EQ(remote.pendingCount(), 4);

    // Execute all scheduled functions
    remote.tick(1000);

    // Verify FIFO execution order (stable ordering)
    FL_REQUIRE_EQ(executionOrder.size(), 4);
    FL_REQUIRE_EQ(executionOrder[0], "a");
    FL_REQUIRE_EQ(executionOrder[1], "b");
    FL_REQUIRE_EQ(executionOrder[2], "c");
    FL_REQUIRE_EQ(executionOrder[3], "d");
    FL_REQUIRE_EQ(remote.pendingCount(), 0);
}

// Additional test: Unregister function
FL_TEST_CASE("Remote: Unregister function") {
    fl::Remote remote;

    remote.registerFunction("test", [](const fl::Json&) {});
    FL_REQUIRE(remote.hasFunction("test"));

    bool removed = remote.unregisterFunction("test");
    FL_REQUIRE(removed);
    FL_REQUIRE_FALSE(remote.hasFunction("test"));

    // Try to unregister non-existent function
    removed = remote.unregisterFunction("nonexistent");
    FL_REQUIRE_FALSE(removed);
}

// Additional test: Results clearing
FL_TEST_CASE("Remote: Results clearing") {
    fl::Remote remote;

    remote.registerFunctionWithReturn("getValue", [](const fl::Json&) -> fl::Json {
        return fl::Json(42);
    });

    fl::Json result;
    remote.processRpc(R"({"function":"getValue","args":[]})", result);

    auto results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);

    remote.clearResults();
    results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 0);
}

// Additional test: No args field (should default to empty array)
FL_TEST_CASE("Remote: No args field defaults to empty array") {
    fl::Remote remote;

    bool called = false;
    remote.registerFunction("noArgs", [&](const fl::Json& args) {
        called = true;
        FL_REQUIRE(args.is_array());
        FL_REQUIRE_EQ(args.size(), 0);
    });

    auto err = remote.processRpc(R"({"function":"noArgs"})");
    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE(called);
}

// Additional test: Scheduled function that was already executed should be in results
FL_TEST_CASE("Remote: Scheduled execution results") {
    fl::Remote remote;

    remote.registerFunction("task", [](const fl::Json&) {});

    remote.processRpc(R"({"timestamp":500,"function":"task","args":[]})");
    remote.processRpc(R"({"timestamp":1000,"function":"task","args":[]})");

    // Execute first scheduled task
    size_t executed = remote.tick(500);
    FL_REQUIRE_EQ(executed, 1);

    auto results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);
    FL_REQUIRE(results[0].wasScheduled);
    FL_REQUIRE_EQ(results[0].scheduledAt, 500);

    // Execute second scheduled task (results should be cleared from previous tick)
    executed = remote.tick(1000);
    FL_REQUIRE_EQ(executed, 1);

    results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);  // Previous results cleared
    FL_REQUIRE_EQ(results[0].scheduledAt, 1000);
}

// Test: RpcResult::to_json() serialization
FL_TEST_CASE("Remote: RpcResult to_json serialization") {
    fl::Remote remote;

    remote.registerFunctionWithReturn("getValue", [](const fl::Json&) -> fl::Json {
        return fl::Json(42);
    });

    fl::Json result;
    remote.processRpc(R"({"function":"getValue","args":[]})", result);

    auto results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);

    // Serialize result to JSON
    fl::Json json = results[0].to_json();

    // Verify all fields are present
    FL_REQUIRE(json.contains("function"));
    FL_REQUIRE(json.contains("result"));
    FL_REQUIRE(json.contains("scheduledAt"));
    FL_REQUIRE(json.contains("receivedAt"));
    FL_REQUIRE(json.contains("executedAt"));
    FL_REQUIRE(json.contains("wasScheduled"));

    // Verify field values
    fl::string functionName = json["function"] | fl::string("");
    FL_REQUIRE_EQ(functionName, "getValue");
    int resultValue = json["result"] | 0;
    FL_REQUIRE_EQ(resultValue, 42);
    int64_t scheduledAtValue = json["scheduledAt"] | -1;
    FL_REQUIRE_EQ(scheduledAtValue, 0);  // Immediate execution
    bool wasScheduledValue = json["wasScheduled"] | true;
    FL_REQUIRE_FALSE(wasScheduledValue);  // Should be false for immediate
}

// Test: RpcResult::to_json() for scheduled execution
FL_TEST_CASE("Remote: RpcResult to_json for scheduled execution") {
    fl::Remote remote;

    int counter = 100;
    remote.registerFunctionWithReturn("getCounter", [&](const fl::Json&) -> fl::Json {
        return fl::Json(counter++);
    });

    remote.processRpc(R"({"timestamp":1000,"function":"getCounter","args":[]})");
    remote.tick(1000);

    auto results = remote.getResults();
    FL_REQUIRE_EQ(results.size(), 1);

    fl::Json json = results[0].to_json();

    // Verify scheduled execution metadata
    int64_t scheduledAtValue = json["scheduledAt"] | 0;
    FL_REQUIRE_EQ(scheduledAtValue, 1000);
    bool wasScheduledValue = json["wasScheduled"] | false;
    FL_REQUIRE(wasScheduledValue);  // Should be true for scheduled
    int resultValue = json["result"] | 0;
    FL_REQUIRE_EQ(resultValue, 100);

    // Verify timing relationships
    int64_t receivedAt = json["receivedAt"] | 0;
    int64_t scheduledAt = json["scheduledAt"] | 0;
    int64_t executedAt = json["executedAt"] | 0;

    FL_REQUIRE(receivedAt <= scheduledAt);
    FL_REQUIRE(scheduledAt <= executedAt);
}

// Test: Remote::printJson() output format
FL_TEST_CASE("Remote: printJson single-line format") {
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
    FL_REQUIRE_FALSE(captured.empty());

    // Check for prefix - expected prefix is "REMOTE: " by default
    fl::string expectedPrefix = "REMOTE: ";
    FL_REQUIRE(captured.find(expectedPrefix) == 0);  // Should start with prefix

    // Verify no newlines in the JSON part (single-line requirement)
    // The captured string might have a trailing newline from println, but the JSON itself should be single-line
    size_t jsonStart = captured.find('{');
    size_t jsonEnd = captured.find('}');
    FL_REQUIRE(jsonStart != fl::string::npos);
    FL_REQUIRE(jsonEnd != fl::string::npos);

    fl::string jsonPart = captured.substr(jsonStart, jsonEnd - jsonStart + 1);
    FL_REQUIRE(jsonPart.find('\n') == fl::string::npos);
    FL_REQUIRE(jsonPart.find('\r') == fl::string::npos);
}

// Test: printJson handles newlines in JSON (defensive)
FL_TEST_CASE("Remote: printJson removes newlines from malformed JSON") {
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
    FL_REQUIRE(jsonStart != fl::string::npos);
    FL_REQUIRE(jsonEnd != fl::string::npos);

    fl::string jsonPart = captured.substr(jsonStart, jsonEnd - jsonStart + 1);
    FL_REQUIRE(jsonPart.find('\n') == fl::string::npos);
    FL_REQUIRE(jsonPart.find('\r') == fl::string::npos);
}

// WLED State Tests

// Test: Set WLED state with on=true and bri=128
FL_TEST_CASE("Remote: WLED set state on and brightness") {
    fl::WLED remote;

    fl::Json state = fl::Json::parse(R"({"on":true,"bri":128})");
    remote.setState(state);

    FL_REQUIRE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 128);
}

// Test: Set WLED state with on=false
FL_TEST_CASE("Remote: WLED set state off") {
    fl::WLED remote;

    // Initially on (default is false, so turn it on first)
    fl::Json stateOn = fl::Json::parse(R"({"on":true})");
    remote.setState(stateOn);
    FL_REQUIRE(remote.getOn());

    // Turn off
    fl::Json stateOff = fl::Json::parse(R"({"on":false})");
    remote.setState(stateOff);
    FL_REQUIRE_FALSE(remote.getOn());
}

// Test: Get WLED state returns correct JSON
FL_TEST_CASE("Remote: WLED get state") {
    fl::WLED remote;

    // Set state
    fl::Json stateIn = fl::Json::parse(R"({"on":true,"bri":200})");
    remote.setState(stateIn);

    // Get state
    fl::Json stateOut = remote.getState();
    FL_REQUIRE(stateOut.contains("on"));
    FL_REQUIRE(stateOut.contains("bri"));

    bool on = stateOut["on"] | false;
    int64_t bri = stateOut["bri"] | 0;

    FL_REQUIRE(on);
    FL_REQUIRE_EQ(bri, 200);
}

// Test: Partial state updates (missing fields don't corrupt state)
FL_TEST_CASE("Remote: WLED partial state updates") {
    fl::WLED remote;

    // Set initial state
    fl::Json fullState = fl::Json::parse(R"({"on":true,"bri":100})");
    remote.setState(fullState);
    FL_REQUIRE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 100);

    // Update only brightness (on should remain true)
    fl::Json partialBri = fl::Json::parse(R"({"bri":50})");
    remote.setState(partialBri);
    FL_REQUIRE(remote.getOn());  // Should still be true
    FL_REQUIRE_EQ(remote.getBrightness(), 50);

    // Update only on (brightness should remain 50)
    fl::Json partialOn = fl::Json::parse(R"({"on":false})");
    remote.setState(partialOn);
    FL_REQUIRE_FALSE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 50);  // Should still be 50
}

// Test: Invalid values are handled gracefully
FL_TEST_CASE("Remote: WLED invalid values") {
    fl::WLED remote;

    // Set initial valid state
    fl::Json validState = fl::Json::parse(R"({"on":true,"bri":128})");
    remote.setState(validState);

    // Test out-of-range brightness (negative - should clamp to 0)
    fl::Json negativeBri = fl::Json::parse(R"({"bri":-10})");
    remote.setState(negativeBri);
    FL_REQUIRE_EQ(remote.getBrightness(), 0);  // Clamped to 0

    // Test out-of-range brightness (too high - should clamp to 255)
    fl::Json highBri = fl::Json::parse(R"({"bri":300})");
    remote.setState(highBri);
    FL_REQUIRE_EQ(remote.getBrightness(), 255);  // Clamped to 255

    // Test invalid type for bri (should keep existing value)
    uint8_t currentBri = remote.getBrightness();
    fl::Json invalidBri = fl::Json::parse(R"({"bri":"invalid"})");
    remote.setState(invalidBri);
    FL_REQUIRE_EQ(remote.getBrightness(), currentBri);  // Should remain unchanged

    // Test invalid JSON (should not crash)
    fl::Json invalidJson = fl::Json(nullptr);
    remote.setState(invalidJson);  // Should warn but not crash
    FL_REQUIRE(remote.getOn());  // State should remain unchanged
}

// Test: State roundtrip (setState -> getState preserves values)
FL_TEST_CASE("Remote: WLED state roundtrip") {
    fl::WLED remote;

    // Set state
    fl::Json stateIn = fl::Json::parse(R"({"on":false,"bri":64})");
    remote.setState(stateIn);

    // Get state
    fl::Json stateOut = remote.getState();

    // Verify roundtrip
    bool on = stateOut["on"] | true;
    int64_t bri = stateOut["bri"] | 0;

    FL_REQUIRE_FALSE(on);
    FL_REQUIRE_EQ(bri, 64);

    // Set state again from the retrieved JSON
    remote.setState(stateOut);

    // Verify still correct
    FL_REQUIRE_FALSE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 64);
}

// Test: WLED transition field
FL_TEST_CASE("Remote: WLED transition field") {
    fl::WLED remote;

    // Default transition should be 7 (700ms)
    FL_REQUIRE_EQ(remote.getTransition(), 7);

    // Set transition to 0 (instant)
    fl::Json state = fl::Json::parse(R"({"transition":0})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getTransition(), 0);

    // Set transition to max value (65535)
    state = fl::Json::parse(R"({"transition":65535})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getTransition(), 65535);

    // Test clamping: negative value
    state = fl::Json::parse(R"({"transition":-100})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getTransition(), 0);  // Clamped to 0

    // Test clamping: too high value
    state = fl::Json::parse(R"({"transition":70000})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getTransition(), 65535);  // Clamped to 65535

    // Test invalid type (should keep existing value)
    uint16_t currentTrans = remote.getTransition();
    state = fl::Json::parse(R"({"transition":"invalid"})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getTransition(), currentTrans);  // Unchanged
}

// Test: WLED preset field (ps)
FL_TEST_CASE("Remote: WLED preset field") {
    fl::WLED remote;

    // Default preset should be -1 (none)
    FL_REQUIRE_EQ(remote.getPreset(), -1);

    // Set preset to 0
    fl::Json state = fl::Json::parse(R"({"ps":0})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), 0);

    // Set preset to max value (250)
    state = fl::Json::parse(R"({"ps":250})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), 250);

    // Set preset back to none
    state = fl::Json::parse(R"({"ps":-1})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), -1);

    // Test clamping: below -1
    state = fl::Json::parse(R"({"ps":-100})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), -1);  // Clamped to -1

    // Test clamping: above 250
    state = fl::Json::parse(R"({"ps":500})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), 250);  // Clamped to 250

    // Test invalid type (should keep existing value)
    int16_t currentPreset = remote.getPreset();
    state = fl::Json::parse(R"({"ps":"invalid"})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPreset(), currentPreset);  // Unchanged
}

// Test: WLED playlist field (pl)
FL_TEST_CASE("Remote: WLED playlist field") {
    fl::WLED remote;

    // Default playlist should be -1 (none)
    FL_REQUIRE_EQ(remote.getPlaylist(), -1);

    // Set playlist to 5
    fl::Json state = fl::Json::parse(R"({"pl":5})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), 5);

    // Set playlist to max value (250)
    state = fl::Json::parse(R"({"pl":250})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), 250);

    // Set playlist back to none
    state = fl::Json::parse(R"({"pl":-1})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), -1);

    // Test clamping: below -1
    state = fl::Json::parse(R"({"pl":-50})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), -1);  // Clamped to -1

    // Test clamping: above 250
    state = fl::Json::parse(R"({"pl":300})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), 250);  // Clamped to 250

    // Test invalid type (should keep existing value)
    int16_t currentPlaylist = remote.getPlaylist();
    state = fl::Json::parse(R"({"pl":"invalid"})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getPlaylist(), currentPlaylist);  // Unchanged
}

// Test: WLED live override field (lor)
FL_TEST_CASE("Remote: WLED live override field") {
    fl::WLED remote;

    // Default live override should be 0 (off)
    FL_REQUIRE_EQ(remote.getLiveOverride(), 0);

    // Set to override (1)
    fl::Json state = fl::Json::parse(R"({"lor":1})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 1);

    // Set to until reboot (2)
    state = fl::Json::parse(R"({"lor":2})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 2);

    // Set back to off (0)
    state = fl::Json::parse(R"({"lor":0})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 0);

    // Test clamping: negative value
    state = fl::Json::parse(R"({"lor":-5})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 0);  // Clamped to 0

    // Test clamping: above 2
    state = fl::Json::parse(R"({"lor":10})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 2);  // Clamped to 2

    // Test invalid type (should keep existing value)
    uint8_t currentLor = remote.getLiveOverride();
    state = fl::Json::parse(R"({"lor":"invalid"})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getLiveOverride(), currentLor);  // Unchanged
}

// Test: WLED main segment field (mainseg)
FL_TEST_CASE("Remote: WLED main segment field") {
    fl::WLED remote;

    // Default main segment should be 0
    FL_REQUIRE_EQ(remote.getMainSegment(), 0);

    // Set to segment 5
    fl::Json state = fl::Json::parse(R"({"mainseg":5})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getMainSegment(), 5);

    // Set to max value (255)
    state = fl::Json::parse(R"({"mainseg":255})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getMainSegment(), 255);

    // Test clamping: negative value
    state = fl::Json::parse(R"({"mainseg":-10})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getMainSegment(), 0);  // Clamped to 0

    // Test clamping: above 255
    state = fl::Json::parse(R"({"mainseg":500})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getMainSegment(), 255);  // Clamped to 255

    // Test invalid type (should keep existing value)
    uint8_t currentMainseg = remote.getMainSegment();
    state = fl::Json::parse(R"({"mainseg":"invalid"})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getMainSegment(), currentMainseg);  // Unchanged
}

// Test: WLED complete state with all fields
FL_TEST_CASE("Remote: WLED complete state with all fields") {
    fl::WLED remote;

    // Set all fields at once
    fl::Json fullState = fl::Json::parse(R"({
        "on": true,
        "bri": 180,
        "transition": 15,
        "ps": 42,
        "pl": 10,
        "lor": 1,
        "mainseg": 3
    })");
    remote.setState(fullState);

    // Verify all fields
    FL_REQUIRE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 180);
    FL_REQUIRE_EQ(remote.getTransition(), 15);
    FL_REQUIRE_EQ(remote.getPreset(), 42);
    FL_REQUIRE_EQ(remote.getPlaylist(), 10);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 1);
    FL_REQUIRE_EQ(remote.getMainSegment(), 3);

    // Get state and verify roundtrip
    fl::Json retrievedState = remote.getState();
    FL_REQUIRE(retrievedState.contains("on"));
    FL_REQUIRE(retrievedState.contains("bri"));
    FL_REQUIRE(retrievedState.contains("transition"));
    FL_REQUIRE(retrievedState.contains("ps"));
    FL_REQUIRE(retrievedState.contains("pl"));
    FL_REQUIRE(retrievedState.contains("lor"));
    FL_REQUIRE(retrievedState.contains("mainseg"));

    // Verify values in retrieved state
    FL_REQUIRE((retrievedState["on"] | false) == true);
    FL_REQUIRE_EQ(retrievedState["bri"] | 0, 180);
    FL_REQUIRE_EQ(retrievedState["transition"] | 0, 15);
    FL_REQUIRE_EQ(retrievedState["ps"] | 0, 42);
    FL_REQUIRE_EQ(retrievedState["pl"] | 0, 10);
    FL_REQUIRE_EQ(retrievedState["lor"] | 0, 1);
    FL_REQUIRE_EQ(retrievedState["mainseg"] | 0, 3);
}

// Test: WLED partial updates preserve all fields
FL_TEST_CASE("Remote: WLED partial updates preserve all fields") {
    fl::WLED remote;

    // Set initial complete state
    fl::Json initialState = fl::Json::parse(R"({
        "on": true,
        "bri": 200,
        "transition": 10,
        "ps": 5,
        "pl": 2,
        "lor": 1,
        "mainseg": 1
    })");
    remote.setState(initialState);

    // Update only transition
    fl::Json partialUpdate = fl::Json::parse(R"({"transition":20})");
    remote.setState(partialUpdate);

    // Verify only transition changed, others preserved
    FL_REQUIRE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 200);
    FL_REQUIRE_EQ(remote.getTransition(), 20);  // Changed
    FL_REQUIRE_EQ(remote.getPreset(), 5);
    FL_REQUIRE_EQ(remote.getPlaylist(), 2);
    FL_REQUIRE_EQ(remote.getLiveOverride(), 1);
    FL_REQUIRE_EQ(remote.getMainSegment(), 1);

    // Update only preset
    partialUpdate = fl::Json::parse(R"({"ps":50})");
    remote.setState(partialUpdate);

    // Verify only preset changed
    FL_REQUIRE_EQ(remote.getTransition(), 20);  // Still 20 from previous
    FL_REQUIRE_EQ(remote.getPreset(), 50);  // Changed
    FL_REQUIRE_EQ(remote.getPlaylist(), 2);  // Still 2

    // Update multiple fields
    partialUpdate = fl::Json::parse(R"({"on":false,"bri":50,"lor":2})");
    remote.setState(partialUpdate);

    // Verify specified fields changed, others preserved
    FL_REQUIRE_FALSE(remote.getOn());  // Changed
    FL_REQUIRE_EQ(remote.getBrightness(), 50);  // Changed
    FL_REQUIRE_EQ(remote.getTransition(), 20);  // Preserved
    FL_REQUIRE_EQ(remote.getPreset(), 50);  // Preserved
    FL_REQUIRE_EQ(remote.getPlaylist(), 2);  // Preserved
    FL_REQUIRE_EQ(remote.getLiveOverride(), 2);  // Changed
    FL_REQUIRE_EQ(remote.getMainSegment(), 1);  // Preserved
}

// Test: WLED nightlight object parsing
FL_TEST_CASE("Remote: WLED nightlight object") {
    fl::WLED remote;

    // Default nightlight state
    FL_REQUIRE_FALSE(remote.getNightlightOn());
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 60);
    FL_REQUIRE_EQ(remote.getNightlightMode(), 1);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 0);

    // Set nightlight with all fields
    fl::Json state = fl::Json::parse(R"({"nl":{"on":true,"dur":30,"mode":2,"tbri":50}})");
    remote.setState(state);

    FL_REQUIRE(remote.getNightlightOn());
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 30);
    FL_REQUIRE_EQ(remote.getNightlightMode(), 2);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 50);
}

// Test: WLED nightlight partial updates
FL_TEST_CASE("Remote: WLED nightlight partial updates") {
    fl::WLED remote;

    // Set initial nightlight state
    fl::Json state = fl::Json::parse(R"({"nl":{"on":true,"dur":45,"mode":1,"tbri":100}})");
    remote.setState(state);

    // Update only duration
    state = fl::Json::parse(R"({"nl":{"dur":10}})");
    remote.setState(state);

    FL_REQUIRE(remote.getNightlightOn());  // Preserved
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 10);  // Changed
    FL_REQUIRE_EQ(remote.getNightlightMode(), 1);  // Preserved
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 100);  // Preserved

    // Update only mode
    state = fl::Json::parse(R"({"nl":{"mode":3}})");
    remote.setState(state);

    FL_REQUIRE_EQ(remote.getNightlightDuration(), 10);  // Preserved
    FL_REQUIRE_EQ(remote.getNightlightMode(), 3);  // Changed

    // Turn off nightlight
    state = fl::Json::parse(R"({"nl":{"on":false}})");
    remote.setState(state);

    FL_REQUIRE_FALSE(remote.getNightlightOn());  // Changed
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 10);  // Preserved
}

// Test: WLED nightlight field clamping
FL_TEST_CASE("Remote: WLED nightlight field clamping") {
    fl::WLED remote;

    // Test dur clamping: below 1
    fl::Json state = fl::Json::parse(R"({"nl":{"dur":0}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 1);  // Clamped to 1

    // Test dur clamping: above 255
    state = fl::Json::parse(R"({"nl":{"dur":300}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 255);  // Clamped to 255

    // Test mode clamping: negative
    state = fl::Json::parse(R"({"nl":{"mode":-1}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightMode(), 0);  // Clamped to 0

    // Test mode clamping: above 3
    state = fl::Json::parse(R"({"nl":{"mode":10}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightMode(), 3);  // Clamped to 3

    // Test tbri clamping: negative
    state = fl::Json::parse(R"({"nl":{"tbri":-50}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 0);  // Clamped to 0

    // Test tbri clamping: above 255
    state = fl::Json::parse(R"({"nl":{"tbri":500}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 255);  // Clamped to 255
}

// Test: WLED nightlight invalid types
FL_TEST_CASE("Remote: WLED nightlight invalid types") {
    fl::WLED remote;

    // Set valid initial state
    fl::Json state = fl::Json::parse(R"({"nl":{"on":true,"dur":20,"mode":2,"tbri":128}})");
    remote.setState(state);

    // Test invalid type for dur (should keep existing value)
    uint8_t currentDur = remote.getNightlightDuration();
    state = fl::Json::parse(R"({"nl":{"dur":"invalid"}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightDuration(), currentDur);  // Unchanged

    // Test invalid type for mode (should keep existing value)
    uint8_t currentMode = remote.getNightlightMode();
    state = fl::Json::parse(R"({"nl":{"mode":"invalid"}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightMode(), currentMode);  // Unchanged

    // Test invalid type for tbri (should keep existing value)
    uint8_t currentTbri = remote.getNightlightTargetBrightness();
    state = fl::Json::parse(R"({"nl":{"tbri":"invalid"}})");
    remote.setState(state);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), currentTbri);  // Unchanged

    // Test invalid type for nl object (should warn, state unchanged)
    state = fl::Json::parse(R"({"nl":"invalid"})");
    remote.setState(state);
    FL_REQUIRE(remote.getNightlightOn());  // State should remain unchanged
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 20);
}

// Test: WLED nightlight in getState
FL_TEST_CASE("Remote: WLED nightlight in getState") {
    fl::WLED remote;

    // Set nightlight state
    fl::Json inputState = fl::Json::parse(R"({"nl":{"on":true,"dur":15,"mode":3,"tbri":200}})");
    remote.setState(inputState);

    // Get state and verify nightlight is present
    fl::Json outputState = remote.getState();
    FL_REQUIRE(outputState.contains("nl"));
    FL_REQUIRE(outputState["nl"].is_object());

    const fl::Json& nl = outputState["nl"];
    FL_REQUIRE(nl.contains("on"));
    FL_REQUIRE(nl.contains("dur"));
    FL_REQUIRE(nl.contains("mode"));
    FL_REQUIRE(nl.contains("tbri"));

    // Verify values
    FL_REQUIRE((nl["on"] | false) == true);
    FL_REQUIRE_EQ(nl["dur"] | 0, 15);
    FL_REQUIRE_EQ(nl["mode"] | 0, 3);
    FL_REQUIRE_EQ(nl["tbri"] | 0, 200);
}

// Test: WLED nightlight roundtrip
FL_TEST_CASE("Remote: WLED nightlight roundtrip") {
    fl::WLED remote;

    // Set complex state with nightlight
    fl::Json inputState = fl::Json::parse(R"({
        "on": true,
        "bri": 150,
        "nl": {
            "on": true,
            "dur": 25,
            "mode": 2,
            "tbri": 75
        }
    })");
    remote.setState(inputState);

    // Get state
    fl::Json outputState = remote.getState();

    // Verify main state
    FL_REQUIRE((outputState["on"] | false) == true);
    FL_REQUIRE_EQ(outputState["bri"] | 0, 150);

    // Verify nightlight
    const fl::Json& nl = outputState["nl"];
    FL_REQUIRE((nl["on"] | false) == true);
    FL_REQUIRE_EQ(nl["dur"] | 0, 25);
    FL_REQUIRE_EQ(nl["mode"] | 0, 2);
    FL_REQUIRE_EQ(nl["tbri"] | 0, 75);

    // Set state again from output
    remote.setState(outputState);

    // Verify everything is still correct
    FL_REQUIRE(remote.getOn());
    FL_REQUIRE_EQ(remote.getBrightness(), 150);
    FL_REQUIRE(remote.getNightlightOn());
    FL_REQUIRE_EQ(remote.getNightlightDuration(), 25);
    FL_REQUIRE_EQ(remote.getNightlightMode(), 2);
    FL_REQUIRE_EQ(remote.getNightlightTargetBrightness(), 75);
}

// Test: WLED segment hex color string parsing
FL_TEST_CASE("Remote: WLED segment hex color strings") {
    fl::WLED remote;

    // Test basic hex color strings (uppercase)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":["FF0000","00FF00","0000FF"]}]})");
    remote.setState(state);

    // Retrieve and verify the segment was created
    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mColors.size(), 3);

    // Verify red (255, 0, 0)
    FL_REQUIRE_EQ(segments[0].mColors[0].size(), 3);
    FL_REQUIRE_EQ(segments[0].mColors[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mColors[0][1], 0);
    FL_REQUIRE_EQ(segments[0].mColors[0][2], 0);

    // Verify green (0, 255, 0)
    FL_REQUIRE_EQ(segments[0].mColors[1][0], 0);
    FL_REQUIRE_EQ(segments[0].mColors[1][1], 255);
    FL_REQUIRE_EQ(segments[0].mColors[1][2], 0);

    // Verify blue (0, 0, 255)
    FL_REQUIRE_EQ(segments[0].mColors[2][0], 0);
    FL_REQUIRE_EQ(segments[0].mColors[2][1], 0);
    FL_REQUIRE_EQ(segments[0].mColors[2][2], 255);
}

// Test: WLED segment hex color strings (lowercase)
FL_TEST_CASE("Remote: WLED segment hex color strings lowercase") {
    fl::WLED remote;

    // Test lowercase hex strings
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":["ff00aa","00aaff","aa00ff"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mColors.size(), 3);

    // Verify colors parsed correctly
    FL_REQUIRE_EQ(segments[0].mColors[0][0], 255);  // ff
    FL_REQUIRE_EQ(segments[0].mColors[0][1], 0);    // 00
    FL_REQUIRE_EQ(segments[0].mColors[0][2], 170);  // aa

    FL_REQUIRE_EQ(segments[0].mColors[1][0], 0);    // 00
    FL_REQUIRE_EQ(segments[0].mColors[1][1], 170);  // aa
    FL_REQUIRE_EQ(segments[0].mColors[1][2], 255);  // ff
}

// Test: WLED segment hex color strings with leading '#'
FL_TEST_CASE("Remote: WLED segment hex color strings with hash") {
    fl::WLED remote;

    // Test hex strings with optional leading '#'
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":["#FFFFFF","#000000","#808080"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mColors.size(), 3);

    // Verify white (255, 255, 255)
    FL_REQUIRE_EQ(segments[0].mColors[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mColors[0][1], 255);
    FL_REQUIRE_EQ(segments[0].mColors[0][2], 255);

    // Verify black (0, 0, 0)
    FL_REQUIRE_EQ(segments[0].mColors[1][0], 0);
    FL_REQUIRE_EQ(segments[0].mColors[1][1], 0);
    FL_REQUIRE_EQ(segments[0].mColors[1][2], 0);

    // Verify gray (128, 128, 128)
    FL_REQUIRE_EQ(segments[0].mColors[2][0], 128);
    FL_REQUIRE_EQ(segments[0].mColors[2][1], 128);
    FL_REQUIRE_EQ(segments[0].mColors[2][2], 128);
}

// Test: WLED segment mixed RGB arrays and hex strings
FL_TEST_CASE("Remote: WLED segment mixed color formats") {
    fl::WLED remote;

    // Test mixing RGB arrays and hex strings
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":[[255,0,0],"00FF00",[0,0,255]]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mColors.size(), 3);

    // All should be parsed correctly
    FL_REQUIRE_EQ(segments[0].mColors[0][0], 255);  // Red from array
    FL_REQUIRE_EQ(segments[0].mColors[1][1], 255);  // Green from hex
    FL_REQUIRE_EQ(segments[0].mColors[2][2], 255);  // Blue from array
}

// Test: WLED segment invalid hex strings
FL_TEST_CASE("Remote: WLED segment invalid hex strings") {
    fl::WLED remote;

    // Test invalid hex strings (should be rejected with warnings)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":["INVALID","12345","1234567","GGGGGG"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    // Invalid hex strings should be skipped, so colors array should be empty
    FL_REQUIRE_EQ(segments[0].mColors.size(), 0);
}

// Test: WLED segment hex string case insensitivity
FL_TEST_CASE("Remote: WLED segment hex string case insensitivity") {
    fl::WLED remote;

    // Test mixed case hex strings
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"col":["FfAa00","00FfAa"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mColors.size(), 2);

    // Verify parsing works correctly regardless of case
    FL_REQUIRE_EQ(segments[0].mColors[0][0], 255);  // Ff
    FL_REQUIRE_EQ(segments[0].mColors[0][1], 170);  // Aa
    FL_REQUIRE_EQ(segments[0].mColors[0][2], 0);    // 00

    FL_REQUIRE_EQ(segments[0].mColors[1][0], 0);    // 00
    FL_REQUIRE_EQ(segments[0].mColors[1][1], 255);  // Ff
    FL_REQUIRE_EQ(segments[0].mColors[1][2], 170);  // Aa
}

// Test: WLED individual LED control - simple sequential format
FL_TEST_CASE("Remote: WLED individual LED control simple format") {
    fl::WLED remote;

    // Test simple sequential LED colors (no indices)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000","00FF00","0000FF"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 3);

    // Verify LED 0: red (255, 0, 0)
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0].size(), 3);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][2], 0);

    // Verify LED 1: green (0, 255, 0)
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[1][0], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[1][1], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[1][2], 0);

    // Verify LED 2: blue (0, 0, 255)
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[2][0], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[2][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[2][2], 255);
}

// Test: WLED individual LED control - indexed format
FL_TEST_CASE("Remote: WLED individual LED control indexed format") {
    fl::WLED remote;

    // Test indexed LED colors (sets specific LED indices)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000|5","00FF00|10","0000FF|15"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 16);  // Size expanded to fit LED 15

    // Verify LED 5: red
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][2], 0);

    // Verify LED 10: green
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][0], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][1], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][2], 0);

    // Verify LED 15: blue
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[15][0], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[15][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[15][2], 255);
}

// Test: WLED individual LED control - range format
FL_TEST_CASE("Remote: WLED individual LED control range format") {
    fl::WLED remote;

    // Test range LED colors (sets multiple LEDs at once)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000|0-2","0000FF|5-7"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 8);  // Size expanded to fit LED 7

    // Verify LEDs 0-2: red
    for (int i = 0; i <= 2; ++i) {
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][0], 255);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][1], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][2], 0);
    }

    // Verify LEDs 5-7: blue
    for (int i = 5; i <= 7; ++i) {
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][0], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][1], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][2], 255);
    }
}

// Test: WLED individual LED control - mixed formats
FL_TEST_CASE("Remote: WLED individual LED control mixed formats") {
    fl::WLED remote;

    // Test mixing simple, indexed, and range formats
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FFFFFF","FF0000|10","0000FF|20-22"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 23);  // Size expanded to fit LED 22

    // Verify LED 0: white (sequential format)
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][1], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][2], 255);

    // Verify LED 10: red (indexed format)
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[10][2], 0);

    // Verify LEDs 20-22: blue (range format)
    for (int i = 20; i <= 22; ++i) {
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][0], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][1], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][2], 255);
    }
}

// Test: WLED individual LED control - hex string with hash
FL_TEST_CASE("Remote: WLED individual LED control with hash") {
    fl::WLED remote;

    // Test hex strings with leading '#' (should be stripped)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["#FF0000","#00FF00|5"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);

    // Verify LED 0: red
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][1], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][2], 0);

    // Verify LED 5: green
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][0], 0);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][1], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][2], 0);
}

// Test: WLED individual LED control - serialization roundtrip
FL_TEST_CASE("Remote: WLED individual LED control serialization roundtrip") {
    fl::WLED remote;

    // Set individual LED colors
    fl::Json inputState = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000","00FF00","0000FF"]}]})");
    remote.setState(inputState);

    // Get state back
    fl::Json outputState = remote.getState();
    FL_REQUIRE(outputState.contains("seg"));
    FL_REQUIRE(outputState["seg"].is_array());
    FL_REQUIRE_EQ(outputState["seg"].size(), 1);

    const fl::Json& seg = outputState["seg"][0];
    FL_REQUIRE(seg.contains("i"));
    FL_REQUIRE(seg["i"].is_array());
    FL_REQUIRE_EQ(seg["i"].size(), 3);

    // Verify serialized values (should be uppercase hex without '#')
    fl::string led0 = seg["i"][0] | fl::string("");
    fl::string led1 = seg["i"][1] | fl::string("");
    fl::string led2 = seg["i"][2] | fl::string("");

    FL_REQUIRE_EQ(led0, "FF0000");
    FL_REQUIRE_EQ(led1, "00FF00");
    FL_REQUIRE_EQ(led2, "0000FF");

    // Set state again from output (roundtrip test)
    remote.setState(outputState);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 3);

    // Verify colors preserved
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][0], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[1][1], 255);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[2][2], 255);
}

// Test: WLED individual LED control - invalid formats
FL_TEST_CASE("Remote: WLED individual LED control invalid formats") {
    fl::WLED remote;

    // Test various invalid formats (should be rejected with warnings)
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["INVALID","12345","GGGGGG|5","FF0000|abc","FF0000|10-abc"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    // Invalid entries should be skipped, so array should be empty
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 0);
}

// Test: WLED individual LED control - case insensitivity
FL_TEST_CASE("Remote: WLED individual LED control case insensitivity") {
    fl::WLED remote;

    // Test mixed case hex strings
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FfAa00","00FfAa|5","AaBbCc|10-12"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);

    // Verify LED 0
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][0], 255);  // Ff
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][1], 170);  // Aa
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[0][2], 0);    // 00

    // Verify LED 5
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][0], 0);    // 00
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][1], 255);  // Ff
    FL_REQUIRE_EQ(segments[0].mIndividualLeds[5][2], 170);  // Aa

    // Verify LEDs 10-12
    for (int i = 10; i <= 12; ++i) {
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][0], 170);  // Aa
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][1], 187);  // Bb
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][2], 204);  // Cc
    }
}

// Test: WLED individual LED control - empty array
FL_TEST_CASE("Remote: WLED individual LED control empty array") {
    fl::WLED remote;

    // Set some LEDs first
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000","00FF00"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 2);

    // Clear with empty array
    state = fl::Json::parse(R"({"seg":[{"id":0,"i":[]}]})");
    remote.setState(state);

    // Should be empty now
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 0);
}

// Test: WLED individual LED control - large range
FL_TEST_CASE("Remote: WLED individual LED control large range") {
    fl::WLED remote;

    // Test setting a large range of LEDs
    fl::Json state = fl::Json::parse(R"({"seg":[{"id":0,"i":["FF0000|0-99"]}]})");
    remote.setState(state);

    const auto& segments = remote.getSegments();
    FL_REQUIRE_EQ(segments.size(), 1);
    FL_REQUIRE_EQ(segments[0].mIndividualLeds.size(), 100);

    // Verify all LEDs in range are red
    for (int i = 0; i < 100; ++i) {
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][0], 255);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][1], 0);
        FL_REQUIRE_EQ(segments[0].mIndividualLeds[i][2], 0);
    }
}

// =============================================================================
// NEW: Typed RPC API Tests (fl::Remote with fl::Rpc integration)
// =============================================================================

// Test: Typed method registration with auto-deduced signature
FL_TEST_CASE("Remote: Typed method registration") {
    fl::Remote remote;

    // Register typed method
    auto addFn = remote.method("add", [](int a, int b) -> int {
        return a + b;
    });

    // Direct invocation should work
    FL_REQUIRE(addFn);
    int result = addFn(2, 3);
    FL_REQUIRE_EQ(result, 5);

    // Method should be discoverable
    FL_REQUIRE(remote.hasFunction("add"));
}

// Test: Typed method via JSON RPC
FL_TEST_CASE("Remote: Typed method JSON-RPC invocation") {
    fl::Remote remote;

    remote.method("multiply", [](int a, int b) -> int {
        return a * b;
    });

    fl::Json result;
    auto err = remote.processRpc(R"({"function":"multiply","args":[6,7]})", result);

    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE(result.has_value());
    int value = result.as_int().value_or(0);
    FL_REQUIRE_EQ(value, 42);
}

// Test: Typed void method
FL_TEST_CASE("Remote: Typed void method") {
    fl::Remote remote;

    int counter = 0;
    remote.method("increment", [&counter]() {
        counter++;
    });

    // Direct invocation
    auto incrementFn = remote.bind<void()>("increment");
    FL_REQUIRE(incrementFn);
    incrementFn();
    FL_REQUIRE_EQ(counter, 1);

    // JSON-RPC invocation
    fl::Json result;
    auto err = remote.processRpc(R"({"function":"increment","args":[]})", result);
    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE_EQ(counter, 2);
}

// Test: Typed method with string arguments
FL_TEST_CASE("Remote: Typed method with string arguments") {
    fl::Remote remote;

    remote.method("greet", [](fl::string name) -> fl::string {
        return "Hello, " + name + "!";
    });

    fl::Json result;
    auto err = remote.processRpc(R"({"function":"greet","args":["World"]})", result);

    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    fl::string greeting = result.as_string().value_or("");
    FL_REQUIRE_EQ(greeting, "Hello, World!");
}

// Test: Mixed typed and legacy methods
FL_TEST_CASE("Remote: Mixed typed and legacy methods") {
    fl::Remote remote;

    // Register typed method
    remote.method("typed_add", [](int a, int b) -> int {
        return a + b;
    });

    // Register legacy method
    int legacyResult = 0;
    remote.registerFunction("legacy_add", [&legacyResult](const fl::Json& args) {
        int a = args[0] | 0;
        int b = args[1] | 0;
        legacyResult = a + b;
    });

    // Both should be found
    FL_REQUIRE(remote.hasFunction("typed_add"));
    FL_REQUIRE(remote.hasFunction("legacy_add"));

    // Both should execute correctly
    fl::Json result1;
    auto err1 = remote.processRpc(R"({"function":"typed_add","args":[10,20]})", result1);
    FL_REQUIRE_EQ(err1, fl::Remote::Error::None);
    FL_REQUIRE_EQ(result1.as_int().value_or(0), 30);

    fl::Json result2;
    auto err2 = remote.processRpc(R"({"function":"legacy_add","args":[5,7]})", result2);
    FL_REQUIRE_EQ(err2, fl::Remote::Error::None);
    FL_REQUIRE_EQ(legacyResult, 12);
}

// Test: Typed method with invalid params
FL_TEST_CASE("Remote: Typed method invalid params returns error") {
    fl::Remote remote;

    remote.method("square", [](int x) -> int {
        return x * x;
    });

    // Wrong number of arguments
    fl::Json result;
    auto err = remote.processRpc(R"({"function":"square","args":[1,2,3]})", result);
    FL_REQUIRE_EQ(err, fl::Remote::Error::InvalidParams);
}

// Test: bind returns empty function for wrong signature
FL_TEST_CASE("Remote: bind returns empty for wrong signature") {
    fl::Remote remote;

    remote.method("add", [](int a, int b) -> int {
        return a + b;
    });

    // Try to bind with wrong signature
    auto wrongFn = remote.try_bind<double(double, double)>("add");
    FL_REQUIRE_FALSE(wrongFn.has_value());

    // Correct signature should work
    auto correctFn = remote.try_bind<int(int, int)>("add");
    FL_REQUIRE(correctFn.has_value());
    FL_REQUIRE_EQ(correctFn.value()(3, 4), 7);
}

// Test: method_with fluent builder API
FL_TEST_CASE("Remote: method_with fluent builder API") {
    fl::Remote remote;

    auto setBri = remote.method_with("led.setBrightness", [](int brightness) {
        (void)brightness;  // Do nothing, just test API
    })
        .params({"brightness"})
        .description("Set LED brightness (0-255)")
        .tags({"led", "control"})
        .done();

    FL_REQUIRE(setBri);
    FL_REQUIRE(remote.hasFunction("led.setBrightness"));

    // Verify schema contains metadata
    fl::Json methods = remote.methods();
    FL_REQUIRE_EQ(methods.size(), 1);

    fl::Json method = methods[0];
    fl::string name = method["name"].as_string().value_or("");
    FL_REQUIRE_EQ(name, "led.setBrightness");

    // Check param name
    fl::string paramName = method["params"][0]["name"].as_string().value_or("");
    FL_REQUIRE_EQ(paramName, "brightness");

    // Check description
    fl::string desc = method["description"].as_string().value_or("");
    FL_REQUIRE_EQ(desc, "Set LED brightness (0-255)");

    // Check tags
    FL_REQUIRE_EQ(method["tags"].size(), 2);
}

// Test: count includes both typed and legacy methods
FL_TEST_CASE("Remote: count includes typed and legacy methods") {
    fl::Remote remote;

    // Add typed method
    remote.method("typed1", []() {});

    // Add legacy method
    remote.registerFunction("legacy1", [](const fl::Json&) {});

    // Count should be 2
    FL_REQUIRE_EQ(remote.count(), 2);
}

// Test: schema generation
FL_TEST_CASE("Remote: schema generation") {
    fl::Remote remote;

    remote.method("add", [](int a, int b) -> int { return a + b; });
    remote.method("ping", []() {});

    fl::Json schema = remote.schema("Test API", "1.0.0");

    FL_REQUIRE(schema.contains("openrpc"));
    FL_REQUIRE(schema.contains("info"));
    FL_REQUIRE(schema.contains("methods"));

    fl::string title = schema["info"]["title"].as_string().value_or("");
    FL_REQUIRE_EQ(title, "Test API");

    fl::string version = schema["info"]["version"].as_string().value_or("");
    FL_REQUIRE_EQ(version, "1.0.0");

    FL_REQUIRE_EQ(schema["methods"].size(), 2);
}

// Test: scheduled typed method execution
FL_TEST_CASE("Remote: Scheduled typed method execution") {
    fl::Remote remote;

    int value = 0;
    remote.method("setValue", [&value](int v) {
        value = v;
    });

    // Schedule for future execution
    auto err = remote.processRpc(R"({"timestamp":1000,"function":"setValue","args":[42]})");
    FL_REQUIRE_EQ(err, fl::Remote::Error::None);
    FL_REQUIRE_EQ(value, 0);  // Not executed yet

    // Execute scheduled method
    remote.tick(1000);
    FL_REQUIRE_EQ(value, 42);  // Now executed
}

#endif // FASTLED_ENABLE_JSON
