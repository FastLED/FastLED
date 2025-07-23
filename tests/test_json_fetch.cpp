// Unit tests for fl::json functionality
// Tests basic JSON parsing, serialization, and type inspection

#include "test.h"
#include "fl/json.h"
#include "fl/str.h"
#include "crgb.h"  // For CRGB type testing

using namespace fl;

TEST_CASE("fl::json - Basic JSON parsing") {
    SUBCASE("Parse valid JSON object") {
        const char* jsonStr = R"({"name": "test", "value": 42, "active": true})";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        CHECK(error.empty());
        
#if FASTLED_ENABLE_JSON
        // Test accessing parsed values
        CHECK_EQ(fl::string(doc["name"].as<const char*>()), fl::string("test"));
        CHECK_EQ(doc["value"].as<int>(), 42);
        CHECK(doc["active"].as<bool>());
#endif
    }
    
    SUBCASE("Parse valid JSON array") {
        const char* jsonStr = R"([1, 2, 3, "hello", true])";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        CHECK(error.empty());
        
#if FASTLED_ENABLE_JSON
        // Test accessing array elements
        CHECK_EQ(doc[0].as<int>(), 1);
        CHECK_EQ(doc[1].as<int>(), 2);
        CHECK_EQ(doc[2].as<int>(), 3);
        CHECK_EQ(fl::string(doc[3].as<const char*>()), fl::string("hello"));
        CHECK(doc[4].as<bool>());
#endif
    }
    
    SUBCASE("Parse nested JSON structure") {
        const char* jsonStr = R"({
            "config": {
                "brightness": 128,
                "color": {"r": 255, "g": 0, "b": 128}
            },
            "modes": ["rainbow", "solid", "sparkle"]
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        CHECK(error.empty());
        
#if FASTLED_ENABLE_JSON
        // Test nested object access
        CHECK_EQ(doc["config"]["brightness"].as<int>(), 128);
        CHECK_EQ(doc["config"]["color"]["r"].as<int>(), 255);
        CHECK_EQ(doc["config"]["color"]["g"].as<int>(), 0);
        CHECK_EQ(doc["config"]["color"]["b"].as<int>(), 128);
        
        // Test nested array access
        CHECK_EQ(fl::string(doc["modes"][0].as<const char*>()), fl::string("rainbow"));
        CHECK_EQ(fl::string(doc["modes"][1].as<const char*>()), fl::string("solid"));
        CHECK_EQ(fl::string(doc["modes"][2].as<const char*>()), fl::string("sparkle"));
#endif
    }
}

TEST_CASE("fl::json - JSON parsing error handling") {
    SUBCASE("Parse invalid JSON - missing quotes") {
        const char* jsonStr = R"({name: test, value: 42})";  // Missing quotes around name and test
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK_FALSE(success);
        CHECK_FALSE(error.empty());
    }
    
    SUBCASE("Parse invalid JSON - trailing comma") {
        const char* jsonStr = R"({"name": "test", "value": 42,})";  // Trailing comma
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK_FALSE(success);
        CHECK_FALSE(error.empty());
    }
    
    SUBCASE("Parse empty string") {
        const char* jsonStr = "";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK_FALSE(success);
        CHECK_FALSE(error.empty());
    }
    
    SUBCASE("Parse null pointer") {
        const char* jsonStr = nullptr;
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK_FALSE(success);
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("fl::json - JSON serialization") {
    SUBCASE("Serialize simple object") {
#if FASTLED_ENABLE_JSON
        fl::JsonDocument doc;
        doc["name"] = "test";
        doc["value"] = 42;
        doc["active"] = true;
        
        fl::string jsonBuffer;
        fl::toJson(doc, &jsonBuffer);
        
        CHECK_FALSE(jsonBuffer.empty());
        
        // Should contain expected key-value pairs (order may vary)
        CHECK(jsonBuffer.find("\"name\"") != fl::string::npos);
        CHECK(jsonBuffer.find("\"test\"") != fl::string::npos);
        CHECK(jsonBuffer.find("\"value\"") != fl::string::npos);
        CHECK(jsonBuffer.find("42") != fl::string::npos);
        CHECK(jsonBuffer.find("\"active\"") != fl::string::npos);
        CHECK(jsonBuffer.find("true") != fl::string::npos);
#else
        // When JSON is disabled, toJson should not crash but may not produce output
        fl::JsonDocument doc;
        fl::string jsonBuffer;
        fl::toJson(doc, &jsonBuffer);
        CHECK(true); // Just verify no crash
#endif
    }
    
    SUBCASE("Serialize array") {
#if FASTLED_ENABLE_JSON
        fl::JsonDocument doc;
        auto array = doc.to<FLArduinoJson::JsonArray>();
        array.add("item1");
        array.add(123);
        array.add(false);
        
        fl::string jsonBuffer;
        fl::toJson(doc, &jsonBuffer);
        
        CHECK_FALSE(jsonBuffer.empty());
        CHECK(jsonBuffer.find("\"item1\"") != fl::string::npos);
        CHECK(jsonBuffer.find("123") != fl::string::npos);
        CHECK(jsonBuffer.find("false") != fl::string::npos);
#endif
    }
}

TEST_CASE("fl::json - JSON type inspection") {
    SUBCASE("Check JSON types") {
        const char* jsonStr = R"({
            "string_val": "hello",
            "int_val": 42,
            "float_val": 3.14,
            "bool_val": true,
            "null_val": null,
            "array_val": [1, 2, 3],
            "object_val": {"nested": "value"}
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        // Test type detection using getJsonTypeStr
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["string_val"])), fl::string("string"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["int_val"])), fl::string("integer"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["float_val"])), fl::string("float"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["bool_val"])), fl::string("boolean"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["null_val"])), fl::string("null"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["array_val"])), fl::string("array"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["object_val"])), fl::string("object"));
        
        // Test undefined/missing key
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["nonexistent"])), fl::string("null"));
#endif
    }
}

TEST_CASE("fl::json - Real-world LED configuration scenarios") {
    SUBCASE("LED strip configuration") {
        const char* configJson = R"({
            "strip": {
                "num_leds": 100,
                "pin": 3,
                "type": "WS2812",
                "brightness": 128
            },
            "effects": {
                "current": "rainbow",
                "speed": 50,
                "palette": "rainbow"
            }
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        bool success = fl::parseJson(configJson, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        // Simulate processing LED configuration
        int numLeds = doc["strip"]["num_leds"].as<int>();
        int pin = doc["strip"]["pin"].as<int>();
        int brightness = doc["strip"]["brightness"].as<int>();
        fl::string ledType = doc["strip"]["type"].as<const char*>();
        
        CHECK_EQ(numLeds, 100);
        CHECK_EQ(pin, 3);
        CHECK_EQ(brightness, 128);
        CHECK_EQ(ledType, fl::string("WS2812"));
        
        fl::string currentEffect = doc["effects"]["current"].as<const char*>();
        int speed = doc["effects"]["speed"].as<int>();
        
        CHECK_EQ(currentEffect, fl::string("rainbow"));
        CHECK_EQ(speed, 50);
#endif
    }
    
    SUBCASE("Color palette data") {
        const char* paletteJson = R"({
            "palette": {
                "name": "sunset",
                "colors": [
                    {"r": 255, "g": 94, "b": 0},
                    {"r": 255, "g": 154, "b": 0},
                    {"r": 255, "g": 206, "b": 84}
                ]
            }
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        bool success = fl::parseJson(paletteJson, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        fl::string paletteName = doc["palette"]["name"].as<const char*>();
        CHECK_EQ(paletteName, fl::string("sunset"));
        
        // Process color array
        auto colors = doc["palette"]["colors"];
        CHECK_EQ(colors.size(), 3);
        
        // Check first color
        int r = colors[0]["r"].as<int>();
        int g = colors[0]["g"].as<int>();
        int b = colors[0]["b"].as<int>();
        CHECK_EQ(r, 255);
        CHECK_EQ(g, 94);
        CHECK_EQ(b, 0);
        
        // Check second color
        r = colors[1]["r"].as<int>();
        g = colors[1]["g"].as<int>();
        b = colors[1]["b"].as<int>();
        CHECK_EQ(r, 255);
        CHECK_EQ(g, 154);
        CHECK_EQ(b, 0);
        
        // Check third color
        r = colors[2]["r"].as<int>();
        g = colors[2]["g"].as<int>();
        b = colors[2]["b"].as<int>();
        CHECK_EQ(r, 255);
        CHECK_EQ(g, 206);
        CHECK_EQ(b, 84);
#endif
    }
}

TEST_CASE("fl::json - Edge cases and robustness") {
    SUBCASE("Very large numbers") {
        const char* jsonStr = R"({"large_int": 2147483647, "large_float": 1.7976931348623157e+308})";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        CHECK_EQ(doc["large_int"].as<long>(), 2147483647L);
        // Note: Very large floats may have precision limitations
        CHECK(doc["large_float"].as<double>() > 1e100);
#endif
    }
    
    SUBCASE("Unicode strings") {
        const char* jsonStr = R"({"unicode": "Hello 🌈 World", "emoji": "✨⭐🎨"})";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        fl::string unicode = doc["unicode"].as<const char*>();
        fl::string emoji = doc["emoji"].as<const char*>();
        
        CHECK_FALSE(unicode.empty());
        CHECK_FALSE(emoji.empty());
        CHECK(unicode.find("Hello") != fl::string::npos);
        CHECK(unicode.find("World") != fl::string::npos);
#endif
    }
    
    SUBCASE("Empty structures") {
        const char* jsonStr = R"({"empty_object": {}, "empty_array": []})";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK(success);
        
#if FASTLED_ENABLE_JSON
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["empty_object"])), fl::string("object"));
        CHECK_EQ(fl::string(fl::getJsonTypeStr(doc["empty_array"])), fl::string("array"));
        CHECK_EQ(doc["empty_array"].size(), 0);
#endif
    }
}

#if !FASTLED_ENABLE_JSON
TEST_CASE("fl::json - Disabled JSON functionality") {
    SUBCASE("parseJson returns false when JSON disabled") {
        const char* jsonStr = R"({"test": "value"})";
        fl::JsonDocument doc;
        fl::string error;
        
        bool success = fl::parseJson(jsonStr, &doc, &error);
        CHECK_FALSE(success);
        CHECK_FALSE(error.empty());
        CHECK(error.find("not enabled") != fl::string::npos);
    }
    
    SUBCASE("toJson does not crash when JSON disabled") {
        fl::JsonDocument doc;
        fl::string jsonBuffer;
        
        // Should not crash, but may not produce meaningful output
        fl::toJson(doc, &jsonBuffer);
        CHECK(true); // Just verify no crash occurred
    }
}
#endif

#if FASTLED_ENABLE_JSON
TEST_CASE("fl::json - Simple Ideal API Demo") {
    SUBCASE("Basic Json class usage") {
        const char* jsonStr = R"({"name": "test", "value": 42})";
        
        // Parse using new ideal API
        Json json = Json::parse(jsonStr);
        CHECK(json.has_value());
        
        // Default value access (the key feature)
        int value = json["value"] | 0;          // Gets 42
        int missing = json["missing"] | 999;    // Gets 999 (default)
        string name = json["name"] | string("default");  // Gets "test"
        
        CHECK_EQ(value, 42);
        CHECK_EQ(missing, 999);
        CHECK_EQ(name, "test");
    }
    
    SUBCASE("JsonBuilder basic usage") {
        auto json = JsonBuilder()
            .set("brightness", 128)
            .set("enabled", true)
            .set("name", "test_strip")
            .build();
        
        // Test the built values using the ideal API
        int brightness = json["brightness"] | 0;
        bool enabled = json["enabled"] | false;
        string name = json["name"] | string("");
        
        CHECK_EQ(brightness, 128);
        CHECK(enabled);
        CHECK_EQ(name, "test_strip");
    }
}
#endif 

#if FASTLED_ENABLE_JSON
TEST_CASE("fl::json - FastLED Integration") {
    SUBCASE("Color values as numbers") {
        // Test that JsonBuilder can store color values as numbers
        JsonBuilder builder;
        builder.set("red_color", 16711680);   // 0xFF0000 = Red (255,0,0)
        builder.set("green_color", 65280);     // 0x00FF00 = Green (0,255,0)
        builder.set("blue_color", 255);        // 0x0000FF = Blue (0,0,255)
        
        // Build and test numeric access
        auto json = builder.build();
        
        // CRGB colors are stored as decimal numbers
        // Red (255,0,0) = 0xFF0000 = 16711680
        // Green (0,255,0) = 0x00FF00 = 65280  
        // Blue (0,0,255) = 0x0000FF = 255
        
        // Test that colors are accessible as numbers
        auto red_num = json["red_color"].get<int>();
        auto green_num = json["green_color"].get<int>();
        auto blue_num = json["blue_color"].get<int>();
        
        CHECK(red_num.has_value());
        CHECK(green_num.has_value()); 
        CHECK(blue_num.has_value());
        
        // Verify the actual numeric values  
        if (red_num.has_value()) {
            CHECK_EQ(*red_num, 16711680);  // 0xFF0000
        }
        if (green_num.has_value()) {
            CHECK_EQ(*green_num, 65280);   // 0x00FF00
        }
        if (blue_num.has_value()) {
            CHECK_EQ(*blue_num, 255);      // 0x0000FF
        }
        
        // Test mixed types still work
        builder.set("brightness", 128);
        builder.set("enabled", true);
        
        auto mixed_json = builder.build();
        int brightness = mixed_json["brightness"] | 0;
        bool enabled = mixed_json["enabled"] | false;
        
        CHECK_EQ(brightness, 128);
        CHECK(enabled);
    }
    
    // TODO: Fix stack overflow in get_flexible method
    // SUBCASE("Flexible numeric type handling") {
        // Test that floats and ints can be accessed as either type
        /*
        JsonBuilder builder;
        builder.set("float_value", 42.5f);      // Store as float
        builder.set("int_as_float", 128.0f);    // Store int value as float  
        builder.set("pure_int", 256);           // Store as int
        
        auto json = builder.build();
        
        // Test that float value can be accessed as both types
        auto float_val = json["float_value"];
        
        // ArduinoJSON allows flexible numeric access
        CHECK(float_val.has_value());
        
        // Test accessing float as float (this should work)
        auto as_float = float_val.get<float>();
        CHECK(as_float.has_value());
        if (as_float.has_value()) {
            CHECK_EQ(*as_float, 42.5f);
        }
        
        // Current implementation uses strict type checking
        // So this demonstrates the strict behavior
        auto as_int_strict = float_val.get<int>();
        FL_WARN("Strict int access of float: " << (as_int_strict.has_value() ? "works" : "fails"));
        
        // NEW: Test the flexible method that allows cross-type access
        auto as_int_flexible = float_val.get_flexible<int>();
        FL_WARN("Flexible int access of float: " << (as_int_flexible.has_value() ? "works" : "fails"));
        if (as_int_flexible.has_value()) {
            CHECK_EQ(*as_int_flexible, 42);  // 42.5 truncated to 42
            FL_WARN("Float 42.5 accessed as int via get_flexible: " << *as_int_flexible);
        }
        
        // But ArduinoJSON's underlying as<T>() is more flexible
        // Let's test direct access to the variant to show flexible behavior
        if (float_val.has_value()) {
            // Access the underlying variant directly 
            auto& variant = float_val.variant();
            
            // Test if ArduinoJSON can convert float to int
            bool can_be_int = !variant.isNull();
            if (can_be_int) {
                // Try to get as int - ArduinoJSON should handle conversion
                int int_value = variant.as<int>();  // Should truncate 42.5 to 42
                FL_WARN("Float 42.5 converted to int: " << int_value);
                CHECK_EQ(int_value, 42);  // 42.5 truncated to 42
            }
            
            // Test if ArduinoJSON can convert back
            float float_from_variant = variant.as<float>();
            CHECK_EQ(float_from_variant, 42.5f);
        }
        
        // Test pure int can be accessed as float using direct variant access
        auto pure_int_val = json["pure_int"];
        if (pure_int_val.has_value()) {
            auto& variant = pure_int_val.variant();
            
            // Direct access should allow int->float conversion
            float float_from_int = variant.as<float>();
            CHECK_EQ(float_from_int, 256.0f);
            FL_WARN("Int 256 converted to float: " << float_from_int);
            
            // And int->int should still work
            int int_from_int = variant.as<int>();  
            CHECK_EQ(int_from_int, 256);
        }
        */
    // }
}
#endif 
