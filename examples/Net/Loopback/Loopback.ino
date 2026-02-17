/// @file    Loopback.ino
/// @brief   HTTP Server Loopback Test for FastLED
/// @filter: (platform is native)
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
///   Yellow (flash) - Tests running
///   Green (solid)  - All tests passed
///   Red (solid)    - Test failed
///
/// USAGE:
///   bash test Loopback --examples
///
/// ARCHITECTURE:
///   - Loopback.ino (this file) - Main setup/loop, server routes, LED feedback
///   - LoopbackTestRunner.h - Non-blocking async test orchestration

#include <FastLED.h>
#include "fl/net/http/server.h"
#include "LoopbackTestRunner.h"

// LED Configuration
#define NUM_LEDS 10
#define DATA_PIN 2

fl::CRGB leds[NUM_LEDS];
fl::HttpServer server;
LoopbackTestRunner testRunner;

// Main state machine
enum AppState {
    SERVER_STARTING,
    WAITING_FOR_TESTS,
    TESTS_RUNNING,
    ALL_PASSED,
    FAILED
};

AppState state = SERVER_STARTING;
uint32_t startup_time = 0;

// LED visual feedback based on current state
void updateLEDs() {
    switch (state) {
        case SERVER_STARTING:
            // Blue pulse while server initializes
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 0, beatsin8(60)));
            break;

        case WAITING_FOR_TESTS:
        case TESTS_RUNNING:
            // Yellow during test execution
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 64, 0));
            break;

        case ALL_PASSED:
            // Green when all tests pass
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 64, 0));
            break;

        case FAILED:
            // Red when tests fail
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 0));
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("HTTP Server Loopback Test");

    // Initialize LED strip
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);

    // Register HTTP server routes inline (clear and simple)
    server.get("/", [](const fl::http_request& req) {
        return fl::http_response::ok("Hello from loopback test!\n");
    });

    server.get("/ping", [](const fl::http_request& req) {
        return fl::http_response::ok("pong\n");
    });

    server.get("/test", [](const fl::http_request& req) {
        return fl::http_response::ok("test response\n");
    });

    // Start HTTP server
    if (server.start(8080)) {
        Serial.println("Server started on http://localhost:8080/");
        state = WAITING_FOR_TESTS;
        startup_time = fl::millis();
    } else {
        Serial.println("ERROR: Failed to start server");
        Serial.print("Error: ");
        Serial.println(server.last_error().c_str());
        state = FAILED;
    }

    updateLEDs();
    FastLED.show();
}

void loop() {
    // Pump async task queue (async fetch tasks + server)
    fl::async_run();

    // State: WAITING_FOR_TESTS -> wait 1 second after startup -> start tests
    if (state == WAITING_FOR_TESTS && (fl::millis() - startup_time > 1000)) {
        // Start test sequence with completion callback
        testRunner.startTests([](bool success, int passed, int total) {
            // Called when all tests complete
            state = success ? ALL_PASSED : FAILED;
            server.stop();
        });
        state = TESTS_RUNNING;
    }

    // Update test runner state machine (advances tests, processes callbacks)
    if (state == TESTS_RUNNING) {
        testRunner.update();
    }


    if (state == ALL_PASSED || state == FAILED) {
        updateLEDs();
        FastLED.show();
        delay(100);
        return;  // Stay in terminal state, let the framework break from this test.
    }

    // Update LED feedback and show
    updateLEDs();
    FastLED.show();

    // Small delay allows async promise callbacks to process
    // On WASM, delay() automatically pumps async tasks every 1ms
    delay(10);
}
