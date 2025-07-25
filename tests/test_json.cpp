#include "test.h"
#include <FastLED.h>
#include "fl/json.h" // Assumed ideal JSON API header

// Define these within the test scope or globally if needed for other tests
#define NUM_LEDS 100
#define DATA_PIN 3

// CRGB leds[NUM_LEDS]; // Not strictly needed for this test, as we're only testing JSON parsing

TEST_CASE("Json.ino example as a test case [json]") {
    // The original setup() content from Json.ino
    // Serial.begin(115200); // Not needed in a unit test environment

    // Initialize FastLED (mock or actual, depending on test setup)
    // FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS); // Not strictly needed for this test
    // FastLED.setBrightness(64); // Not strictly needed for this test

    // Example JSON string with LED configuration
    const char* configJson = R"({
        "strip": {
            "num_leds": 150,
            "pin": 5,
            "type": "WS2812B",
            "brightness": 200
        },
        "effects": {
            "current": "rainbow",
            "speed": 75
        },
        "animation_settings": {
            "duration_ms": 5000,
            "loop": true
        }
    })";

    // NEW: Parse using ideal API
    fl::Json json = fl::Json::parse(configJson);

    REQUIRE(json.has_value() == true); // Assert that JSON parsing was successful

    // NEW: Clean syntax with default values - no more verbose error checking!
    int numLeds = json["strip"]["num_leds"] | 100;      // Gets 150, or 100 if missing
    int pin = json["strip"]["pin"] | 3;                 // Gets 5, or 3 if missing
    fl::string type = json["strip"]["type"] | fl::string("WS2812");  // Gets "WS2812B"
    int brightness = json["strip"]["brightness"] | 64;  // Gets 200, or 64 if missing

    // Safe access to missing values - no crashes!
    int missing = json["non_existent"]["missing"] | 999;  // Gets 999

    // Assertions for the extracted values
    REQUIRE(numLeds == 150);
    REQUIRE(pin == 5);
    REQUIRE(type == "WS2812B");
    REQUIRE(brightness == 200);
    REQUIRE(missing == 999);

    // Effect configuration with safe defaults
    fl::string effect = json["effects"]["current"] | fl::string("solid");
    int speed = json["effects"]["speed"] | 50;

    REQUIRE(effect == "rainbow");
    REQUIRE(speed == 75);

    // Accessing nested objects with defaults
    long duration = json["animation_settings"]["duration_ms"] | 1000;
    bool loop = json["animation_settings"]["loop"] | false;

    REQUIRE(duration == 5000);
    REQUIRE(loop == true);

    // The Serial.println statements from the original example can be replaced with INFO or CHECK
    // for better test reporting, or removed if not directly asserting.
    // For now, keeping them as comments to show where they would go.
    /*
    INFO("LED Strip Configuration:");
    INFO("  LEDs: " << numLeds);
    INFO("  Pin: " << pin);
    INFO("  Type: " << type.c_str());
    INFO("  Brightness: " << brightness);
    INFO("  Missing field default: " << missing);

    INFO("Effect Configuration:");
    INFO("  Current: " << effect.c_str());
    INFO("  Speed: " << speed);

    INFO("Animation Settings:");
    INFO("  Duration (ms): " << static_cast<int32_t>(duration));
    INFO("  Loop: " << (loop ? "true" : "false"));
    */
}
