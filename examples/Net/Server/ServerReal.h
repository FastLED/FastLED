/// @file    Network.ino
/// @brief   HTTP Server Example for FastLED
///
/// Demonstrates minimal HTTP server with route handlers using fl::net::HttpServer.
///
/// ROUTES:
///   GET  /           - Hello message
///   GET  /status     - LED status (JSON)
///   POST /color      - Set LED color (JSON body: {"r":255,"g":0,"b":0})
///   GET  /ping       - Health check
///
/// LED STATUS:
///   Blue (pulse)   - Server starting
///   Green (solid)  - Server running
///   Cyan (flash)   - Request received
///   Purple (flash) - Response sent
///   Red (solid)    - Error
///
/// USAGE:
///   1. Compile: bash compile posix --examples Network
///   2. Run: .build/meson-quick/examples/Network.exe
///   3. Test: uv run python examples/Network/test_client.py
///   4. Or: curl http://localhost:8080/

#pragma once

// Note: FASTLED_STUB_MAIN_FAST_EXIT is enabled by default for testing
// This allows the server to start, verify it's working, and exit cleanly after 5 iterations

#include "fl/net/http/server.h"
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 2

fl::CRGB leds[NUM_LEDS];
fl::HttpServer server;

enum ServerState {
    STARTING,
    RUNNING,
    REQUEST_RECEIVED,
    RESPONDED,
    ERROR
};

ServerState state = STARTING;
fl::u32 lastEventTime = 0;

void updateLEDs() {
    switch (state) {
        case STARTING:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 0, beatsin8(60)));  // Blue pulse
            break;
        case RUNNING:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 64, 0));  // Green
            break;
        case REQUEST_RECEIVED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(0, 128, 128));  // Cyan
            break;
        case RESPONDED:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 64));  // Purple
            break;
        case ERROR:
            fill_solid(leds, NUM_LEDS, fl::CRGB(64, 0, 0));  // Red
            break;
    }
}

void autoReset() {
    // Auto-reset flash states to RUNNING after 200ms
    if ((state == REQUEST_RECEIVED || state == RESPONDED) &&
        (fl::millis() - lastEventTime > 200)) {
        state = RUNNING;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("HTTP Server Example");

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);

    // ROUTE 1: GET / - Hello message
    server.get("/", [](const fl::http_request& req) {
        Serial.println("[GET /] Hello request");

        fl::http_response response;
        response.status(200)
                .header("Content-Type", "text/plain")
                .body("Hello from FastLED!\n");

        return response;
    });

    // ROUTE 2: GET /status - LED status as JSON
    server.get("/status", [](const fl::http_request& req) {
        Serial.println("[GET /status] Status request");

        fl::Json status = fl::Json::object();
        status.set("num_leds", NUM_LEDS);
        status.set("brightness", FastLED.getBrightness());
        status.set("uptime_ms", static_cast<fl::i64>(fl::millis()));

        return fl::http_response::ok().json(status);
    });

    // ROUTE 3: POST /color - Set LED color
    server.post("/color", [](const fl::http_request& req) {
        Serial.println("[POST /color] Color change request");

        fl::Json body = fl::Json::parse(req.body());
        if (body.is_null()) {
            return fl::http_response::bad_request("Invalid JSON");
        }

        int r = body["r"] | 0;
        int g = body["g"] | 0;
        int b = body["b"] | 0;

        fill_solid(leds, NUM_LEDS, fl::CRGB(r, g, b));

        Serial.print("Color set to RGB(");
        Serial.print(r); Serial.print(", ");
        Serial.print(g); Serial.print(", ");
        Serial.print(b); Serial.println(")");

        return fl::http_response::ok("Color updated\n");
    });

    // ROUTE 4: GET /ping - Health check
    server.get("/ping", [](const fl::http_request& req) {
        return fl::http_response::ok("pong\n");
    });

    // Start server
    if (server.start(8080)) {
        Serial.println("Server started on http://localhost:8080/");
        state = RUNNING;
    } else {
        Serial.println("ERROR: Failed to start server");
        Serial.print("Error: ");
        Serial.println(server.last_error().c_str());
        state = ERROR;
    }

    updateLEDs();
    FastLED.show();
}

void loop() {
    // Update server multiple times per loop for better responsiveness
    // Process up to 10 update cycles per loop iteration
    fl::size total_processed = 0;
    for (int i = 0; i < 10; ++i) {
        fl::size processed = server.update();
        total_processed += processed;
        if (processed == 0) break;  // No more pending requests
    }

    if (total_processed > 0) {
        Serial.print("Processed ");
        Serial.print(static_cast<int>(total_processed));
        Serial.println(" request(s)");
        state = REQUEST_RECEIVED;
        lastEventTime = fl::millis();
        // After processing, flash purple for "responded"
        state = RESPONDED;
    }

    autoReset();
    updateLEDs();
    FastLED.show();

    delay(10);  // 10ms delay between cycles - up to 1000 requests/second possible
}
