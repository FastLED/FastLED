/// @file Json.ino
/// @brief JSON parsing examples with FastLED
///
/// This example demonstrates how to use the basic JSON parsing functions
/// provided by the FastLED JSON API:
/// - fl::parseJson() for parsing JSON strings
/// - fl::toJson() for serializing JSON documents
/// - fl::getJsonType() for inspecting JSON value types
/// 
/// NEW: Also demonstrates the ideal JSON API with fl::Json class

#include "FastLED.h"
#include "fl/json.h"  // Basic JSON API

#define NUM_LEDS 100
#define DATA_PIN 3

fl::CRGB leds[NUM_LEDS];

// Forward declaration
void checkJsonType(const fl::Json& value, const char* fieldName);

void setup() {
    Serial.begin(115200);
    
    // Initialize FastLED
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);
    
    Serial.println("FastLED JSON Examples Starting...");
    
    // Example 1: Basic JSON Parsing
    basicJsonParsingExample();
    
    // Example 2: JSON Type Inspection
    jsonTypeInspectionExample();
    
    // Example 3: JSON Document Creation
    jsonCreationExample();
    
    // NEW: Example 4: Ideal JSON API Demo
    idealJsonApiDemo();
}

void loop() {
    // Simple LED pattern
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(50);
}

void basicJsonParsingExample() {
    Serial.println("\n=== Basic JSON Parsing Example ===");
    
    // Example JSON string with LED configuration
    const char* jsonStr = R"({
        "brightness": 80,
        "color": {
            "r": 255,
            "g": 100,
            "b": 50
        },
        "effects": ["rainbow", "sparkle", "fade"],
        "enabled": true
    })";
    
    // Parse the JSON
    fl::JsonDocument doc;
    fl::string error;
    
    if (fl::parseJson(jsonStr, &doc, &error)) {
        Serial.println("JSON parsed successfully!");
        
        // Access values using ArduinoJson API
        int brightness = 64; // Default brightness
        if (doc["brightness"]) {
            brightness = doc["brightness"];
            Serial.print("Brightness: ");
            Serial.println(brightness);
        }
        
        if (doc["color"]) {
            auto color = doc["color"];
            int r = color["r"] | 0;
            int g = color["g"] | 0;
            int b = color["b"] | 0;
            
            Serial.print("Color: RGB(");
            Serial.print(r); Serial.print(", ");
            Serial.print(g); Serial.print(", ");
            Serial.print(b); Serial.println(")");
            
            // Set LED color based on JSON
            CRGB ledColor = CRGB(r, g, b);
            fill_solid(leds, NUM_LEDS, ledColor);
            FastLED.setBrightness(brightness);
            FastLED.show();
        }
        
        if (doc["enabled"]) {
            bool enabled = doc["enabled"];
            Serial.print("Effects enabled: ");
            Serial.println(enabled ? "true" : "false");
        }
        
        if (doc["effects"].is<FLArduinoJson::JsonArray>()) {
            auto effects = doc["effects"].as<FLArduinoJson::JsonArray>();
            Serial.print("Available effects: ");
            for (auto effect : effects) {
                Serial.print(effect.as<const char*>());
                Serial.print(" ");
            }
            Serial.println();
        }
        
    } else {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
    }
}

void jsonTypeInspectionExample() {
    Serial.println("\n=== JSON Type Inspection Example ===");
    
    const char* jsonStr = R"({
        "name": "FastLED",
        "version": 3.7,
        "stable": true,
        "features": ["ws2812", "apa102", "effects"],
        "metadata": {
            "author": "FastLED Team",
            "year": 2024
        },
        "empty": null
    })";
    
    fl::JsonDocument doc;
    fl::string error;
    
    if (fl::parseJson(jsonStr, &doc, &error)) {
        Serial.println("Inspecting JSON value types:");
        
        // Inspect each field type using fl::Json wrapper for ideal API
        auto jsonWrapper = fl::Json(doc);
        checkJsonType(jsonWrapper["name"], "name");
        checkJsonType(jsonWrapper["version"], "version");
        checkJsonType(jsonWrapper["stable"], "stable");
        checkJsonType(jsonWrapper["features"], "features");
        checkJsonType(jsonWrapper["metadata"], "metadata");
        checkJsonType(jsonWrapper["empty"], "empty");
        
    } else {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
    }
}

void checkJsonType(const fl::Json& value, const char* fieldName) {
    fl::JsonType type = value.type();
    const char* typeStr = value.type_str();
    
    Serial.print("Field '");
    Serial.print(fieldName);
    Serial.print("' is of type: ");
    Serial.println(typeStr);
    
    // Show some type-specific processing
    switch (type) {
        case fl::JSON_STRING: {
            Serial.print("  String value: ");
            auto strOpt = value.get<fl::string>();
            if (strOpt.has_value()) {
                Serial.println(strOpt->c_str());
            }
            break;
        }
        case fl::JSON_INTEGER: {
            Serial.print("  Integer value: ");
            auto intOpt = value.get<int>();
            if (intOpt.has_value()) {
                Serial.println(*intOpt);
            }
            break;
        }
        case fl::JSON_FLOAT: {
            Serial.print("  Float value: ");
            auto floatOpt = value.get<float>();
            if (floatOpt.has_value()) {
                Serial.println(*floatOpt);
            }
            break;
        }
        case fl::JSON_ARRAY:
            Serial.print("  Array size: ");
            Serial.println(value.size());
            break;
        case fl::JSON_OBJECT:
            Serial.print("  Object size: ");
            Serial.println(value.size());
            break;
        default:
            Serial.println("  (no additional info)");
            break;
    }
}

void jsonCreationExample() {
    Serial.println("\n=== JSON Creation Example ===");
    
    // Create a JSON document
    fl::JsonDocument doc;
    
    // Add various types of data
    doc["device"] = "FastLED Controller";
    doc["num_leds"] = NUM_LEDS;
    doc["brightness"] = 64;
    doc["active"] = true;
    
    // Create nested object
    auto status = doc["status"].to<FLArduinoJson::JsonObject>();
    status["temperature"] = 23.5;
    status["voltage"] = 5.0;
    status["errors"] = 0;
    
    // Create array
    auto modes = doc["modes"].to<FLArduinoJson::JsonArray>();
    modes.add("rainbow");
    modes.add("solid");
    modes.add("sparkle");
    
    // Serialize to string
    fl::string jsonOutput;
    fl::toJson(doc, &jsonOutput);
    
    Serial.println("Generated JSON:");
    Serial.println(jsonOutput.c_str());
    
    // Pretty print option (if you need formatted output)
    Serial.println("\nPretty formatted JSON:");
    Serial.flush();  // Ensure previous output is sent
    serializeJsonPretty(doc, Serial);
    Serial.println();
}

void idealJsonApiDemo() {
    Serial.println("\n=== NEW: Ideal JSON API Demo ===");
    
    // Example showing the improved ergonomics of the ideal API
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
        }
    })";
    
    // NEW: Parse using ideal API
    fl::Json json = fl::Json::parse(configJson);
    
    if (json.has_value()) {
        Serial.println("Parsed with ideal API!");
        
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
        
        // Compare old vs new API
        Serial.println("\n--- API Comparison ---");
        
        // OLD API (verbose and error-prone):
        fl::JsonDocument oldDoc;
        fl::string error;
        if (fl::parseJson(configJson, &oldDoc, &error)) {
            int oldNumLeds = oldDoc["strip"]["num_leds"].as<int>();  // Can crash if missing
            Serial.print("Old API num_leds: "); Serial.println(oldNumLeds);
        }
        
        // NEW API (clean and safe):
        int newNumLeds = json["strip"]["num_leds"] | 0;  // Never crashes
        Serial.print("New API num_leds: "); Serial.println(newNumLeds);
        
        Serial.println("New API provides:");
        Serial.println("  ✓ Type safety");
        Serial.println("  ✓ Default values"); 
        Serial.println("  ✓ No crashes on missing fields");
        Serial.println("  ✓ Clean, readable syntax");
        Serial.println("  ✓ 50% less code for common operations");
        
    } else {
        Serial.println("JSON parsing failed with ideal API");
    }
} 
