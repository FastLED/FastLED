/// @file JsonIdeal.ino
/// @brief Demonstrates the ideal fluid JSON API with FastLED
///
/// This example showcases the new ideal JSON API for FastLED,
/// emphasizing clean syntax, type safety, default values, and robust
/// handling of missing fields.

#include "FastLED.h"
#include "fl/json_ideal.h" // Using the new ideal JSON API

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

    // Parse using the ideal API
    fl::JsonIdeal json = fl::JsonIdeal::parse(configJson);

    if (!json.is_null()) {
        Serial.println("JSON parsed successfully with ideal API!");

        // Clean syntax with default values - no more verbose error checking!
        int numLeds = json["strip"]["num_leds"] | 100;      // Gets 150, or 100 if missing
        int pin = json["strip"]["pin"] | 3;                 // Gets 5, or 3 if missing
        fl::string type = json["strip"]["type"] | fl::string("WS2812");  // Gets "WS2812B"
        int brightness = json["strip"]["brightness"] | 64;  // Gets 200, or 64 if missing

        // Safe access to missing values - no crashes!
        int missing = json["non_existent"]["missing"] | 999;  // Gets 999

        Serial.println("LED Strip Configuration:");
        Serial.print("  LEDs: "); Serial.println(numLeds);
        Serial.print("  Pin: "); Serial.println(pin);
        Serial.print("  Type: "); Serial.println(type.c_str());
        Serial.print("  Brightness: "); Serial.println(brightness);
        Serial.print("  Missing field default: "); Serial.println(missing);

        // Effect configuration with safe defaults
        fl::string effect = json["effects"]["current"] | fl::string("solid");
        int speed = json["effects"]["speed"] | 50;

        Serial.println("Effect Configuration:");
        Serial.print("  Current: "); Serial.println(effect.c_str());
        Serial.print("  Speed: "); Serial.println(speed);

        // Accessing nested objects with defaults
        long duration = json["animation_settings"]["duration_ms"] | 1000L;
        bool loop = json["animation_settings"]["loop"] | false;

        Serial.println("Animation Settings:");
        Serial.print("  Duration (ms): "); Serial.println(duration);
        Serial.print("  Loop: "); Serial.println(loop ? "true" : "false");

        // Demonstrate serialization back to string
        fl::string serialized = json.serialize();
        Serial.println("Serialized JSON:");
        Serial.println(serialized.c_str());

        Serial.println("\nNew API provides:");
        Serial.println("  ✓ Type safety");
        Serial.println("  ✓ Default values");
        Serial.println("  ✓ No crashes on missing fields");
        Serial.println("  ✓ Clean, readable syntax");
        Serial.println("  ✓ Significantly less code for common operations");

    } else {
        Serial.println("JSON parsing failed with ideal API");
    }
}

void loop() {
    // Simple LED pattern to show something is happening
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(50);
}