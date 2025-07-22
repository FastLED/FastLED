/// @file    NetTest.ino
/// @brief   Network functionality test for FastLED WASM including fetch API
/// @author  FastLED Community
///
/// This sketch tests network functionality in FastLED WASM builds, specifically
/// the fetch API for making HTTP requests. It demonstrates two different approaches
/// for handling async operations and provides visual feedback via LEDs.
///
/// APPROACH 1: Promise-based with .then() and .catch_() callbacks (JavaScript-like)
/// APPROACH 2: fl::await pattern for synchronous-style async code
///
/// The example toggles between these two approaches every 10 seconds to demonstrate
/// both patterns working with the same underlying fetch API.
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
#include "fl/async.h"
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 10
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

// Toggle between two different async approaches
static bool use_await_pattern = false;
static uint32_t last_toggle_time = 0;
static const uint32_t TOGGLE_INTERVAL = 10000; // Switch approaches every 10 seconds

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Set all LEDs to dark red initially
    fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Dark red
    FastLED.show();

    FL_WARN("NetTest started - 10 LEDs set to dark red");
    FL_WARN("Testing JavaScript-like fetch API with TWO different async patterns:");
    FL_WARN("  APPROACH 1: Promise-based (.then/.catch_)");
    FL_WARN("  APPROACH 2: fl::await pattern");
    FL_WARN("Toggles between approaches every 10 seconds...");
}

// APPROACH 1: Promise-based async pattern (JavaScript-like)
void test_promise_approach() {
    FL_WARN("🔄 APPROACH 1: Using Promise-based pattern (.then/.catch_)");
    
    fl::fetch_get("http://fastled.io")
        .then([](const fl::Response &response) {
            if (response.ok()) {
                FL_WARN("✅ [Promise] Fetch successful! Status: "
                        << response.status() << " "
                        << response.status_text());

                // Show content type if available
                auto content_type = response.get_content_type();
                if (content_type.has_value()) {
                    FL_WARN("[Promise] Content-Type: " << *content_type);
                }

                // Show response content (truncated for readability)
                const fl::string &content = response.text();
                if (content.length() >= 100) {
                    FL_WARN("[Promise] First 100 characters: " << content.substr(0, 100));
                } else {
                    FL_WARN("[Promise] Response (" << content.length()
                                         << " chars): " << content);
                }

                // Update LEDs to green for success
                fill_solid(leds, NUM_LEDS, CRGB(0, 64, 0)); // Green
            } else {
                FL_WARN("❌ [Promise] HTTP Error! Status: "
                        << response.status() << " "
                        << response.status_text());
                FL_WARN("[Promise] Error content: " << response.text());

                // Update LEDs to orange for HTTP error
                fill_solid(leds, NUM_LEDS, CRGB(64, 32, 0)); // Orange
            }
        })
        .catch_([](const fl::Error &err) {
            FL_WARN("❌ [Promise] Network Error: " << err.message);

            // Update LEDs to red for network error
            fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red
        });
}

// APPROACH 2: fl::await pattern (synchronous-style async code)
void test_await_approach() {
    FL_WARN("🔄 APPROACH 2: Using fl::await pattern");
    
    // Use fl::await to handle the async fetch in a synchronous style
    auto promise = fl::fetch_get("http://fastled.io");
    auto result = fl::await(promise);  // Type automatically deduced!
    
    if (result.ok()) {
        const auto& response = result.value();
        
        FL_WARN("✅ [Await] Fetch successful! Status: "
                << response.status() << " "
                << response.status_text());

        // Show content type if available
        auto content_type = response.get_content_type();
        if (content_type.has_value()) {
            FL_WARN("[Await] Content-Type: " << *content_type);
        }

        // Show response content (truncated for readability)
        const fl::string &content = response.text();
        if (content.length() >= 100) {
            FL_WARN("[Await] First 100 characters: " << content.substr(0, 100));
        } else {
            FL_WARN("[Await] Response (" << content.length()
                                 << " chars): " << content);
        }

        // Update LEDs to blue for success (different color to distinguish from promise approach)
        fill_solid(leds, NUM_LEDS, CRGB(0, 0, 64)); // Blue
    } else {
        FL_WARN("❌ [Await] Error: " << result.error_message());

        // Update LEDs to red for any error
        fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red
    }
}

void loop() {
    // Note: fetch promises are automatically updated through FastLED's async system!
    // On WASM platforms, delay() also pumps all async tasks in 1ms intervals.
    // No manual updates needed - everything happens automatically.

    uint32_t current_time = millis();
    
    // Toggle between the two approaches every TOGGLE_INTERVAL milliseconds
    if (current_time - last_toggle_time >= TOGGLE_INTERVAL) {
        use_await_pattern = !use_await_pattern;
        last_toggle_time = current_time;
        
        FL_WARN("🔀 Switching to " << (use_await_pattern ? "fl::await" : "Promise") << " approach");
    }

    EVERY_N_MILLISECONDS(2000) {
        if (use_await_pattern) {
            test_await_approach();
        } else {
            test_promise_approach();
        }

        // Keep LEDs updated
        FastLED.show();
        
        // This delay will automatically pump all async tasks on WASM platforms!
        // The delay is broken into 1ms chunks with async task pumping between each chunk.
        delay(10);
    }
}
