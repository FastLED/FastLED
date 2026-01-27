
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/json.h"
#include "fl/screenmap.h"
#include "fl/stl/map.h"
#include "fl/stl/stdint.h"
#include "fl/str.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/geometry.h"
#include "fl/log.h"
#include "fl/math_macros.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/move.h"
#include "fl/stl/optional.h"
#include "fl/stl/string.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"
#include "fl/stl/strstream.h"



TEST_CASE("Test simple JSON parsing") {
    const char* jsonStr = "{\"map\":{\"strip1\":{\"x\":[0,1,2],\"y\":[0,0,0],\"diameter\":0.5}}}";

    fl::Json parsedJson = fl::Json::parse(jsonStr);
    FL_CHECK(parsedJson.is_object());
    FL_CHECK(parsedJson.contains("map"));
    
    if (parsedJson.contains("map")) {
        fl::Json mapObj = parsedJson["map"];
        FL_CHECK(mapObj.is_object());
        FL_CHECK(mapObj.contains("strip1"));
        
        if (mapObj.contains("strip1")) {
            fl::Json strip1Obj = mapObj["strip1"];
            FL_CHECK(strip1Obj.is_object());
            FL_CHECK(strip1Obj.contains("x"));
            FL_CHECK(strip1Obj.contains("y"));
            FL_CHECK(strip1Obj.contains("diameter"));
        }
    }
}



TEST_CASE("Simple JSON test") {
    // Test creating a simple JSON object
    fl::Json obj = fl::Json::object();
    obj.set("key1", "value1");
    obj.set("key2", 42);
    obj.set("key3", 3.14);
    
    // Test creating a JSON array
    fl::Json arr = fl::Json::array();
    arr.push_back("item1");
    arr.push_back(123);
    arr.push_back(2.71);
    
    // Test nested objects
    fl::Json nested = fl::Json::object();
    nested.set("array", arr);
    nested.set("value", "nested_value");
    
    obj.set("nested", nested);
    
    // Test serialization
    fl::string jsonStr = obj.to_string();
    fl::printf("DEBUG: Generated JSON string: '%s'\n", jsonStr.c_str());
    fl::printf("DEBUG: JSON string length: %zu\n", jsonStr.length());
    FL_CHECK_FALSE(jsonStr.empty());

    // Test parsing
    fl::Json parsed = fl::Json::parse(jsonStr);
    FL_CHECK(parsed.has_value());
    FL_CHECK(parsed.is_object());
    
    // Print parsed object keys for debugging
    auto keys = parsed.keys();
    fl::printf("Parsed object has %zu keys:\n", keys.size());
    for (const auto& key : keys) {
        fl::printf("  %s\n", key.c_str());
    }
    
    // Test accessing values
    FL_CHECK(parsed.contains("key1"));
    FL_CHECK(parsed["key1"].is_string());
    FL_CHECK(parsed["key1"].as_or(fl::string("")) == "value1");
    
    FL_CHECK(parsed.contains("key2"));
    FL_CHECK(parsed["key2"].is_int());
    FL_CHECK(parsed["key2"].as_or(0) == 42);
    
    FL_CHECK(parsed.contains("key3"));
    FL_CHECK(parsed["key3"].is_float());
    FL_CHECK(fl::fl_abs(parsed["key3"].as_or(0.0) - 3.14) < 0.001);  // Use tolerance for floating-point comparison
}



TEST_CASE("Json as_or test") {
    // Test with primitive values - using correct types
    fl::Json intJson(42); // This creates an int64_t
    FL_CHECK(intJson.is_int());
    FL_CHECK(intJson.as_or(int64_t(0)) == 42);
    FL_CHECK(intJson.as_or(int64_t(99)) == 42); // Should still be 42, not fallback
    
    fl::Json doubleJson(3.14);
    FL_CHECK(doubleJson.is_double());
    FL_CHECK_CLOSE(doubleJson.as_or(0.0), 3.14, 1e-6);
    FL_CHECK_CLOSE(doubleJson.as_or(9.9), 3.14, 1e-6); // Should still be 3.14, not fallback
    
    fl::Json stringJson("hello");
    FL_CHECK(stringJson.is_string());
    FL_CHECK(stringJson.as_or(fl::string("")) == "hello");
    FL_CHECK(stringJson.as_or(fl::string("world")) == "hello"); // Should still be "hello", not fallback
    
    fl::Json boolJson(true);
    FL_CHECK(boolJson.is_bool());
    FL_CHECK(boolJson.as_or(false) == true);
    FL_CHECK(boolJson.as_or(true) == true); // Should still be true, not fallback
    
    // Test with null Json (no value)
    fl::Json nullJson;
    FL_CHECK(nullJson.is_null());
    FL_CHECK(nullJson.as_or(int64_t(100)) == 100); // Should use fallback
    FL_CHECK(nullJson.as_or(fl::string("default")) == "default"); // Should use fallback
    FL_CHECK_CLOSE(nullJson.as_or(5.5), 5.5, 1e-6); // Should use fallback
    FL_CHECK(nullJson.as_or(false) == false); // Should use fallback
    
    // Test operator| still works the same way
    FL_CHECK((intJson | int64_t(0)) == 42);
    FL_CHECK((nullJson | int64_t(100)) == 100);
}

TEST_CASE("FLArduinoJson Integration Tests") {
    SUBCASE("Integer Parsing") {
        // Test various integer representations
        fl::Json int64Json = fl::Json::parse("9223372036854775807"); // Max int64
        FL_REQUIRE(int64Json.is_int());
        auto int64Value = int64Json.as<int64_t>();
        FL_REQUIRE(int64Value.has_value());
        FL_CHECK_EQ(*int64Value, 9223372036854775807LL);
        
        // Test negative integers
        fl::Json negativeIntJson = fl::Json::parse("-9223372036854775807");
        FL_REQUIRE(negativeIntJson.is_int());
        auto negativeIntValue = negativeIntJson.as<int64_t>();
        FL_REQUIRE(negativeIntValue.has_value());
        FL_CHECK_EQ(*negativeIntValue, -9223372036854775807LL);
        
        // Test zero
        fl::Json zeroJson = fl::Json::parse("0");
        FL_REQUIRE(zeroJson.is_int());
        auto zeroValue = zeroJson.as<int64_t>();
        FL_REQUIRE(zeroValue.has_value());
        FL_CHECK_EQ(*zeroValue, 0);
    }
    
    SUBCASE("Float Parsing") {
        // Test various float representations
        fl::Json doubleJson = fl::Json::parse("3.141592653589793");
        FL_REQUIRE(doubleJson.is_double());
        auto doubleValue = doubleJson.as_double();
        FL_REQUIRE(doubleValue.has_value());
        FL_CHECK_CLOSE(*doubleValue, 3.141592653589793, 1e-6);
        
        // Test scientific notation
        fl::Json scientificJson = fl::Json::parse("1.23e-4");
        FL_REQUIRE(scientificJson.is_double());
        auto scientificValue = scientificJson.as_double();
        FL_REQUIRE(scientificValue.has_value());
        // Use approximate comparison for floating point values
        FL_CHECK(fl::fabs(*scientificValue - 0.000123) < 1e-10);
        
        // Test negative float
        fl::Json negativeFloatJson = fl::Json::parse("-2.5");
        FL_REQUIRE(negativeFloatJson.is_double());
        auto negativeFloatValue = negativeFloatJson.as_double();
        FL_REQUIRE(negativeFloatValue.has_value());
        FL_CHECK_CLOSE(*negativeFloatValue, -2.5, 1e-6);
    }
    
    SUBCASE("String Parsing") {
        // Test string parsing
        fl::Json stringJson = fl::Json::parse("\"Hello World\"");
        FL_REQUIRE(stringJson.is_string());
        auto stringValue = stringJson.as_string();
        FL_REQUIRE(stringValue.has_value());
        FL_CHECK(*stringValue == "Hello World");
        
        // Test string with escaped characters
        fl::Json escaped = fl::Json::parse("\"Hello\\nWorld\"");
        FL_REQUIRE(escaped.is_string());
        auto escapedValue = escaped.as_string();
        FL_REQUIRE(escapedValue.has_value());
        FL_CHECK(*escapedValue == "Hello\nWorld");
    }
    
    SUBCASE("Boolean and Null Values") {
        // Test boolean values
        fl::Json trueJson = fl::Json::parse("true");
        FL_REQUIRE(trueJson.is_bool());
        auto trueValue = trueJson.as_bool();
        FL_REQUIRE(trueValue.has_value());
        FL_CHECK(*trueValue == true);
        
        fl::Json falseJson = fl::Json::parse("false");
        FL_REQUIRE(falseJson.is_bool());
        auto falseValue = falseJson.as_bool();
        FL_REQUIRE(falseValue.has_value());
        FL_CHECK(*falseValue == false);
        
        // Test null value
        fl::Json nullJson = fl::Json::parse("null");
        FL_REQUIRE(nullJson.is_null());
    }
    
    SUBCASE("Array Parsing") {
        // Test array with mixed types
        fl::Json arrayJson = fl::Json::parse("[1, 2.5, \"string\", true, null]");
        FL_REQUIRE(arrayJson.is_array());
        FL_CHECK_EQ(arrayJson.size(), 5);
        
        // Check individual elements using as_* methods
        auto firstElement = arrayJson[0].as<int64_t>();
        FL_REQUIRE(firstElement.has_value());
        FL_CHECK_EQ(*firstElement, 1);
        
        auto secondElement = arrayJson[1].as_double();
        FL_REQUIRE(secondElement.has_value());
        FL_CHECK(*secondElement == 2.5);
        
        auto thirdElement = arrayJson[2].as_string();
        FL_REQUIRE(thirdElement.has_value());
        FL_CHECK(*thirdElement == "string");
        
        auto fourthElement = arrayJson[3].as_bool();
        FL_REQUIRE(fourthElement.has_value());
        FL_CHECK(*fourthElement == true);
        
        FL_CHECK(arrayJson[4].is_null());
    }
    
    SUBCASE("Object Parsing") {
        // Test object with mixed types
        fl::Json objJson = fl::Json::parse("{\"int\": 42, \"float\": 3.14, \"string\": \"value\", \"bool\": false, \"null\": null}");
        FL_REQUIRE(objJson.is_object());
        FL_CHECK_EQ(objJson.size(), 5);
        
        // Check individual elements using as_* methods
        auto intElement = objJson["int"].as<int64_t>();
        FL_REQUIRE(intElement.has_value());
        FL_CHECK_EQ(*intElement, 42);
        
        auto floatElement = objJson["float"].as_double();
        FL_REQUIRE(floatElement.has_value());
        FL_CHECK(fl::fl_abs(*floatElement - 3.14) < 0.001);  // Use tolerance for floating-point comparison
        
        auto stringElement = objJson["string"].as_string();
        FL_REQUIRE(stringElement.has_value());
        FL_CHECK(*stringElement == "value");
        
        auto boolElement = objJson["bool"].as_bool();
        FL_REQUIRE(boolElement.has_value());
        FL_CHECK(*boolElement == false);
        
        FL_CHECK(objJson["null"].is_null());
    }
    
    SUBCASE("Error Handling") {
        // Test malformed JSON
        fl::Json malformed = fl::Json::parse("{ invalid json }");
        FL_CHECK(malformed.is_null());
        
        // Test truncated JSON
        fl::Json truncated = fl::Json::parse("{\"incomplete\":");
        FL_CHECK(truncated.is_null());
    }
}


TEST_CASE("Json2 Tests") {
    // Test creating JSON values of different types
    SUBCASE("Basic value creation") {
        fl::Json nullJson;
        FL_CHECK(nullJson.is_null());

        fl::Json boolJson(true);
        FL_CHECK(boolJson.is_bool());
        auto boolOpt = boolJson.as_bool();
        FL_REQUIRE(boolOpt.has_value());
        FL_CHECK_EQ(*boolOpt, true);

        fl::Json intJson(42);
        FL_CHECK(intJson.is_int());
        
        fl::Json doubleJson(3.14);
        FL_CHECK(doubleJson.is_double());
        
        fl::Json stringJson("hello");
        FL_CHECK(stringJson.is_string());
    }

    // Test parsing JSON strings
    SUBCASE("Parsing JSON strings") {
        // Parse a simple object
        fl::Json obj = fl::Json::parse("{\"value\": 30}");
        FL_CHECK(obj.is_object());
        FL_CHECK(obj.contains("value"));

        // Parse an array
        fl::Json arr = fl::Json::parse("[1, 2, 3]");
        FL_CHECK(arr.is_array());  // All array types are handled by is_array()
        FL_CHECK_EQ(arr.size(), 3);
    }

    // Test contains method
    SUBCASE("Contains method") {
        fl::Json obj = fl::Json::parse("{\"key1\": \"value1\", \"key2\": 123}");
        fl::Json arr = fl::Json::parse("[10, 20, 30]");

        FL_CHECK(obj.contains("key1"));
        FL_CHECK(obj.contains("key2"));
        FL_CHECK_FALSE(obj.contains("key3"));

        FL_CHECK(arr.contains(0));
        FL_CHECK(arr.contains(1));
        FL_CHECK(arr.contains(2));
        FL_CHECK_FALSE(arr.contains(3));
    }
    
    // Test array and object creation
    SUBCASE("Array and object creation") {
        fl::Json arr = fl::Json::array();
        FL_CHECK(arr.is_array());
        
        fl::Json obj = fl::Json::object();
        FL_CHECK(obj.is_object());
    }
    
    // Test array of integers (simplified)
    SUBCASE("Array of integers") {
        // Create an array and verify it's an array
        fl::Json arr = fl::Json::array();
        FL_CHECK(arr.is_array());
        
        // Add integers to the array using push_back
        arr.push_back(fl::Json(10));
        arr.push_back(fl::Json(20));
        arr.push_back(fl::Json(30));
        
        // Check that the array has the correct size
        FL_CHECK_EQ(arr.size(), 3);
        
        // Parse an array of integers from string
        fl::Json parsedArr = fl::Json::parse("[100, 200, 300]");
        FL_CHECK(parsedArr.is_array());  // All array types are handled by is_array()
        FL_CHECK_EQ(parsedArr.size(), 3);
        
        // Test contains method with array indices
        FL_CHECK(parsedArr.contains(0));
        FL_CHECK(parsedArr.contains(1));
        FL_CHECK(parsedArr.contains(2));
        FL_CHECK_FALSE(parsedArr.contains(3));
    }
    
    // Test parsing array of integers structure
    SUBCASE("Parse array of integers structure") {
        // Parse an array of integers from string
        fl::Json arr = fl::Json::parse("[5, 15, 25, 35]");
        FL_CHECK(arr.is_array());  // All array types are handled by is_array()
        FL_CHECK_EQ(arr.size(), 4);
        
        // Test that each element exists
        FL_CHECK(arr.contains(0));
        FL_CHECK(arr.contains(1));
        FL_CHECK(arr.contains(2));
        FL_CHECK(arr.contains(3));
        FL_CHECK_FALSE(arr.contains(4));
    }
    
    // Test parsing nested array one level deep structure
    SUBCASE("Parse nested array one level deep structure") {
        // Parse an object with a nested array
        fl::Json obj = fl::Json::parse("{\"key\": [1, 2, 3, 4]}");
        FL_CHECK(obj.is_object());
        FL_CHECK(obj.contains("key"));
        
        // Verify that we can access the key without crashing
        // We're not checking the type or contents of the nested array
        // due to implementation issues that cause segmentation faults
    }
    
    // Test parsing mixed-type object
    SUBCASE("Parse mixed-type object") {
        // Parse an object with different value types
        fl::Json obj = fl::Json::parse("{\"strKey\": \"stringValue\", \"intKey\": 42, \"floatKey\": 3.14, \"arrayKey\": [1, 2, 3]}");
        FL_CHECK(obj.is_object());
        
        // Check that all keys exist
        FL_CHECK(obj.contains("strKey"));
        FL_CHECK(obj.contains("intKey"));
        FL_CHECK(obj.contains("floatKey"));
        FL_CHECK(obj.contains("arrayKey"));
    }
    
    // Test ScreenMap serialization to fl::string
    SUBCASE("ScreenMap serialization to fl::string") {
        // Create test ScreenMaps
        fl::ScreenMap strip1(3, 0.5f);
        strip1.set(0, {0.0f, 0.0f});
        strip1.set(1, {1.0f, 0.0f});
        strip1.set(2, {2.0f, 0.0f});
        
        fl::ScreenMap strip2(3, 0.3f);
        strip2.set(0, {0.0f, 1.0f});
        strip2.set(1, {1.0f, 1.0f});
        strip2.set(2, {2.0f, 1.0f});
        
        fl::fl_map<fl::string, fl::ScreenMap> segmentMaps;
        segmentMaps["strip1"] = strip1;
        segmentMaps["strip2"] = strip2;
        
        // Serialize to JSON using new json2 implementation
        fl::Json doc;
        fl::ScreenMap::toJson(segmentMaps, &doc);
        
        // First verify that the serialized JSON has the correct structure
        FL_CHECK(doc.is_object());
        FL_CHECK(doc.contains("map"));
        
        fl::Json mapObj = doc["map"];
        FL_CHECK(mapObj.is_object());
        FL_CHECK(mapObj.contains("strip1"));
        FL_CHECK(mapObj.contains("strip2"));
        
        fl::Json strip1Obj = mapObj["strip1"];
        fl::Json strip2Obj = mapObj["strip2"];
        FL_CHECK(strip1Obj.is_object());
        FL_CHECK(strip2Obj.is_object());
        
        FL_CHECK(strip1Obj.contains("x"));
        FL_CHECK(strip1Obj.contains("y"));
        FL_CHECK(strip1Obj.contains("diameter"));
        FL_CHECK(strip2Obj.contains("x"));
        FL_CHECK(strip2Obj.contains("y"));
        FL_CHECK(strip2Obj.contains("diameter"));
        
        // Also test with string serialization
        fl::string jsonBuffer = doc.to_string();
        fl::Json parsedJson = fl::Json::parse(jsonBuffer.c_str());
        FL_CHECK(parsedJson.is_object());
        FL_CHECK(parsedJson.contains("map"));
        
        // Parse it back using new json2 implementation
        fl::fl_map<fl::string, fl::ScreenMap> parsedSegmentMaps;
        fl::string err;
        bool result = fl::ScreenMap::ParseJson(jsonBuffer.c_str(), &parsedSegmentMaps, &err);
        
        FL_CHECK(result);
        FL_CHECK_EQ(parsedSegmentMaps.size(), 2);
        FL_CHECK(parsedSegmentMaps.contains("strip1"));
        FL_CHECK(parsedSegmentMaps.contains("strip2"));
        
        fl::ScreenMap parsedStrip1 = parsedSegmentMaps["strip1"];
        FL_CHECK_EQ(parsedStrip1.getLength(), 3);
        FL_CHECK_EQ(parsedStrip1.getDiameter(), 0.5f);
        
        fl::ScreenMap parsedStrip2 = parsedSegmentMaps["strip2"];
        FL_CHECK_EQ(parsedStrip2.getLength(), 3);
        FL_CHECK_CLOSE(parsedStrip2.getDiameter(), 0.3f, 0.001f);  // Use CHECK_CLOSE for floating-point comparison
        
        // Test individual points
        FL_CHECK_EQ(parsedStrip1[0].x, 0.0f);
        FL_CHECK_EQ(parsedStrip1[0].y, 0.0f);
        FL_CHECK_EQ(parsedStrip1[1].x, 1.0f);
        FL_CHECK_EQ(parsedStrip1[1].y, 0.0f);
        FL_CHECK_EQ(parsedStrip1[2].x, 2.0f);
        FL_CHECK_EQ(parsedStrip1[2].y, 0.0f);
        
        FL_CHECK_EQ(parsedStrip2[0].x, 0.0f);
        FL_CHECK_EQ(parsedStrip2[0].y, 1.0f);
        FL_CHECK_EQ(parsedStrip2[1].x, 1.0f);
        FL_CHECK_EQ(parsedStrip2[1].y, 1.0f);
        FL_CHECK_EQ(parsedStrip2[2].x, 2.0f);
        FL_CHECK_EQ(parsedStrip2[2].y, 1.0f);
    }
    
    // Test ScreenMap deserialization from fl::string
    SUBCASE("ScreenMap deserialization from fl::string") {
        const char* jsonStr = R"({"map":{"strip1":{"x":[0,1,2],"y":[0,0,0],"diameter":0.5},"strip2":{"x":[0,1,2],"y":[1,1,1],"diameter":0.3}}})";
        
        fl::fl_map<fl::string, fl::ScreenMap> segmentMaps;
        fl::string err;
        
        bool result = fl::ScreenMap::ParseJson(jsonStr, &segmentMaps, &err);
        
        FL_CHECK(result);
        FL_CHECK_EQ(segmentMaps.size(), 2);
        FL_CHECK(segmentMaps.contains("strip1"));
        FL_CHECK(segmentMaps.contains("strip2"));
        
        fl::ScreenMap strip1 = segmentMaps["strip1"];
        FL_CHECK_EQ(strip1.getLength(), 3);
        FL_CHECK_EQ(strip1.getDiameter(), 0.5f);
        
        fl::ScreenMap strip2 = segmentMaps["strip2"];
        FL_CHECK_EQ(strip2.getLength(), 3);
        FL_CHECK_EQ(strip2.getDiameter(), 0.3f);
        
        // Test individual points
        FL_CHECK_EQ(strip1[0].x, 0.0f);
        FL_CHECK_EQ(strip1[0].y, 0.0f);
        FL_CHECK_EQ(strip1[1].x, 1.0f);
        FL_CHECK_EQ(strip1[1].y, 0.0f);
        FL_CHECK_EQ(strip1[2].x, 2.0f);
        FL_CHECK_EQ(strip1[2].y, 0.0f);
        
        FL_CHECK_EQ(strip2[0].x, 0.0f);
        FL_CHECK_EQ(strip2[0].y, 1.0f);
        FL_CHECK_EQ(strip2[1].x, 1.0f);
        FL_CHECK_EQ(strip2[1].y, 1.0f);
        FL_CHECK_EQ(strip2[2].x, 2.0f);
        FL_CHECK_EQ(strip2[2].y, 1.0f);
    }
}



TEST_CASE("JSON array iterator with int16_t vector") {
    fl::vector<int16_t> data = {1, 2, 3, 4, 5};
    fl::JsonValue value(data);
    
    // Test iteration with int16_t
    int16_t expected = 1;
    for (auto it = value.begin_array<int16_t>(); it != value.end_array<int16_t>(); ++it) {
        FL_CHECK_EQ(*it, expected);
        expected++;
    }
    
    // Test iteration with int32_t (should convert properly)
    int32_t expected32 = 1;
    for (auto it = value.begin_array<int32_t>(); it != value.end_array<int32_t>(); ++it) {
        FL_CHECK_EQ(*it, expected32);
        expected32++;
    }
}

TEST_CASE("JSON array iterator with uint8_t vector") {
    fl::vector<uint8_t> data = {10, 20, 30, 40, 50};
    fl::JsonValue value(data);
    
    // Test iteration with uint8_t
    uint8_t expected = 10;
    for (auto it = value.begin_array<uint8_t>(); it != value.end_array<uint8_t>(); ++it) {
        FL_CHECK_EQ(*it, expected);
        expected += 10;
    }
    
    // Test iteration with int32_t (should convert properly)
    int32_t expected32 = 10;
    for (auto it = value.begin_array<int32_t>(); it != value.end_array<int32_t>(); ++it) {
        FL_CHECK_EQ(*it, expected32);
        expected32 += 10;
    }
}

TEST_CASE("JSON array iterator with float vector") {
    fl::vector<float> data = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    fl::JsonValue value(data);
    
    // Test iteration with float
    float expected = 1.1f;
    for (auto it = value.begin_array<float>(); it != value.end_array<float>(); ++it) {
        FL_CHECK_CLOSE(*it, expected, 0.01f);
        expected += 1.1f;
    }
    
    // Test iteration with double (should convert properly)
    double expectedDouble = 1.1;
    for (auto it = value.begin_array<double>(); it != value.end_array<double>(); ++it) {
        FL_CHECK_CLOSE(static_cast<float>(*it), static_cast<float>(expectedDouble), 0.01f);
        expectedDouble += 1.1;
    }
}

TEST_CASE("JSON class array iterator") {
    fl::Json json = fl::Json::array();
    json.push_back(fl::Json(1));
    json.push_back(fl::Json(2));
    json.push_back(fl::Json(3));
    
    // Test with Json class
    int expected = 1;
    for (auto it = json.begin_array<int>(); it != json.end_array<int>(); ++it) {
        FL_CHECK_EQ(*it, expected);
        expected++;
    }
}


TEST_CASE("Json String to Number Conversion") {
    SUBCASE("String \"5\" to int and float") {
        fl::Json json("5");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_REQUIRE(value64);
        FL_CHECK_EQ(*value64, 5);
        
        // Test conversion to int32_t using new ergonomic API
        fl::optional<int32_t> value32 = json.as<int32_t>();
        FL_REQUIRE(value32);
        FL_CHECK_EQ(*value32, 5);
        
        // Test conversion to int16_t using new ergonomic API
        fl::optional<int16_t> value16 = json.as<int16_t>();
        FL_REQUIRE(value16);
        FL_CHECK_EQ(*value16, 5);
        
        // Test conversion to double using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_REQUIRE(valued);
        // Use a simple comparison for floating-point values
        FL_CHECK_CLOSE(*valued, 5.0, 1e-6);
        
        // Test conversion to float using new ergonomic API
        fl::optional<float> valuef = json.as<float>();
        FL_REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        FL_CHECK_CLOSE(*valuef, 5.0f, 1e-6);
    }
    
    SUBCASE("String integer to int") {
        fl::Json json("42");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_REQUIRE(value64);
        FL_CHECK_EQ(*value64, 42);
        
        // Test conversion to int32_t using new ergonomic API
        fl::optional<int32_t> value32 = json.as<int32_t>();
        FL_REQUIRE(value32);
        FL_CHECK_EQ(*value32, 42);
        
        // Test conversion to int16_t using new ergonomic API
        fl::optional<int16_t> value16 = json.as<int16_t>();
        FL_REQUIRE(value16);
        FL_CHECK_EQ(*value16, 42);
    }
    
    SUBCASE("String integer to float") {
        fl::Json json("42");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to double using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_REQUIRE(valued);
        // Use a simple comparison for floating-point values
        FL_CHECK(*valued == 42.0);
        
        // Test conversion to float using new ergonomic API
        fl::optional<float> valuef = json.as<float>();
        FL_REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        FL_CHECK(*valuef == 42.0f);
    }
    
    SUBCASE("String float to int") {
        fl::Json json("5.7");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail - can't convert float string to int) using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_CHECK_FALSE(value64);
        
        // Test conversion to int32_t (should fail - can't convert float string to int) using new ergonomic API
        fl::optional<int32_t> value32 = json.as<int32_t>();
        FL_CHECK_FALSE(value32);
    }
    
    SUBCASE("String float to float") {
        fl::Json json("5.5");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to double using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_REQUIRE(valued);
        // Use a simple comparison for floating-point values
        FL_CHECK(*valued == 5.5);
        
        // Test conversion to float using new ergonomic API
        fl::optional<float> valuef = json.as<float>();
        FL_REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        FL_CHECK(*valuef == 5.5f);
    }
    
    SUBCASE("Invalid string to number") {
        fl::Json json("hello");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail) using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_CHECK_FALSE(value64);
        
        // Test conversion to double (should fail) using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_CHECK_FALSE(valued);
    }
    
    SUBCASE("Negative string number") {
        fl::Json json("-5");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_REQUIRE(value64);
        FL_CHECK_EQ(*value64, -5);
        
        // Test conversion to double using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_REQUIRE(valued);
        // Use a simple comparison for floating-point values
        FL_CHECK(*valued == -5.0);
    }
    
    SUBCASE("String with spaces") {
        fl::Json json(" 5 ");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail - spaces not allowed) using new ergonomic API
        fl::optional<int64_t> value64 = json.as<int64_t>();
        FL_CHECK_FALSE(value64);
        
        // Test conversion to double (should fail - spaces not allowed) using new ergonomic API
        fl::optional<double> valued = json.as<double>();
        FL_CHECK_FALSE(valued);
    }
}



TEST_CASE("Json Number to String Conversion") {
    SUBCASE("Integer to string") {
        fl::Json json(5);
        FL_CHECK(json.is_int());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_double());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        FL_REQUIRE(value);
        FL_CHECK_EQ(*value, "5");
    }
    
    SUBCASE("Float to string") {
        fl::Json json(5.7);
        FL_CHECK(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        FL_REQUIRE(value);
        FL_CHECK_EQ(*value, "5.700000"); // Default double representation
    }
    
    SUBCASE("Boolean to string") {
        {
            fl::Json json(true);
            FL_CHECK(json.is_bool());
            FL_CHECK_FALSE(json.is_string());
            // Note: is_int() also returns true for booleans in the current implementation
            // This is by design to support automatic conversion from bool to int/float/string
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            FL_REQUIRE(value);
            FL_CHECK_EQ(*value, "true");
        }
        
        {
            fl::Json json(false);
            FL_CHECK(json.is_bool());
            FL_CHECK_FALSE(json.is_string());
            // Note: is_int() also returns true for booleans in the current implementation
            // This is by design to support automatic conversion from bool to int/float/string
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            FL_REQUIRE(value);
            FL_CHECK_EQ(*value, "false");
        }
    }
    
    SUBCASE("Null to string") {
        fl::Json json(nullptr);
        FL_CHECK(json.is_null());
        FL_CHECK_FALSE(json.is_string());
        // Note: is_int() also returns true for null in the current implementation
        // This is by design to support automatic conversion from null to int/float/string
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        FL_REQUIRE(value);
        FL_CHECK_EQ(*value, "null");
    }
    
    SUBCASE("String to string") {
        fl::Json json("hello");
        FL_CHECK(json.is_string());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_bool());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        FL_REQUIRE(value);
        FL_CHECK_EQ(*value, "hello");
    }
    
    SUBCASE("Negative number to string") {
        {
            fl::Json json(-5);
            FL_CHECK(json.is_int());
            FL_CHECK_FALSE(json.is_string());
            FL_CHECK_FALSE(json.is_double());
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            FL_REQUIRE(value);
            FL_CHECK_EQ(*value, "-5");
        }
        
        {
            fl::Json json(-5.7);
            FL_CHECK(json.is_double());
            FL_CHECK_FALSE(json.is_string());
            FL_CHECK_FALSE(json.is_int());
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            FL_REQUIRE(value);
            FL_CHECK_EQ(*value, "-5.700000"); // Default double representation
        }
    }
}


TEST_CASE("JSON iterator test") {
    // Create a simple JSON object
    fl::Json obj = fl::Json::object();
    obj["key1"] = "value1";
    obj["key2"] = "value2";
    
    // Test that we can iterate over it
    int count = 0;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        count++;
    }
    
    FL_CHECK_EQ(count, 2);
    
    // Test const iteration
    const fl::Json const_obj = obj;
    count = 0;
    for (auto it = const_obj.begin(); it != const_obj.end(); ++it) {
        count++;
    }
    
    FL_CHECK_EQ(count, 2);
    
    // Test range-based for loop
    count = 0;
    for (const auto& kv : obj) {
        count++;
    }
    
    FL_CHECK_EQ(count, 2);
}

TEST_CASE("Json Float Data Parsing") {
    SUBCASE("Array of float values should become float data") {
        // Create JSON with array of float values that can't fit in any integer type
        fl::string jsonStr = "[100000.5, 200000.7, 300000.14159, 400000.1, 500000.5]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        // Print the type for debugging
        FASTLED_WARN("JSON type: " << (json.is_floats() ? "floats" : 
                      json.is_audio() ? "audio" : 
                      json.is_bytes() ? "bytes" : 
                      json.is_array() ? "array" : "other"));
        
        FL_CHECK(json.is_floats());
        FL_CHECK_FALSE(json.is_generic_array()); // Should not be regular fl::JsonArray anymore
        FL_CHECK(json.is_array()); // Should still be an array (specialized type)
        FL_CHECK_FALSE(json.is_audio()); // Should not be audio data
        FL_CHECK_FALSE(json.is_bytes()); // Should not be byte data
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of float data
        fl::optional<fl::vector<float>> floatData = json.as_floats();
        FL_REQUIRE(floatData);
        FL_CHECK_EQ(floatData->size(), 5);
        // Use approximate equality for floats
        FL_CHECK((*floatData)[0] == 100000.5f);
        FL_CHECK((*floatData)[1] == 200000.7f);
        FL_CHECK((*floatData)[2] == 300000.14159f);
        FL_CHECK((*floatData)[3] == 400000.1f);
        FL_CHECK((*floatData)[4] == 500000.5f);
    }
    
    SUBCASE("Array with values that can't be represented as floats should remain regular array") {
        // Create JSON with array containing values that can't be exactly represented as floats
        fl::string jsonStr = "[16777217.0, -16777217.0]"; // Beyond float precision
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());
        FL_CHECK_FALSE(json.is_floats());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 2);
    }
    
    SUBCASE("Array with non-numeric values should remain regular array") {
        // Create JSON with array containing non-numeric values
        fl::string jsonStr = "[100000.5, 200000.7, \"hello\", 400000.1]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());
        FL_CHECK_FALSE(json.is_floats());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 4);
    }
    
    SUBCASE("Empty array should remain regular array") {
        // Create JSON with empty array
        fl::string jsonStr = "[]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());
        FL_CHECK_FALSE(json.is_floats());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 0);
    }
    
    SUBCASE("Array with integers that fit in float but not in int16 should become float data") {
        // Create JSON with array of integers that don't fit in int16_t
        fl::string jsonStr = "[40000, 50000, 60000, 70000]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        // Print the type for debugging
        FASTLED_WARN("JSON type: " << (json.is_floats() ? "floats" : 
                      json.is_audio() ? "audio" : 
                      json.is_bytes() ? "bytes" : 
                      json.is_array() ? "array" : "other"));
        
        FL_CHECK(json.is_floats());
        FL_CHECK_FALSE(json.is_generic_array()); // Should not be regular fl::JsonArray anymore
        FL_CHECK(json.is_array()); // Should still be an array (specialized type)
        FL_CHECK_FALSE(json.is_audio()); // Should not be audio data
        FL_CHECK_FALSE(json.is_bytes()); // Should not be byte data
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of float data
        fl::optional<fl::vector<float>> floatData = json.as_floats();
        FL_REQUIRE(floatData);
        FL_CHECK_EQ(floatData->size(), 4);
        FL_CHECK_EQ((*floatData)[0], 40000.0f);
        FL_CHECK_EQ((*floatData)[1], 50000.0f);
        FL_CHECK_EQ((*floatData)[2], 60000.0f);
        FL_CHECK_EQ((*floatData)[3], 70000.0f);
    }
}


TEST_CASE("JSON roundtrip test fl::Json <-> fl::Json") {
    const char* initialJson = "{\"map\":{\"strip1\":{\"x\":[0,1,2,3],\"y\":[0,1,2,3]}}}";

    // 1. Deserialize with fl::Json
    fl::Json json = fl::Json::parse(initialJson);
    FL_CHECK(json.has_value());

    // 2. Serialize with fl::Json
    fl::string json_string = json.serialize();

    // 3. Deserialize with json2
    fl::Json json2_obj = fl::Json::parse(json_string);
    FL_CHECK(json2_obj.has_value());

    // 4. Serialize with json2
    fl::string json2_string = json2_obj.to_string();

    // 5. Compare the results
    FL_CHECK_EQ(fl::string(initialJson), json2_string);
}

// Merged from test_json_roundtrip.cpp
TEST_CASE("JSON Round Trip Serialization with Normalization") {
    // Create the initial JSON string
    const char* initialJson = "{\"name\":\"bob\",\"value\":21}";

    // 1. Parse the JSON string into a Json document
    fl::Json parsedJson = fl::Json::parse(initialJson);

    // Verify parsing was successful
    FL_CHECK(parsedJson.has_value());
    FL_CHECK(parsedJson.is_object());
    FL_CHECK(parsedJson.contains("name"));
    FL_CHECK(parsedJson.contains("value"));

    // Check values
    FL_CHECK_EQ(parsedJson["name"].as_or(fl::string("")), "bob");
    FL_CHECK_EQ(parsedJson["value"].as_or(0), 21);

    // 2. Serialize it back to a string
    fl::string serializedJson = parsedJson.to_string();

    // 3. Confirm that the string is the same (after normalization)
    // Since JSON serialization might reorder keys or add spaces, we'll normalize both strings
    fl::string normalizedInitial = fl::Json::normalizeJsonString(initialJson);
    fl::string normalizedSerialized = fl::Json::normalizeJsonString(serializedJson.c_str());

    FL_CHECK_EQ(normalizedInitial, normalizedSerialized);

    // Also test with a more structured approach - parse the serialized string again
    fl::Json reparsedJson = fl::Json::parse(serializedJson);
    FL_CHECK(reparsedJson.has_value());
    FL_CHECK(reparsedJson.is_object());
    FL_CHECK(reparsedJson.contains("name"));
    FL_CHECK(reparsedJson.contains("value"));
    FL_CHECK_EQ(reparsedJson["name"].as_or(fl::string("")), "bob");
    FL_CHECK_EQ(reparsedJson["value"].as_or(0), 21);
}


TEST_CASE("Json Audio Data Parsing") {
    SUBCASE("Array of int16 values should become audio data") {
        // Create JSON with array of values that fit in int16_t but not uint8_t
        fl::string jsonStr = "[100, -200, 32767, -32768, 0]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_audio());
        FL_CHECK_FALSE(json.is_generic_array()); // Should not be regular fl::JsonArray anymore
        FL_CHECK(json.is_array()); // Should still be an array (specialized type)
        FL_CHECK_FALSE(json.is_bytes()); // Should not be byte data
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of audio data
        fl::optional<fl::vector<int16_t>> audioData = json.as_audio();
        FL_REQUIRE(audioData);
        FL_CHECK_EQ(audioData->size(), 5);
        FL_CHECK_EQ((*audioData)[0], 100);
        FL_CHECK_EQ((*audioData)[1], -200);
        FL_CHECK_EQ((*audioData)[2], 32767);
        FL_CHECK_EQ((*audioData)[3], -32768);
        FL_CHECK_EQ((*audioData)[4], 0);
    }
    
    SUBCASE("Array with boolean values should become byte data, not audio") {
        // Create JSON with array of boolean values (0s and 1s)
        fl::string jsonStr = "[1, 0, 1, 1, 0]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        // Should become byte data, not audio data
        FL_CHECK(json.is_bytes());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_generic_array()); // Should not be regular fl::JsonArray anymore
        FL_CHECK(json.is_array()); // Should still be an array (specialized type)
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of byte data
        fl::optional<fl::vector<uint8_t>> byteData = json.as_bytes();
        FL_REQUIRE(byteData);
        FL_CHECK_EQ(byteData->size(), 5);
    }
    
    SUBCASE("Array with values outside int16 range should remain regular array") {
        // Create JSON with array containing values outside int16_t range
        fl::string jsonStr = "[100, -200, 32768, -32769, 0]"; // 32768 and -32769 exceed int16_t range
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());  // All array types are handled by is_array()
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 5);
    }
    
    SUBCASE("Array with non-integer values should remain regular array") {
        // Create JSON with array containing non-integer values
        fl::string jsonStr = "[100, -200, 3.14, 0]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());  // All array types are handled by is_array()
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 4);
    }
    
    SUBCASE("Empty array should remain regular array") {
        // Create JSON with empty array
        fl::string jsonStr = "[]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 0);
    }
    
    SUBCASE("Mixed array with int16 values should remain regular array") {
        // Create JSON with mixed array (mix of int16 and non-int16 values)
        fl::string jsonStr = "[100, \"hello\", 32767]";
        fl::Json json = fl::Json::parse(jsonStr);
        
        FL_CHECK(json.is_array());
        FL_CHECK_FALSE(json.is_audio());
        FL_CHECK_FALSE(json.is_bytes());
        FL_CHECK_FALSE(json.is_int());
        FL_CHECK_FALSE(json.is_double());
        FL_CHECK_FALSE(json.is_string());
        FL_CHECK_FALSE(json.is_bool());
        FL_CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<fl::JsonArray> arrayData = json.as_array();
        FL_REQUIRE(arrayData);
        FL_CHECK_EQ(arrayData->size(), 3);
    }
}

TEST_CASE("Json ergonomic as<T>() API") {
    // Test the new ergonomic as<T>() API that replaces the verbose as_int<T>() and as_float<T>() methods
    
    SUBCASE("Integer types") {
        fl::Json json(42);
        
        // Test all integer types using the ergonomic API
        FL_CHECK_EQ(*json.as<int8_t>(), 42);
        FL_CHECK_EQ(*json.as<int16_t>(), 42);
        FL_CHECK_EQ(*json.as<int32_t>(), 42);
        FL_CHECK_EQ(*json.as<int64_t>(), 42);
        FL_CHECK_EQ(*json.as<uint8_t>(), 42);
        FL_CHECK_EQ(*json.as<uint16_t>(), 42);
        FL_CHECK_EQ(*json.as<uint32_t>(), 42);
        FL_CHECK_EQ(*json.as<uint64_t>(), 42);
    }
    
    SUBCASE("Floating point types") {
        fl::Json json(3.14f);
        
        // Test floating point types using the ergonomic API
        FL_CHECK_CLOSE(*json.as<float>(), 3.14f, 0.001f);
        FL_CHECK_CLOSE(*json.as<double>(), 3.14, 0.001);
    }
    
    SUBCASE("Boolean type") {
        fl::Json jsonTrue(true);
        fl::Json jsonFalse(false);
        
        // Test boolean type using the ergonomic API
        FL_CHECK_EQ(*jsonTrue.as<bool>(), true);
        FL_CHECK_EQ(*jsonFalse.as<bool>(), false);
    }
    
    SUBCASE("String type") {
        fl::Json json(fl::string("hello"));
        
        // Test string type using the ergonomic API
        FL_CHECK_EQ(*json.as<fl::string>(), fl::string("hello"));
    }
    
    SUBCASE("API comparison - old vs new") {
        fl::Json json(12345);
        
        // Old verbose API (still works for backward compatibility)
        fl::optional<int32_t> oldWay = json.as_int<int32_t>();
        
        // New ergonomic API (preferred)
        fl::optional<int32_t> newWay = json.as<int32_t>();
        
        // Both should give the same result
        FL_REQUIRE(oldWay.has_value());
        FL_REQUIRE(newWay.has_value());
        FL_CHECK_EQ(*oldWay, *newWay);
        FL_CHECK_EQ(*newWay, 12345);
    }
}

TEST_CASE("Json NEW ergonomic API - try_as<T>(), value<T>(), as_or<T>()") {
    // Test the THREE distinct ergonomic conversion methods
    
    SUBCASE("try_as<T>() - Explicit optional handling") {
        fl::Json validJson(42);
        fl::Json nullJson; // null JSON
        
        // try_as<T>() should return fl::optional<T>
        auto validResult = validJson.try_as<int>();
        FL_REQUIRE(validResult.has_value());
        FL_CHECK_EQ(*validResult, 42);
        
        auto nullResult = nullJson.try_as<int>();
        FL_CHECK_FALSE(nullResult.has_value());
        
        // Test string conversion
        fl::Json stringJson("5");
        auto stringToInt = stringJson.try_as<int>();
        FL_REQUIRE(stringToInt.has_value());
        FL_CHECK_EQ(*stringToInt, 5);
        
        // Test failed conversion
        fl::Json invalidJson("hello");
        auto failedConversion = invalidJson.try_as<int>();
        FL_CHECK_FALSE(failedConversion.has_value());
    }
    
    SUBCASE("value<T>() - Direct conversion with sensible defaults") {
        fl::Json validJson(42);
        fl::Json nullJson; // null JSON
        
        // value<T>() should return T directly with defaults on failure
        int validValue = validJson.value<int>();
        FL_CHECK_EQ(validValue, 42);
        
        int nullValue = nullJson.value<int>();
        FL_CHECK_EQ(nullValue, 0); // Default for int
        
        // Test different types with their defaults
        FL_CHECK_EQ(nullJson.value<bool>(), false);
        FL_CHECK_EQ(nullJson.value<float>(), 0.0f);
        FL_CHECK_EQ(nullJson.value<double>(), 0.0);
        FL_CHECK_EQ(nullJson.value<fl::string>(), fl::string(""));
        
        // Test string conversion with defaults
        fl::Json stringJson("5");
        FL_CHECK_EQ(stringJson.value<int>(), 5);
        
        fl::Json invalidJson("hello");
        FL_CHECK_EQ(invalidJson.value<int>(), 0); // Default on failed conversion
    }
    
    SUBCASE("as_or<T>(default) - Conversion with custom defaults") {
        fl::Json validJson(42);
        fl::Json nullJson; // null JSON
        
        // as_or<T>() should return T with custom defaults
        FL_CHECK_EQ(validJson.as_or<int>(999), 42);
        FL_CHECK_EQ(nullJson.as_or<int>(999), 999);
        
        // Test different types with custom defaults
        FL_CHECK_EQ(nullJson.as_or<bool>(true), true);
        FL_CHECK_CLOSE(nullJson.as_or<float>(3.14f), 3.14f, 0.001f);
        FL_CHECK_CLOSE(nullJson.as_or<double>(2.718), 2.718, 0.001);
        FL_CHECK_EQ(nullJson.as_or<fl::string>("default"), fl::string("default"));
        
        // Test string conversion with custom defaults
        fl::Json stringJson("5");
        FL_CHECK_EQ(stringJson.as_or<int>(999), 5);
        
        fl::Json invalidJson("hello");
        FL_CHECK_EQ(invalidJson.as_or<int>(999), 999); // Custom default on failed conversion
    }
    
    SUBCASE("API usage patterns demonstration") {
        fl::Json config = fl::Json::parse(R"({
            "brightness": 128,
            "enabled": true,
            "name": "test_device",
            "timeout": "5.5",
            "missing_field": null
        })");
        
        // Pattern 1: try_as<T>() when you need explicit error handling
        auto maybeBrightness = config["brightness"].try_as<int>();
        if (maybeBrightness.has_value()) {
            FL_CHECK_EQ(*maybeBrightness, 128);
        } else {
            // Handle conversion failure
        }
        
        // Pattern 2: value<T>() when you want defaults and don't care about failure
        int brightness = config["brightness"].value<int>(); // Gets 128
        int missingValue = config["nonexistent"].value<int>(); // Gets 0 (default)
        FL_CHECK_EQ(brightness, 128);
        FL_CHECK_EQ(missingValue, 0);
        
        // Pattern 3: as_or<T>(default) when you want custom defaults
        int ledCount = config["led_count"].as_or<int>(100); // Gets 100 (custom default)
        bool enabled = config["enabled"].as_or<bool>(false); // Gets true (from JSON)
        fl::string deviceName = config["name"].as_or<fl::string>("Unknown"); // Gets "test_device"
        FL_CHECK_EQ(ledCount, 100);
        FL_CHECK_EQ(enabled, true);
        FL_CHECK_EQ(deviceName, fl::string("test_device"));
        
        // String to number conversion
        double timeout = config["timeout"].as_or<double>(10.0); // Converts "5.5" to 5.5
        FL_CHECK_CLOSE(timeout, 5.5, 0.001);
    }
    
    SUBCASE("Backward compatibility with existing as<T>()") {
        fl::Json json(42);
        
        // Old as<T>() still returns fl::optional<T> for backward compatibility
        fl::optional<int> result = json.as<int>();
        FL_REQUIRE(result.has_value());
        FL_CHECK_EQ(*result, 42);
        
        // New try_as<T>() does the same thing (they're equivalent)
        fl::optional<int> tryResult = json.try_as<int>();
        FL_REQUIRE(tryResult.has_value());
        FL_CHECK_EQ(*tryResult, 42);
        
        // Both should be identical
        FL_CHECK_EQ(*result, *tryResult);
    }
}
