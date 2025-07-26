/// @file JsonIdeal.ino
/// @brief Demonstrates an ideal fluid JSON API with FastLED
///
/// This example showcases the proposed ideal JSON API for FastLED,
/// emphasizing clean syntax, type safety, default values, and robust
/// handling of missing fields.

#include "FastLED.h"
#include "fl/json_new.h" // Using the new JSON API

#define NUM_LEDS 100
#define DATA_PIN 3

fl::CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);

    // Initialize FastLED
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);

    Serial.println("FastLED Ideal JSON API Demo Starting...");

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

    // Parse using the new API
    fl::Json json = fl::Json::parse(fl::string(configJson));

    if (!json.is_null()) {
        Serial.println("JSON parsed successfully with new API!");

        // Clean syntax with default values - no more verbose error checking!
        int64_t numLeds = json["strip"]["num_leds"] | int64_t(100);      // Gets 150, or 100 if missing
        int64_t pin = json["strip"]["pin"] | int64_t(3);                 // Gets 5, or 3 if missing
        fl::string type = json["strip"]["type"] | fl::string("WS2812");  // Gets "WS2812B"
        int64_t brightness = json["strip"]["brightness"] | int64_t(64);  // Gets 200, or 64 if missing

        // Safe access to missing values - no crashes!
        int64_t missing = json["non_existent"]["missing"] | int64_t(999);  // Gets 999

        Serial.println("LED Strip Configuration:");
        Serial.print("  LEDs: "); Serial.println(static_cast<int>(numLeds));
        Serial.print("  Pin: "); Serial.println(static_cast<int>(pin));
        Serial.print("  Type: "); Serial.println(type.c_str());
        Serial.print("  Brightness: "); Serial.println(static_cast<int>(brightness));
        Serial.print("  Missing field default: "); Serial.println(static_cast<int>(missing));

        // Effect configuration with safe defaults
        fl::string effect = json["effects"]["current"] | fl::string("solid");
        int64_t speed = json["effects"]["speed"] | int64_t(50);

        Serial.println("Effect Configuration:");
        Serial.print("  Current: "); Serial.println(effect.c_str());
        Serial.print("  Speed: "); Serial.println(static_cast<int>(speed));

        // Accessing nested objects with defaults
        int64_t duration = json["animation_settings"]["duration_ms"] | int64_t(1000);
        bool loop = json["animation_settings"]["loop"] | false;

        Serial.println("Animation Settings:");
        Serial.print("  Duration (ms): "); Serial.println(static_cast<int>(duration));
        Serial.print("  Loop: "); Serial.println(loop ? "true" : "false");

        // Demonstrate serialization back to string
        fl::string serialized = json.to_string();
        Serial.println("Serialized JSON:");
        Serial.println(serialized.c_str());

        Serial.println("\nNew API provides:");
        Serial.println("  ✓ Type safety");
        Serial.println("  ✓ Default values");
        Serial.println("  ✓ No crashes on missing fields");
        Serial.println("  ✓ Clean, readable syntax");
        Serial.println("  ✓ Significantly less code for common operations");

    } else {
        Serial.println("JSON parsing failed with new API");
    }
}

void loop() {
    // Simple LED pattern to show something is happening
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(50);
}
