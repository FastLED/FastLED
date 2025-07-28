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
/// * fl::fetch_options      - Configuration object for HTTP requests
/// * fl::optional<T>       - May or may not contain a value of type T
/// * fl::Error             - Error information with message and context
///
/// NEW FETCH API STRUCTURE:
/// * fetch_options is a pure data configuration object
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

CRGB leds[NUM_LEDS];

// Timing and approach control variables for tutorial cycling
static u32 last_request_time = 0;        // Track last request time for 10-second intervals
static int approach_counter = 0;                    // Cycle through 4 different approaches

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
    // Chain .then() for success handling and the lambda receives a
    // const fl::response& when the fetch succeeds. error_ will handle network device
    // failures (no connection, DNS failure, etc, but not HTTP errors like 404, 500, etc.)
    fl::fetch_get("http://fastled.io").then([](const fl::response& response) {
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
    
    // TUTORIAL: Create a fetch_options object to configure the HTTP request
    // fetch_options is a data container - you can set timeout, headers, etc.
    fl::fetch_options request_config("");  // Empty URL - will use the URL passed to fetch_get()
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

/// APPROACH 3: JSON Response Handling with FastLED's ideal JSON API
/// This demonstrates fetch responses with automatic JSON parsing
void test_json_response() {
    FL_WARN("APPROACH 3: JSON Response handling with fl::Json integration");
    
    // TUTORIAL: Fetch a JSON API endpoint (httpbin.org provides test JSON)
    // This endpoint returns JSON with request information
    fl::fetch_get("https://httpbin.org/json").then([](const fl::response& response) {
        if (response.ok()) {
            FL_WARN("SUCCESS [JSON Promise] HTTP fetch successful! Status: "
                    << response.status() << " " << response.status_text());
            
            // TUTORIAL: Check if response contains JSON content
            // is_json() checks Content-Type header and body content
            if (response.is_json()) {
                FL_WARN("DETECTED [JSON Promise] Response contains JSON data");
                
                // TUTORIAL: response.json() returns fl::Json with FastLED's ideal API
                // Automatic parsing, caching, and safe access with defaults using operator|
                fl::Json data = response.json();
                
                // TUTORIAL: Safe JSON access with defaults - never crashes!
                // Uses FastLED's proven pattern: json["path"]["to"]["key"] | default_value
                fl::string slideshow_url = data["slideshow"]["author"] | fl::string("unknown");
                fl::string slideshow_title = data["slideshow"]["title"] | fl::string("untitled");
                int slide_count = data["slideshow"]["slides"].size();
                
                FL_WARN("JSON [Promise] Slideshow Author: " << slideshow_url);
                FL_WARN("JSON [Promise] Slideshow Title: " << slideshow_title);
                FL_WARN("JSON [Promise] Slide Count: " << slide_count);
                
                // TUTORIAL: Access nested arrays safely
                if (data.contains("slideshow") && data["slideshow"].contains("slides")) {
                    fl::Json slides = data["slideshow"]["slides"];
                    if (slides.is_array() && slides.size() > 0) {
                        // Get first slide information with safe defaults
                        fl::string first_slide_title = slides[0]["title"] | fl::string("no title");
                        fl::string first_slide_type = slides[0]["type"] | fl::string("unknown");
                        FL_WARN("JSON [Promise] First slide: " << first_slide_title << " (" << first_slide_type << ")");
                    }
                }
                
                // Visual feedback: Blue LEDs for successful JSON parsing
                fill_solid(leds, NUM_LEDS, CRGB(0, 0, 128)); // Blue for JSON success
                
            } else {
                FL_WARN("INFO [JSON Promise] Response is not JSON format");
                // Visual feedback: Yellow for non-JSON response
                fill_solid(leds, NUM_LEDS, CRGB(64, 64, 0)); // Yellow for non-JSON
            }
        } else {
            FL_WARN("ERROR [JSON Promise] HTTP error: " << response.status() 
                    << " " << response.status_text());
            // Visual feedback: Red LEDs for HTTP error
            fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Red for HTTP error
        }
    }).catch_([](const fl::Error& error) {
        FL_WARN("ERROR [JSON Promise] Network error: " << error.message);
        // Visual feedback: Purple LEDs for network error
        fill_solid(leds, NUM_LEDS, CRGB(64, 0, 64)); // Purple for network error
    });
    
    FastLED.show();
}

/// APPROACH 4: JSON Response with await pattern  
/// Same JSON handling but using await_top_level for synchronous-style code
void test_json_await() {
    FL_WARN("APPROACH 4: JSON Response with await pattern");
    
    // TUTORIAL: Using await pattern with JSON responses
    // fl::fetch_get() returns fl::promise<fl::response>
    fl::promise<fl::response> json_promise = fl::fetch_get("https://httpbin.org/get");
    
    // TUTORIAL: await_top_level() converts promise to result
    // fl::result<fl::response> contains either response or error
    fl::result<fl::response> result = fl::await_top_level(json_promise);
    
    if (result.ok()) {
        // TUTORIAL: Extract the response from the result
        const fl::response& http_response = result.value();
        
        FL_WARN("SUCCESS [JSON Await] HTTP fetch successful! Status: "
                << http_response.status() << " " << http_response.status_text());
        
        // TUTORIAL: Check for JSON content and parse if available
        if (http_response.is_json()) {
            FL_WARN("DETECTED [JSON Await] Response contains JSON data");
            
            // TUTORIAL: Parse JSON with automatic caching
            fl::Json data = http_response.json();
            
            // TUTORIAL: httpbin.org/get returns information about the request
            // Extract data with safe defaults using FastLED's ideal JSON API
            fl::string origin_ip = data["origin"] | fl::string("unknown");
            fl::string request_url = data["url"] | fl::string("unknown");
            
            FL_WARN("JSON [Await] Request Origin IP: " << origin_ip);
            FL_WARN("JSON [Await] Request URL: " << request_url);
            
            // TUTORIAL: Access nested headers object safely
            if (data.contains("headers")) {
                fl::Json headers = data["headers"];
                fl::string user_agent = headers["User-Agent"] | fl::string("unknown");
                fl::string accept = headers["Accept"] | fl::string("unknown");
                
                FL_WARN("JSON [Await] User-Agent: " << user_agent);
                FL_WARN("JSON [Await] Accept: " << accept);
            }
            
            // TUTORIAL: Access query parameters (if any)
            if (data.contains("args")) {
                fl::Json args = data["args"];
                if (args.size() > 0) {
                    FL_WARN("JSON [Await] Query parameters found: " << args.size());
                } else {
                    FL_WARN("JSON [Await] No query parameters in request");
                }
            }
            
            // Visual feedback: Cyan LEDs for successful await JSON processing
            fill_solid(leds, NUM_LEDS, CRGB(0, 128, 128)); // Cyan for await JSON success
            
        } else {
            FL_WARN("INFO [JSON Await] Response is not JSON format");
            // Visual feedback: Orange for non-JSON with await
            fill_solid(leds, NUM_LEDS, CRGB(128, 32, 0)); // Orange for non-JSON await
        }
        
    } else {
        // TUTORIAL: Handle request failures (network or HTTP errors)
        FL_WARN("ERROR [JSON Await] Request failed: " << result.error_message());
        // Visual feedback: Red LEDs for any await error
        fill_solid(leds, NUM_LEDS, CRGB(128, 0, 0)); // Red for await error
    }
    
    FastLED.show();
}

void loop() {
    // TUTORIAL: Cycle between different async approaches every 10 seconds
    // This allows you to see both promise-based and await-based patterns in action
    // The LEDs provide visual feedback about which approach succeeded
    
    unsigned long current_time = millis();
    
    // Switch approaches every 10 seconds
    // 4 different approaches: Promise, Await, JSON Promise, JSON Await
    if (current_time - last_request_time >= 10000) {
        last_request_time = current_time;
        
        // Cycle through 4 different approaches
        if (approach_counter % 4 == 0) {
            test_promise_approach();
            FL_WARN("CYCLE: Demonstrated Promise-based pattern (Green LEDs on success)");
        } else if (approach_counter % 4 == 1) {
            test_await_approach();
            FL_WARN("CYCLE: Demonstrated Await-based pattern (Blue LEDs on success)");
        } else if (approach_counter % 4 == 2) {
            test_json_response();
            FL_WARN("CYCLE: Demonstrated JSON Promise pattern (Blue LEDs on success)");
        } else if (approach_counter % 4 == 3) {
            test_json_await();
            FL_WARN("CYCLE: Demonstrated JSON Await pattern (Cyan LEDs on success)");
        }
        
        approach_counter++;
        
        FL_WARN("NEXT: Will switch to next approach in 10 seconds...");
    }

    // TUTORIAL NOTE: Async operations are automatically managed!
    // * On WASM: delay() pumps async tasks every 1ms automatically
    // * On all platforms: FastLED.show() triggers async updates via engine events
    // * No manual async updates needed - everything happens behind the scenes!


    // TUTORIAL: This delay automatically pumps async tasks on WASM!
    // The delay is broken into 1ms chunks with async processing between chunks.
    // This isn't necessary when calling the await approach, but is critical
    // the standard promise.then() approach.
    // Note: In the future loop() may become a macro to inject auto pumping
    // of the async tasks.
    delay(10);
}
