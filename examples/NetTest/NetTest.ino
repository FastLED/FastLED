/// @file    NetTest.ino
/// @brief   Network functionality test for FastLED WASM including fetch API
/// @author  FastLED Community
///
/// This sketch tests network functionality in FastLED WASM builds, specifically
/// the fetch API for making HTTP requests. It demonstrates multiple fetch
/// scenarios and provides visual feedback via LEDs.
///
/// For WASM:
/// 1. Install FastLED: `pip install fastled`
/// 2. cd into this examples directory
/// 3. Run: `fastled NetTest.ino`
/// 4. Open the web page and check the browser console for fetch results
///
/// For other platforms:
/// Uses mock fetch responses for testing the API without network connectivity

#include "fl/fetch.h"
#include "fl/warn.h"
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 10
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Set all LEDs to dark red initially
    fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Dark red
    FastLED.show();

    FL_WARN("NetTest started - 10 LEDs set to dark red");
    FL_WARN("Testing JavaScript-like fetch API...");
}

void loop() {
    // Note: fetch promises are automatically updated through FastLED's async system!
    // On WASM platforms, delay() also pumps all async tasks in 1ms intervals.
    // No manual updates needed - everything happens automatically.

    EVERY_N_MILLISECONDS(2000) {
        // Test 1: Simple GET request (like JavaScript fetch)
        FL_WARN("Test 1: Simple GET request");
        fl::fetch_get("http://fastled.io")
            .then([](const fl::Response &response) {
                if (response.ok()) {
                    FL_WARN("✅ Fetch successful! Status: "
                            << response.status() << " "
                            << response.status_text());

                    // Show content type if available
                    auto content_type = response.get_content_type();
                    if (content_type.has_value()) {
                        FL_WARN("Content-Type: " << *content_type);
                    }

                    // Show response content (truncated for readability)
                    const fl::string &content = response.text();
                    if (content.length() >= 100) {
                        FL_WARN(
                            "First 100 characters: " << content.substr(0, 100));
                    } else {
                        FL_WARN("Response (" << content.length()
                                             << " chars): " << content);
                    }

                    // Update LEDs to green for success
                    fill_solid(leds, NUM_LEDS, CRGB(0, 64, 0)); // Green
                } else {
                    FL_WARN("❌ HTTP Error! Status: "
                            << response.status() << " "
                            << response.status_text());
                    FL_WARN("Error content: " << response.text());

                    // Update LEDs to orange for HTTP error
                    fill_solid(leds, NUM_LEDS, CRGB(64, 32, 0)); // Orange
                }
            })
            .catch_([](const fl::Error &err) {
                FL_WARN("❌ Network Error: " << err.message);

                // Update LEDs to red for network error
                fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red
            });

        // Keep LEDs updated
        FastLED.show();
        
        // This delay will automatically pump all async tasks on WASM platforms!
        // The delay is broken into 1ms chunks with async task pumping between each chunk.
        delay(10);
    }
}
