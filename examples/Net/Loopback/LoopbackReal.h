/// @file    NetworkLoopback.ino
/// @brief   HTTP Server Loopback Test for FastLED
///
/// Tests HTTP server by making client requests from the same process.
/// Demonstrates server and client interaction within a single .ino file.
///
/// TEST SEQUENCE:
///   1. Start HTTP server on port 8080
///   2. Make client request to GET /
///   3. Make client request to GET /ping
///   4. Make client request to GET /test
///   5. Verify all responses are correct
///   6. Display pass/fail status on LEDs
///
/// LED STATUS:
///   Blue (pulse)   - Server starting
///   Green (solid)  - All tests passed
///   Yellow (flash) - Test in progress
///   Red (solid)    - Test failed
///
/// USAGE:
///   bash test NetworkLoopback --examples

#pragma once

// Disable fast exit - loopback test needs time to complete
#undef FASTLED_STUB_MAIN_FAST_EXIT

#include "fl/net/http/server.h"
#include "fl/net/fetch.h"
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 2

fl::CRGB leds[NUM_LEDS];
fl::HttpServer server;

enum TestState {
    STARTING,
    TEST_GET_ROOT,
    TEST_GET_PING,
    TEST_GET_TEST,
    ALL_PASSED,
    FAILED
};

TestState state = STARTING;
int test_count = 0;
int passed_count = 0;
uint32_t test_start_time = 0;

void updateLEDs() {
    switch (state) {
        case STARTING:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 0, beatsin8(60)));  // Blue pulse
            break;
        case TEST_GET_ROOT:
        case TEST_GET_PING:
        case TEST_GET_TEST:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 64, 0));  // Yellow
            break;
        case ALL_PASSED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 64, 0));  // Green
            break;
        case FAILED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 0));  // Red
            break;
    }
}

void runTest(const char* test_name, const char* url, const char* expected_response) {
    Serial.print("Running test: ");
    Serial.println(test_name);

    test_count++;

    // Make HTTP request using fl::fetch_get (new promise-based API)
    fl::promise<fl::response> promise = fl::fetch_get(url);
    fl::result<fl::response> result = fl::await_top_level(promise);

    if (!result.ok()) {
        Serial.print("  ✗ FAILED - Error: ");
        Serial.println(result.error_message().c_str());
        state = FAILED;
        return;
    }

    const fl::response& response = result.value();

    if (response.status() != 200) {
        Serial.print("  ✗ FAILED - Status code: ");
        Serial.println(static_cast<int>(response.status()));
        state = FAILED;
        return;
    }

    // Check response body
    const fl::string& body_str = response.text();
    if (body_str != expected_response) {
        Serial.print("  ✗ FAILED - Expected: '");
        Serial.print(expected_response);
        Serial.print("', Got: '");
        Serial.print(body_str.c_str());
        Serial.println("'");
        state = FAILED;
        return;
    }

    Serial.println("  ✓ PASSED");
    passed_count++;
}

void setup() {
    Serial.begin(115200);
    Serial.println("HTTP Server Loopback Test");

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);

    // Register HTTP routes
    server.get("/", [](const fl::http_request& req) {
        return fl::http_response::ok("Hello from loopback test!\n");
    });

    server.get("/ping", [](const fl::http_request& req) {
        return fl::http_response::ok("pong\n");
    });

    server.get("/test", [](const fl::http_request& req) {
        return fl::http_response::ok("test response\n");
    });

    // Start server
    if (server.start(8080)) {
        Serial.println("Server started on http://localhost:8080/");
        state = STARTING;
    } else {
        Serial.println("ERROR: Failed to start server");
        Serial.print("Error: ");
        Serial.println(server.last_error().c_str());
        state = FAILED;
    }

    updateLEDs();
    FastLED.show();

    test_start_time = fl::millis();
}

void loop() {
    // Update server (process incoming requests)
    server.update();

    // Wait 1 second after startup before starting tests
    if (state == STARTING && (fl::millis() - test_start_time > 1000)) {
        state = TEST_GET_ROOT;
    }

    // Run tests sequentially
    if (state == TEST_GET_ROOT) {
        runTest("GET /", "http://localhost:8080/", "Hello from loopback test!\n");
        if (state != FAILED) state = TEST_GET_PING;
        delay(500);
    }
    else if (state == TEST_GET_PING) {
        runTest("GET /ping", "http://localhost:8080/ping", "pong\n");
        if (state != FAILED) state = TEST_GET_TEST;
        delay(500);
    }
    else if (state == TEST_GET_TEST) {
        runTest("GET /test", "http://localhost:8080/test", "test response\n");
        if (state != FAILED) {
            // All tests passed!
            state = ALL_PASSED;
            Serial.println();
            Serial.println("======================");
            Serial.print("Test Results: ");
            Serial.print(passed_count);
            Serial.print("/");
            Serial.print(test_count);
            Serial.println(" passed");
            Serial.println("======================");
            Serial.println("✓ All loopback tests PASSED");
        }
        delay(500);
    }
    else if (state == FAILED) {
        Serial.println();
        Serial.println("======================");
        Serial.print("Test Results: ");
        Serial.print(passed_count);
        Serial.print("/");
        Serial.print(test_count);
        Serial.println(" passed");
        Serial.println("======================");
        Serial.println("✗ Loopback tests FAILED");

        // Stop after failure
        server.stop();
        while (true) {
            updateLEDs();
            FastLED.show();
            delay(100);
        }
    }
    else if (state == ALL_PASSED) {
        // Stop after success
        server.stop();
        while (true) {
            updateLEDs();
            FastLED.show();
            delay(100);
        }
    }

    updateLEDs();
    FastLED.show();
    delay(100);
}
