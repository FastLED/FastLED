/// @file    ClientValidation.ino
/// @brief   HTTP Client Validation Suite for FastLED fetch API
///
/// Tests fl::fetch API against local Python HTTP server (test_server.py)
///
/// TEST SEQUENCE:
///   1. Fetch GET /json (JSON slideshow data)
///   2. Fetch GET /get (request echo)
///   3. Fetch GET /ping (health check)
///   4. Validate all responses
///   5. Display pass/fail status
///
/// USAGE:
///   1. Start server: uv run python examples/Net/Client/test_server.py
///   2. Run test: bash test ClientValidation --examples

#pragma once

// Disable fast exit - test needs time to complete
#undef FASTLED_STUB_MAIN_FAST_EXIT

#include "fl/net/fetch.h"
#include "fl/warn.h"
#include "fl/async.h"
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 2

fl::CRGB leds[NUM_LEDS];

int tests_passed = 0;
int tests_failed = 0;
bool all_tests_done = false;

void test_json_endpoint() {
    FL_WARN("\n=== Test 1: GET /json (Slideshow Data) ===");

    fl::promise<fl::response> promise = fl::fetch_get("http://localhost:8081/json");
    fl::result<fl::response> result = fl::await_top_level(promise);

    if (!result.ok()) {
        FL_WARN("✗ FAILED: " << result.error_message());
        tests_failed++;
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        return;
    }

    if (!resp.is_json()) {
        FL_WARN("✗ FAILED: Response is not JSON");
        tests_failed++;
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
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        return;
    }

    if (!resp.is_json()) {
        FL_WARN("✗ FAILED: Response is not JSON");
        tests_failed++;
        return;
    }

    fl::Json data = resp.json();
    fl::string origin = data["origin"] | fl::string("unknown");
    fl::string url = data["url"] | fl::string("unknown");

    if (origin == "unknown" || url == "unknown") {
        FL_WARN("✗ FAILED: Invalid response structure");
        tests_failed++;
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
        return;
    }

    const fl::response& resp = result.value();
    if (resp.status() != 200) {
        FL_WARN("✗ FAILED: Status " << resp.status() << " " << resp.status_text());
        tests_failed++;
        return;
    }

    fl::string body = resp.text();
    if (body != "pong\n") {
        FL_WARN("✗ FAILED: Expected 'pong\\n', got '" << body << "'");
        tests_failed++;
        return;
    }

    FL_WARN("✓ PASSED");
    FL_WARN("  Response: " << body.substr(0, body.length() - 1));  // Strip newline for display
    tests_passed++;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nHTTP Client Test (fl::fetch API)");
    Serial.println("Connecting to: http://localhost:8081");
    Serial.println("Make sure test_server.py is running!\n");

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);
    fill_solid(leds, NUM_LEDS, fl::CRGB(0, 0, 64));  // Blue = starting
    FastLED.show();

    // Wait a moment for serial to initialize
    fl::delay(100);
}

void loop() {
    if (!all_tests_done) {
        FL_WARN("=================================");
        FL_WARN("Starting HTTP Client Tests");
        FL_WARN("=================================");

        // Run all tests
        test_json_endpoint();
        test_get_endpoint();
        test_ping_endpoint();

        // Display results
        FL_WARN("\n=================================");
        FL_WARN("Test Results");
        FL_WARN("=================================");
        FL_WARN("Passed: " << tests_passed);
        FL_WARN("Failed: " << tests_failed);
        FL_WARN("Total:  " << (tests_passed + tests_failed));
        FL_WARN("=================================");

        if (tests_failed == 0) {
            FL_WARN("✓ All tests PASSED");
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 64, 0));  // Green = success
        } else {
            FL_WARN("✗ Some tests FAILED");
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 0));  // Red = failure
        }

        FastLED.show();
        all_tests_done = true;
    }

    delay(100);
}
