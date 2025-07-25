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

// Assuming the ideal JSON API is in a namespace or directly accessible
// For this test, we'll use the classes directly as defined in the previous turn.
// In a real scenario, you'd include a header like #include "fl/json_api.h"

TEST_CASE("Ideal JSON API Usage [json][ideal_api]") {
    // Create a JsonObject
    JsonObject user;

    // Set primitive values using operator[]
    user["name"] = "Alice";
    user["age"] = 30;
    user["isStudent"] = false;
    user["height"] = 1.75;
    user["null_value"] = JsonValue(); // Explicitly set a null value

    // Create and set a JsonArray
    JsonArray hobbies;
    hobbies.push_back("reading");
    hobbies.push_back("hiking");
    hobbies.push_back(JsonValue(123)); // Add an integer to the array
    user["hobbies"] = hobbies;

    // Create and set a nested JsonObject
    JsonObject address;
    address["street"] = "123 Main St";
    address["city"] = "Anytown";
    address["zip"] = 90210;
    user["address"] = address;

    // Verify values using get<T>()
    REQUIRE(user["name"].get<std::string>() == "Alice");
    REQUIRE(user["age"].get<int>() == 30);
    REQUIRE(user["isStudent"].get<bool>() == false);
    REQUIRE(user["height"].get<double>() == 1.75);
    REQUIRE(user["null_value"].is_null() == true);

    // Verify array elements
    REQUIRE(user["hobbies"][0].get<std::string>() == "reading");
    REQUIRE(user["hobbies"][1].get<std::string>() == "hiking");
    REQUIRE(user["hobbies"][2].get<int>() == 123);

    // Verify nested object elements
    REQUIRE(user["address"]["street"].get<std::string>() == "123 Main St");
    REQUIRE(user["address"]["city"].get<std::string>() == "Anytown");
    REQUIRE(user["address"]["zip"].get<int>() == 90210);

    // Test type safety: attempting to get a value with the wrong type should throw std::bad_variant_access
    bool caught_exception = false;
    try {
        user["age"].get<std::string>(); // age is int, trying to get as string
    } catch (const std::bad_variant_access& e) {
        caught_exception = true;
        // INFO("Caught expected exception: " << e.what());
    }
    REQUIRE(caught_exception == true);

    caught_exception = false;
    try {
        user["name"].get<int>(); // name is string, trying to get as int
    } catch (const std::bad_variant_access& e) {
        caught_exception = true;
        // INFO("Caught expected exception: " << e.what());
    }
    REQUIRE(caught_exception == true);

    // Test array resizing with operator[]
    JsonArray dynamic_array;
    dynamic_array[2] = "third_element"; // This should create elements at index 0 and 1 as null
    dynamic_array[0] = 100;
    dynamic_array[1] = true;

    REQUIRE(dynamic_array[0].get<int>() == 100);
    REQUIRE(dynamic_array[1].get<bool>() == true);
    REQUIRE(dynamic_array[2].get<std::string>() == "third_element");
    REQUIRE(dynamic_array[3].is_null() == true); // Accessing an unassigned element beyond the last set one

    // Test non-existent key access in JsonObject (should return a null JsonValue)
    REQUIRE(user["non_existent_key"].is_null() == true);
    REQUIRE(user["address"]["non_existent_field"].is_null() == true);

    // Test non-existent index access in JsonArray (const version)
    bool out_of_range_caught = false;
    try {
        const JsonArray& const_hobbies = user["hobbies"].get<JsonArray>();
        const_hobbies[10].get<std::string>(); // Index 10 is out of bounds
    } catch (const std::out_of_range& e) {
        out_of_range_caught = true;
        // INFO("Caught expected out_of_range exception for const array access: " << e.what());
    }
    REQUIRE(out_of_range_caught == true);

    // Optional: Print the full JSON structure (for visual inspection during development)
    // INFO("Full User JSON: " << user.to_string());
}
