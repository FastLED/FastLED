/// @file    LoopbackTestRunner.h
/// @brief   Non-blocking HTTP test runner for FastLED loopback testing
///
/// Manages sequential test execution using async .then()/.catch_() callbacks
/// instead of blocking await_top_level() calls. This keeps the Arduino loop()
/// responsive and allows LEDs to update smoothly during HTTP requests.
///
/// DUAL-STATE PATTERN:
///   RUNNING_* states launch async fetch() and immediately return
///   WAITING_* states wait for promise callbacks to complete
///   Callbacks advance state to next RUNNING_* or COMPLETED
///
/// USAGE:
///   LoopbackTestRunner runner;
///   runner.startTests([](bool success, int passed, int total) {
///       // Called when all tests complete
///   });
///
///   // In loop():
///   runner.update();  // Advances state machine

#pragma once

#include <FastLED.h>
#include "fl/net/fetch.h"
#include "fl/warn.h"

class LoopbackTestRunner {
public:
    using CompletionCallback = fl::function<void(bool success, int passed, int total)>;

    enum TestSequenceState {
        IDLE,
        RUNNING_GET_ROOT,
        WAITING_GET_ROOT,
        RUNNING_GET_PING,
        WAITING_GET_PING,
        RUNNING_GET_TEST,
        WAITING_GET_TEST,
        COMPLETED
    };

    LoopbackTestRunner()
        : state(IDLE)
        , tests_run(0)
        , tests_passed(0)
        , has_failure(false)
        , callback_invoked(false)
    {}

    void startTests(CompletionCallback callback) {
        FL_WARN("[LOOPBACK] startTests() called - initializing test runner");
        FL_WARN("[LOOPBACK] startTests() called");
        completion_callback = callback;
        tests_run = 0;
        tests_passed = 0;
        has_failure = false;
        callback_invoked = false;
        state = RUNNING_GET_ROOT;
    }

    void update() {
        // State machine advances through RUNNING -> WAITING -> RUNNING transitions
        // Callbacks (in runTest) handle WAITING -> RUNNING_NEXT transitions
        switch (state) {
            case RUNNING_GET_ROOT:
                runTest("GET /", "http://localhost:8080/",
                       "Hello from loopback test!\n", RUNNING_GET_PING);
                break;

            case RUNNING_GET_PING:
                runTest("GET /ping", "http://localhost:8080/ping",
                       "pong\n", RUNNING_GET_TEST);
                break;

            case RUNNING_GET_TEST:
                runTest("GET /test", "http://localhost:8080/test",
                       "test response\n", COMPLETED);
                break;

            case COMPLETED:
                if (!callback_invoked && completion_callback) {
                    // Print summary
                    Serial.println();
                    Serial.println("======================");
                    Serial.print("Test Results: ");
                    Serial.print(tests_passed);
                    Serial.print("/");
                    Serial.print(tests_run);
                    Serial.println(" passed");
                    Serial.println("======================");

                    bool success = !has_failure && tests_passed == tests_run;
                    if (success) {
                        Serial.println("✓ All loopback tests PASSED");
                    } else {
                        Serial.println("✗ Loopback tests FAILED");
                    }

                    // Invoke user callback with results
                    completion_callback(success, tests_passed, tests_run);
                    callback_invoked = true;
                }
                state = IDLE;
                break;

            default:
                // WAITING states or IDLE - nothing to do
                break;
        }
    }

    bool isRunning() const {
        return state != IDLE && state != COMPLETED;
    }

    TestSequenceState getState() const {
        return state;
    }

private:
    void runTest(const char* name, const char* url,
                 const char* expected, TestSequenceState next_state) {
        FL_WARN("[LOOPBACK] Running test: " << name << " -> " << url);
        Serial.print("Running test: ");
        Serial.println(name);

        tests_run++;

        // Launch async HTTP request with non-blocking .then()/.catch_() callbacks
        fl::fetch_get(url)
            .then([this, expected, next_state](const fl::response& resp) {
                // Success callback - validate response
                if (resp.status() == 200 && resp.text() == expected) {
                    tests_passed++;
                    Serial.println("  ✓ PASSED");
                } else {
                    has_failure = true;
                    Serial.print("  ✗ FAILED - ");
                    if (resp.status() != 200) {
                        Serial.print("Status: ");
                        Serial.print(static_cast<int>(resp.status()));
                    } else {
                        Serial.print("Expected: '");
                        Serial.print(expected);
                        Serial.print("', Got: '");
                        Serial.print(resp.text().c_str());
                        Serial.print("'");
                    }
                    Serial.println();
                }

                // Advance to next test state
                state = next_state;
            })
            .catch_([this](const fl::Error& err) {
                // Error callback - network/connection failure
                has_failure = true;
                Serial.print("  ✗ FAILED - Error: ");
                Serial.println(err.message.c_str());

                // Abort remaining tests on network error
                state = COMPLETED;
            });

        // Immediately transition to waiting state (non-blocking)
        state = static_cast<TestSequenceState>(state + 1);  // RUNNING_X -> WAITING_X
    }

    TestSequenceState state;
    int tests_run;
    int tests_passed;
    bool has_failure;
    bool callback_invoked;
    CompletionCallback completion_callback;
};
