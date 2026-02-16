/// @file    ClientValidation.ino
/// @brief   HTTP Client Validation Suite for FastLED fetch API
///
/// Self-contained loopback test that validates fl::fetch API by running
/// both HTTP server and client in the same process.
///
/// TEST SEQUENCE:
///   1. Start HTTP server with /json, /get, /ping endpoints
///   2. Fetch GET /json (JSON slideshow data)
///   3. Fetch GET /get (request echo)
///   4. Fetch GET /ping (health check)
///   5. Validate all responses
///   6. Display pass/fail status
///
/// USAGE:
///   bash test ClientValidation --examples

#pragma once

// Disable fast exit - test needs time to complete
#undef FASTLED_STUB_MAIN_FAST_EXIT

#include "fl/net/http/server.h"
#include "fl/net/fetch.h"
#include "fl/warn.h"
#include "fl/async.h"
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 2
#define SERVER_PORT 8081

fl::CRGB leds[NUM_LEDS];
fl::HttpServer server;

enum TestState {
    SERVER_STARTING,
    TEST_JSON,
    TEST_GET,
    TEST_PING,
    ALL_PASSED,
    FAILED
};

TestState state = SERVER_STARTING;
int tests_passed = 0;
int tests_failed = 0;
uint32_t test_start_time = 0;

void updateLEDs() {
    switch (state) {
        case SERVER_STARTING:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 0, beatsin8(60)));  // Blue pulse
            break;
        case TEST_JSON:
        case TEST_GET:
        case TEST_PING:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 64, 0));  // Yellow - testing
            break;
        case ALL_PASSED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 64, 0));  // Green - success
            break;
        case FAILED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 0));  // Red - failure
            break;
    }
}

void test_json_endpoint() {
    FL_WARN("\n=== Test 1: GET /json (Slideshow Data) ===");

    fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8081/json");
    fl::result<fl::response> result = fl::await_top_level(promise);

    if (!result.ok()) {
        FL_WARN("✗ FAILED: " << result.error_message());
        tests_failed++;
        state = FAILED;
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        state = FAILED;
        return;
    }

    if (!resp.is_json()) {
        FL_WARN("✗ FAILED: Response is not JSON");
        tests_failed++;
        state = FAILED;
        return;
    }

    fl::Json data = resp.json();
    fl::string author = data["slideshow"]["author"] | fl::string("unknown");
    fl::string title = data["slideshow"]["title"] | fl::string("untitled");
    int slide_count = data["slideshow"]["slides"].size();

    if (author == "unknown" || title == "untitled" || slide_count == 0) {
        FL_WARN("✗ FAILED: Invalid JSON structure");
        FL_WARN("  Author: " << author);
        FL_WARN("  Title: " << title);
        FL_WARN("  Slides: " << slide_count);
        tests_failed++;
        state = FAILED;
        return;
    }

    FL_WARN("✓ PASSED");
    FL_WARN("  Author: " << author);
    FL_WARN("  Title: " << title);
    FL_WARN("  Slides: " << slide_count);
    tests_passed++;
}

void test_get_endpoint() {
    FL_WARN("\n=== Test 2: GET /get (Request Echo) ===");

    fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8081/get");
    fl::result<fl::response> result = fl::await_top_level(promise);

    if (!result.ok()) {
        FL_WARN("✗ FAILED: " << result.error_message());
        tests_failed++;
        state = FAILED;
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        state = FAILED;
        return;
    }

    if (!resp.is_json()) {
        FL_WARN("✗ FAILED: Response is not JSON");
        tests_failed++;
        state = FAILED;
        return;
    }

    fl::Json data = resp.json();
    fl::string origin = data["origin"] | fl::string("unknown");
    fl::string url = data["url"] | fl::string("unknown");

    if (origin == "unknown" || url == "unknown") {
        FL_WARN("✗ FAILED: Invalid response structure");
        tests_failed++;
        state = FAILED;
        return;
    }

    FL_WARN("✓ PASSED");
    FL_WARN("  Origin: " << origin);
    FL_WARN("  URL: " << url);
    tests_passed++;
}

void test_ping_endpoint() {
    FL_WARN("\n=== Test 3: GET /ping (Health Check) ===");

    fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8081/ping");
    fl::result<fl::response> result = fl::await_top_level(promise);

    if (!result.ok()) {
        FL_WARN("✗ FAILED: " << result.error_message());
        tests_failed++;
        state = FAILED;
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        state = FAILED;
        return;
    }

    fl::string body = resp.text();
    if (body != "pong\n") {
        FL_WARN("✗ FAILED: Expected 'pong\\n', got '" << body << "'");
        tests_failed++;
        state = FAILED;
        return;
    }

    FL_WARN("✓ PASSED");
    FL_WARN("  Response: " << body.substr(0, body.length() - 1));  // Strip newline for display
    tests_passed++;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nHTTP Client Validation Suite (Loopback Mode)");
    Serial.println("Starting self-contained server + client test\n");

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);

    // Register HTTP server routes that mimic httpbin.org

    // ROUTE 1: GET /json - Sample JSON slideshow data
    server.get("/json", [](const fl::http_request& req) {
        // Return JSON as raw string with proper Content-Type header
        const char* json_response = R"({
  "slideshow": {
    "author": "FastLED Community",
    "title": "FastLED Tutorial",
    "slides": [
      {"title": "Introduction to FastLED", "type": "tutorial"},
      {"title": "LED Basics", "type": "lesson"},
      {"title": "HTTP Fetch API", "type": "demo"}
    ]
  }
})";
        fl::http_response response;
        response.status(200)
                .header("Content-Type", "application/json")
                .body(json_response);
        return response;
    });

    // ROUTE 2: GET /get - Echo request information
    server.get("/get", [](const fl::http_request& req) {
        const char* json_response = R"({
  "origin": "127.0.0.1",
  "url": "http://localhost:8081/get"
})";
        fl::http_response response;
        response.status(200)
                .header("Content-Type", "application/json")
                .body(json_response);
        return response;
    });

    // ROUTE 3: GET /ping - Health check
    server.get("/ping", [](const fl::http_request& req) {
        return fl::http_response::ok("pong\n");
    });

    // Start server
    if (server.start(SERVER_PORT)) {
        Serial.print("Server started on http://localhost:");
        Serial.println(SERVER_PORT);
        state = SERVER_STARTING;
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
    if (state == SERVER_STARTING && (fl::millis() - test_start_time > 1000)) {
        FL_WARN("=================================");
        FL_WARN("Starting HTTP Client Tests");
        FL_WARN("=================================");
        state = TEST_JSON;
    }

    // Run tests sequentially
    if (state == TEST_JSON) {
        test_json_endpoint();
        if (state != FAILED) state = TEST_GET;
        delay(500);
    }
    else if (state == TEST_GET) {
        test_get_endpoint();
        if (state != FAILED) state = TEST_PING;
        delay(500);
    }
    else if (state == TEST_PING) {
        test_ping_endpoint();
        if (state != FAILED) {
            // All tests passed!
            state = ALL_PASSED;

            FL_WARN("\n=================================");
            FL_WARN("Test Results");
            FL_WARN("=================================");
            FL_WARN("Passed: " << tests_passed);
            FL_WARN("Failed: " << tests_failed);
            FL_WARN("Total:  " << (tests_passed + tests_failed));
            FL_WARN("=================================");
            FL_WARN("✓ All tests PASSED");
        }
        delay(500);
    }
    else if (state == FAILED) {
        FL_WARN("\n=================================");
        FL_WARN("Test Results");
        FL_WARN("=================================");
        FL_WARN("Passed: " << tests_passed);
        FL_WARN("Failed: " << tests_failed);
        FL_WARN("Total:  " << (tests_passed + tests_failed));
        FL_WARN("=================================");
        FL_WARN("✗ Some tests FAILED");

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
