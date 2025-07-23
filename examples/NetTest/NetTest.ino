/// @file    NetTest.ino
/// @brief   Educational tutorial for FastLED WASM networking with explicit types
/// @author  FastLED Community
///
/// This tutorial demonstrates network functionality in FastLED WASM builds, specifically
/// the fetch API for making HTTP requests. It shows two different approaches for handling
/// asynchronous operations with EXPLICIT TYPES for educational clarity.
///
/// EDUCATIONAL FOCUS: All types are explicitly declared instead of using 'auto'
/// to help you understand the FastLED type system and async patterns.
///
/// TWO ASYNC APPROACHES DEMONSTRATED:
/// 
/// APPROACH 1: Promise-based with .then() and .catch_() callbacks (JavaScript-like)
/// - Uses method chaining for async operations
/// - Callbacks handle success and error cases
/// - Non-blocking, event-driven pattern
///
/// APPROACH 2: fl::await_top_level() pattern for synchronous-style async code
/// - Uses explicit types: fl::promise<T>, fl::result<T>, fl::optional<T>
/// - Blocks until async operation completes (only safe in Arduino loop()!)
/// - More traditional imperative programming style
///
/// The example toggles between these approaches every 10 seconds to demonstrate
/// both patterns working with the same underlying fetch API.
///
/// FASTLED ASYNC TYPE SYSTEM TUTORIAL:
/// 
/// Key Types You'll Learn:
/// * fl::promise<T>        - Represents a future value of type T
/// * fl::result<T>  - Wraps either a successful T value or an Error
/// * fl::response          - HTTP response with status, headers, and body
/// * fl::FetchRequest      - Configuration object for HTTP requests
/// * fl::optional<T>       - May or may not contain a value of type T
/// * fl::Error             - Error information with message and context
///
/// NEW FETCH API STRUCTURE:
/// * FetchRequest is a pure data configuration object
/// * fetch_get() returns fl::promise<fl::response> (not auto!)
/// * Promises can be handled with .then()/.catch_() OR await_top_level()
/// * All async operations integrate with FastLED's engine automatically
///
/// EXPLICIT TYPE EXAMPLES:
/// 
/// Promise-based approach:
/// ```cpp
/// fl::promise<fl::response> promise = fl::fetch_get("http://example.com");
/// promise.then([](const fl::response& response) { /* handle success */ })
///        .catch_([](const fl::Error& error) { /* handle error */ });
/// ```
///
/// Await-based approach:
/// ```cpp
/// fl::promise<fl::response> promise = fl::fetch_get("http://example.com");
/// fl::result<fl::response> result = fl::await_top_level(promise);
/// if (result.ok()) {
///     const fl::response& response = result.value();
///     // Use response...
/// }
/// ```
///
/// TO RUN THIS TUTORIAL:
/// 
/// For WASM (recommended for networking):
/// 1. Install FastLED: `pip install fastled`
/// 2. cd into this examples directory  
/// 3. Run: `fastled NetTest.ino`
/// 4. Open the web page and check browser console for detailed fetch results
///
/// For other platforms:
/// Uses mock responses for testing the API without network connectivity

#include "fl/fetch.h"     // FastLED HTTP fetch API
#include "fl/warn.h"      // FastLED logging system  
#include "fl/async.h"     // FastLED async utilities (await_top_level, etc.)
#include <FastLED.h>      // FastLED core library

using namespace fl;      // Use FastLED namespace for cleaner code

// LED CONFIGURATION
#define NUM_LEDS 10      // Number of LEDs in our strip
#define DATA_PIN 2       // Arduino pin connected to LED data line

CRGB leds[NUM_LEDS];     // Array to hold LED color data

// DEMO STATE VARIABLES  
static bool use_await_pattern = false;              // Which async approach to demonstrate
static uint32_t last_toggle_time = 0;               // Last time we switched approaches
static const uint32_t TOGGLE_INTERVAL = 10000;     // Switch approaches every 10 seconds

void setup() {
    // Initialize LED strip
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Set all LEDs to dark red initially (indicates waiting/starting state)
    fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Dark red = starting up
    FastLED.show();

    // Tutorial introduction messages
    FL_WARN("FastLED Networking Tutorial started - 10 LEDs set to dark red");
    FL_WARN("Learning HTTP fetch API with TWO different async patterns:");
    FL_WARN("  APPROACH 1: Promise-based (.then/.catch_) with explicit types");
    FL_WARN("  APPROACH 2: fl::await_top_level pattern with explicit types");
    FL_WARN("Toggles between approaches every 10 seconds for comparison...");
    FL_WARN("LED colors indicate status: Red=Error, Green=Promise Success, Blue=Await Success");
}

// APPROACH 1: Promise-based async pattern (JavaScript-like)
// This approach uses method chaining and callbacks - very common in web development
void test_promise_approach() {
    FL_WARN("APPROACH 1: Promise-based pattern with explicit types");
    
    // TUTORIAL: fetch_get() returns fl::promise<fl::response> (not auto!)
    // The promise represents a future HTTP response that may succeed or fail
    fl::promise<fl::response> fetch_promise = fl::fetch_get("http://fastled.io");
    
    // TUTORIAL: Chain .then() for success handling
    // The lambda receives a const fl::response& when the fetch succeeds
    fetch_promise.then([](const fl::response& response) {
        // TUTORIAL: Check if HTTP request was successful
        if (response.ok()) {
            FL_WARN("SUCCESS [Promise] HTTP fetch successful! Status: "
                    << response.status() << " " << response.status_text());

            // TUTORIAL: get_content_type() returns fl::optional<fl::string>
            // Optional types may or may not contain a value - always check!
            fl::optional<fl::string> content_type = response.get_content_type();
            if (content_type.has_value()) {
                FL_WARN("CONTENT [Promise] Content-Type: " << *content_type);
            }

            // TUTORIAL: response.text() returns fl::string with response body
            const fl::string& response_body = response.text();
            if (response_body.length() >= 100) {
                FL_WARN("RESPONSE [Promise] First 100 characters: " << response_body.substr(0, 100));
            } else {
                FL_WARN("RESPONSE [Promise] Full response (" << response_body.length()
                                         << " chars): " << response_body);
            }

            // Visual feedback: Green LEDs indicate promise-based success
            fill_solid(leds, NUM_LEDS, CRGB(0, 64, 0)); // Green for promise success
        } else {
            // HTTP error (like 404, 500, etc.) - still a valid response, just an error status
            FL_WARN("ERROR [Promise] HTTP Error! Status: "
                    << response.status() << " " << response.status_text());
            FL_WARN("CONTENT [Promise] Error content: " << response.text());

            // Visual feedback: Orange LEDs indicate HTTP error
            fill_solid(leds, NUM_LEDS, CRGB(64, 32, 0)); // Orange for HTTP error
        }
    })
    // TUTORIAL: Chain .catch_() for network/connection error handling  
    // The lambda receives a const fl::Error& when the fetch fails completely
    .catch_([](const fl::Error& network_error) {
        // Network error (no connection, DNS failure, etc.)
        FL_WARN("ERROR [Promise] Network Error: " << network_error.message);

        // Visual feedback: Red LEDs indicate network failure
        fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red for network error
    });
}

// APPROACH 2: fl::await_top_level() pattern (synchronous-style async code)  
// This approach blocks until completion - feels like traditional programming
void test_await_approach() {
    FL_WARN("APPROACH 2: await_top_level pattern with explicit types");
    
    // TUTORIAL: Create a FetchRequest object to configure the HTTP request
    // FetchRequest is a data container - you can set timeout, headers, etc.
    fl::FetchRequest request_config("");  // Empty URL - will use the URL passed to fetch_get()
    request_config.timeout(5000)          // 5 second timeout
                  .header("User-Agent", "FastLED/NetTest-Tutorial");  // Custom user agent
    
    // TUTORIAL: fetch_get() returns fl::promise<fl::response> (explicit type!)
    // This promise represents the future HTTP response
    fl::promise<fl::response> http_promise = fl::fetch_get("http://fastled.io", request_config);
    
    // TUTORIAL: await_top_level() returns fl::result<fl::response>
    // result wraps either a successful response OR an Error - never both!
    // CRITICAL: await_top_level() blocks until completion - ONLY safe in Arduino loop()!
    fl::result<fl::response> result = fl::await_top_level(http_promise);
    
    // TUTORIAL: Check if the result contains a successful response
    if (result.ok()) {
        // TUTORIAL: Extract the response from the result
        // result.value() returns const fl::response& - the actual HTTP response
        const fl::response& http_response = result.value();
        
        FL_WARN("SUCCESS [Await] HTTP fetch successful! Status: "
                << http_response.status() << " " << http_response.status_text());

        // TUTORIAL: Check for optional Content-Type header
        // get_content_type() returns fl::optional<fl::string> - may be empty!
        fl::optional<fl::string> content_type = http_response.get_content_type();
        if (content_type.has_value()) {
            FL_WARN("CONTENT [Await] Content-Type: " << *content_type);
        }

        // TUTORIAL: Get the response body as fl::string
        const fl::string& response_body = http_response.text();
        if (response_body.length() >= 100) {
            FL_WARN("RESPONSE [Await] First 100 characters: " << response_body.substr(0, 100));
        } else {
            FL_WARN("RESPONSE [Await] Full response (" << response_body.length()
                                     << " chars): " << response_body);
        }

        // Visual feedback: Blue LEDs indicate await-based success (different from promise)
        fill_solid(leds, NUM_LEDS, CRGB(0, 0, 64)); // Blue for await success
    } else {
        // Either HTTP error OR network error - both end up here
        // TUTORIAL: result.error_message() is a convenience method for getting error text
        FL_WARN("ERROR [Await] Request failed: " << result.error_message());

        // Visual feedback: Red LEDs for any await error
        fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red for any error
    }
}

void loop() {
    // TUTORIAL NOTE: Async operations are automatically managed!
    // * On WASM: delay() pumps async tasks every 1ms automatically
    // * On all platforms: FastLED.show() triggers async updates via engine events
    // * No manual async updates needed - everything happens behind the scenes!

    uint32_t current_time = millis();
    
    // Toggle between the two async approaches for educational comparison
    if (current_time - last_toggle_time >= TOGGLE_INTERVAL) {
        use_await_pattern = !use_await_pattern;
        last_toggle_time = current_time;
        
        FL_WARN("SWITCHING to " << (use_await_pattern ? "await_top_level" : "Promise") << " approach");
        FL_WARN("WATCH LEDs: Green=Promise success, Blue=Await success, Red=Error");
    }

    // TUTORIAL: Demonstrate the chosen async approach every 2 seconds
    EVERY_N_MILLISECONDS(2000) {
        if (use_await_pattern) {
            test_await_approach();
        } else {
            test_promise_approach();
        }

        // Always update LED strip after color changes
        FastLED.show();
        
        // TUTORIAL: This delay automatically pumps async tasks on WASM!
        // The delay is broken into 1ms chunks with async processing between chunks
        delay(10);
    }
}
