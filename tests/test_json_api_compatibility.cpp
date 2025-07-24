#include "test.h"
#include "fl/json.h"

TEST_CASE("JSON API Compatibility - FLArduinoJson Pattern Matching") {
    SUBCASE("Template is<T>() methods - Perfect 1:1 API compatibility") {
        // Test template is<T>() matches FLArduinoJson patterns exactly (NO search & replace needed!)
        fl::Json json = fl::Json::parse(R"({"string":"hello","int":42,"float":3.14,"bool":true,"null":null})");
        
        // Template type checking - matches FLArduinoJson exactly
        auto stringValue = json["string"];
        CHECK(stringValue.is<const char*>());  // ✅ EXACT match: value.is<const char*>()
        CHECK_FALSE(stringValue.is<int>());
        CHECK_FALSE(stringValue.is<float>());
        CHECK_FALSE(stringValue.is<bool>());
        
        // Integer type checking  
        auto intValue = json["int"];
        CHECK(intValue.is<int>());  // ✅ EXACT match: value.is<int>()
        CHECK_FALSE(intValue.is<const char*>());
        CHECK_FALSE(intValue.is<float>());
        CHECK_FALSE(intValue.is<bool>());
        
        // Float type checking
        auto floatValue = json["float"];
        CHECK(floatValue.is<float>());  // ✅ EXACT match: value.is<float>()
        CHECK(floatValue.is<double>());  // ✅ Also works for double
        CHECK_FALSE(floatValue.is<const char*>());
        CHECK_FALSE(floatValue.is<int>());
        CHECK_FALSE(floatValue.is<bool>());
        
        // Boolean type checking
        auto boolValue = json["bool"];
        CHECK(boolValue.is<bool>());  // ✅ EXACT match: value.is<bool>()
        CHECK_FALSE(boolValue.is<const char*>());
        CHECK_FALSE(boolValue.is<int>());
        CHECK_FALSE(boolValue.is<float>());
        
        // Null type checking
        auto nullValue = json["null"];
        CHECK(nullValue.isNull());
        CHECK_FALSE(nullValue.is<const char*>());
        CHECK_FALSE(nullValue.is<int>());
        CHECK_FALSE(nullValue.is<float>());
        CHECK_FALSE(nullValue.is<bool>());
    }
    
    SUBCASE("Individual type checking methods (for comparison)") {
        // Test individual methods still work (but template is preferred for compatibility)
        fl::Json json = fl::Json::parse(R"({"string":"hello","int":42,"float":3.14,"bool":true,"null":null})");
        
        // String type checking
        auto stringValue = json["string"];
        CHECK(stringValue.is_string());  // Still works but template is<const char*>() is preferred
        CHECK_FALSE(stringValue.is_int());
        CHECK_FALSE(stringValue.is_float());
        CHECK_FALSE(stringValue.is_bool());
        
        // Integer type checking  
        auto intValue = json["int"];
        CHECK(intValue.is_int());  // Still works but template is<int>() is preferred
        CHECK_FALSE(intValue.is_string());
        CHECK_FALSE(intValue.is_float());
        CHECK_FALSE(intValue.is_bool());
        
        // Float type checking
        auto floatValue = json["float"];
        CHECK(floatValue.is_float());  // Still works but template is<float>() is preferred
        CHECK_FALSE(floatValue.is_string());
        CHECK_FALSE(floatValue.is_int());
        CHECK_FALSE(floatValue.is_bool());
        
        // Boolean type checking
        auto boolValue = json["bool"];
        CHECK(boolValue.is_bool());  // Still works but template is<bool>() is preferred
        CHECK_FALSE(boolValue.is_string());
        CHECK_FALSE(boolValue.is_int());
        CHECK_FALSE(boolValue.is_float());
    }
    
    SUBCASE("as<T>() methods for value extraction") {
        fl::Json json = fl::Json::parse(R"({"string":"hello","int":42,"float":3.14,"bool":true})");
        
        // Test value.as<type>() → value.as_type() or value | default patterns
        CHECK_EQ(json["string"].as<fl::string>(), fl::string("hello"));
        CHECK_EQ(json["int"].as<int>(), 42);
        CHECK_EQ(json["float"].as<float>(), 3.14f);
        CHECK_EQ(json["bool"].as<bool>(), true);
        
        // Test with defaults using operator|
        CHECK_EQ(json["string"] | fl::string("default"), fl::string("hello"));
        CHECK_EQ(json["int"] | 0, 42);
        CHECK_EQ(json["float"] | 0.0f, 3.14f);
        CHECK_EQ(json["bool"] | false, true);
        
        // Test defaults for missing keys
        CHECK_EQ(json["missing"] | fl::string("default"), fl::string("default"));
        CHECK_EQ(json["missing"] | 999, 999);
        CHECK_EQ(json["missing"] | 9.99f, 9.99f);
        CHECK_EQ(json["missing"] | true, true);
    }
    
    SUBCASE("Array building with different value types") {
        auto json = fl::Json::createArray();
        
        // Test json.add() patterns (matches FLArduinoJson array.add())
        json.add("string_item");
        json.add(123);
        json.add(4.56f);
        json.add(true);
        
        // Also test push_back variants
        json.push_back("another_string");
        json.push_back(789);
        json.push_back(7.89f);
        json.push_back(false);
        
        CHECK_EQ(json.getSize(), 8);
        CHECK(json.is_array());
        
        // Verify values can be read back
        CHECK_EQ(json[0] | fl::string(""), fl::string("string_item"));
        CHECK_EQ(json[1] | 0, 123);
        CHECK_EQ(json[2] | 0.0f, 4.56f);
        CHECK_EQ(json[3] | false, true);
        CHECK_EQ(json[4] | fl::string(""), fl::string("another_string"));
        CHECK_EQ(json[5] | 0, 789);
        CHECK_EQ(json[6] | 0.0f, 7.89f);
        CHECK_EQ(json[7] | true, false);
    }
    
    SUBCASE("Object building with set() method") {
        auto json = fl::Json::createObject();
        
        // Test json["key"] = value → json.set(key, value) patterns
        json.set("name", "test_object");
        json.set("id", 42);
        json.set("enabled", true);
        json.set("ratio", 3.14f);
        
        CHECK(json.is_object());
        CHECK_EQ(json.getSize(), 4);
        
        // Verify values can be read back
        CHECK_EQ(json["name"] | fl::string(""), fl::string("test_object"));
        CHECK_EQ(json["id"] | 0, 42);
        CHECK_EQ(json["enabled"] | false, true);
        CHECK_EQ(json["ratio"] | 0.0f, 3.14f);
    }
    
    SUBCASE("Nested object/array creation patterns") {
        auto json = fl::Json::createObject();
        
        // Test json.createNestedObject() patterns (matches FLArduinoJson)
        auto nested_obj = json.createNestedObject("config");
        nested_obj.set("width", 800);
        nested_obj.set("height", 600);
        
        // Test json.createNestedArray() patterns
        auto nested_array = json.createNestedArray("items");
        nested_array.add("item1");
        nested_array.add("item2");
        nested_array.add("item3");
        
        // Verify structure
        CHECK(json.is_object());
        CHECK_EQ(json.getSize(), 2);
        
        auto config = json["config"];
        CHECK(config.is_object());
        CHECK_EQ(config["width"] | 0, 800);
        CHECK_EQ(config["height"] | 0, 600);
        
        auto items = json["items"];
        CHECK(items.is_array());
        CHECK_EQ(items.getSize(), 3);
        CHECK_EQ(items[0] | fl::string(""), fl::string("item1"));
        CHECK_EQ(items[1] | fl::string(""), fl::string("item2"));
        CHECK_EQ(items[2] | fl::string(""), fl::string("item3"));
    }
    
    SUBCASE("Complex JSON structure building and serialization") {
        // Build a complex structure similar to what ActiveStripData might create
        auto json = fl::Json::createArray();
        
        for (int stripId : {0, 2, 5}) {
            auto stripObj = fl::Json::createObject();
            stripObj.set("strip_id", stripId);
            stripObj.set("type", "r8g8b8");
            stripObj.set("enabled", true);
            stripObj.set("brightness", 0.8f);
            
            // Add nested array
            auto pixels = stripObj.createNestedArray("pixels");
            pixels.add(255);
            pixels.add(128);
            pixels.add(64);
            
            json.add(stripObj);
        }
        
        // Verify the structure
        CHECK(json.is_array());
        CHECK_EQ(json.getSize(), 3);
        
        // Check first strip
        auto strip0 = json[0];
        CHECK_EQ(strip0["strip_id"] | -1, 0);
        CHECK_EQ(strip0["type"] | fl::string(""), fl::string("r8g8b8"));
        CHECK_EQ(strip0["enabled"] | false, true);
        CHECK_EQ(strip0["brightness"] | 0.0f, 0.8f);
        
        auto pixels0 = strip0["pixels"];
        CHECK(pixels0.is_array());
        CHECK_EQ(pixels0.getSize(), 3);
        CHECK_EQ(pixels0[0] | 0, 255);
        
        // Test serialization produces valid JSON
        fl::string output = json.serialize();
        CHECK_FALSE(output.empty());
        CHECK(output[0] == '[');  // Should start with array bracket
        
        // Verify it can be re-parsed
        auto reparsed = fl::Json::parse(output.c_str());
        CHECK(reparsed.is_array());
        CHECK_EQ(reparsed.getSize(), 3);
        CHECK_EQ(reparsed[0]["strip_id"] | -1, 0);
    }
} 
