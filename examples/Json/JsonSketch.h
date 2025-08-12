/// @file JsonIdeal.ino
/// @brief Demonstrates an ideal fluid JSON API with FastLED
///
/// This example showcases the proposed ideal JSON API for FastLED,
/// emphasizing clean syntax, type safety, default values, and robust
/// handling of missing fields.

#include <Arduino.h>
#include "FastLED.h"
#include "fl/json.h" // Assumed ideal JSON API header

#define NUM_LEDS 100
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

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

    // NEW: Parse using ideal API
    fl::Json json = fl::Json::parse(configJson);

    if (json.has_value()) {
        Serial.println("JSON parsed successfully with ideal API!");

        // NEW: Clean syntax with default values - no more verbose error checking!
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
        long duration = json["animation_settings"]["duration_ms"] | 1000;
        bool loop = json["animation_settings"]["loop"] | false;

        Serial.println("Animation Settings:");
        Serial.print("  Duration (ms): "); Serial.println(static_cast<int32_t>(duration));
        Serial.print("  Loop: "); Serial.println(loop ? "true" : "false");

        Serial.println("\n=== NEW ERGONOMIC API DEMONSTRATION ===");
        
        // Example JSON with mixed data types including strings that can be converted
        const char* mixedJson = R"({
            "config": {
                "brightness": "128",
                "timeout": "5.5",
                "enabled": true,
                "name": "LED Strip"
            }
        })";
        
        fl::Json config = fl::Json::parse(mixedJson);
        
        Serial.println("\nThree New Conversion Methods:");
        
        // Method 1: try_as<T>() - Explicit optional handling
        Serial.println("\n1. try_as<T>() - When you need explicit error handling:");
        auto maybeBrightness = config["config"]["brightness"].try_as<int>();
        if (maybeBrightness.has_value()) {
            Serial.print("   Brightness converted from string: ");
            Serial.println(*maybeBrightness);
        } else {
            Serial.println("   Brightness conversion failed");
        }
        
        // Method 2: value<T>() - Direct conversion with sensible defaults
        Serial.println("\n2. value<T>() - When you want defaults and don't care about failure:");
        int brightnessDirect = config["config"]["brightness"].value<int>();
        int missingDirect = config["missing_field"].value<int>();
        Serial.print("   Brightness (from string): ");
        Serial.println(brightnessDirect);
        Serial.print("   Missing field (default 0): ");
        Serial.println(missingDirect);
        
        // Method 3: as_or<T>(default) - Custom defaults
        Serial.println("\n3. as_or<T>(default) - When you want custom defaults:");
        int customBrightness = config["config"]["brightness"].as_or<int>(255);
        int customMissing = config["missing_field"].as_or<int>(100);
        double timeout = config["config"]["timeout"].as_or<double>(10.0);
        Serial.print("   Brightness with custom default: ");
        Serial.println(customBrightness);
        Serial.print("   Missing with custom default: ");
        Serial.println(customMissing);
        Serial.print("   Timeout (string to double): ");
        Serial.println(timeout);
        
        Serial.println("\nNew API provides:");
        Serial.println("  ✓ Type safety with automatic string-to-number conversion");
        Serial.println("  ✓ Three distinct patterns for different use cases");
        Serial.println("  ✓ Backward compatibility with existing as<T>() API");
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
