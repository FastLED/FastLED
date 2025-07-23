// Test to identify which types fail with JSON parser's .is<T>() method
#include <doctest.h>
#include "fl/json.h"
#include "fl/warn.h"
#include "fl/str.h"
#include <cstddef>  // for std::nullptr_t

#if FASTLED_ENABLE_JSON

TEST_CASE("JSON is<T>() method - testing all primitive types") {
    SUBCASE("Testing with parsed JSON") {
        const char* jsonStr = R"({
            "null_value": null,
            "bool_true": true,
            "bool_false": false,
            "int_positive": 42,
            "int_negative": -42,
            "int_zero": 0,
            "float_value": 3.14,
            "double_value": 3.14159265359,
            "string_value": "hello world",
            "empty_string": "",
            "array_value": [1, 2, 3],
            "empty_array": [],
            "object_value": {"key": "value"},
            "empty_object": {},
            "large_int": 2147483647,
            "small_int": -2147483648,
            "uint_value": 4294967295,
            "long_value": 9223372036854775807,
            "negative_long": -9223372036854775808
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        bool result = fl::parseJson(jsonStr, &doc, &error);
        REQUIRE(result);
        REQUIRE(error.empty());
        
        // Test null value
        auto null_val = doc["null_value"];
        CHECK(null_val.isNull());
        // Note: nullptr_t test would fail to compile
        
        // Test boolean values
        auto bool_true = doc["bool_true"];
        auto bool_false = doc["bool_false"];
        CHECK(bool_true.is<bool>());
        CHECK(bool_false.is<bool>());
        CHECK(bool_true.as<bool>() == true);
        CHECK(bool_false.as<bool>() == false);
        
        // Test integer types
        auto int_val = doc["int_positive"];
        CHECK(int_val.as<int>() == 42);
        CHECK(int_val.is<int>());
        CHECK(int_val.is<short>());
        CHECK(int_val.is<long>());
        CHECK(int_val.is<long long>());
        CHECK(int_val.is<signed char>());
        // Note: char type is not supported by ArduinoJson
        
        // Test unsigned integer types
        CHECK(int_val.is<unsigned int>());
        CHECK(int_val.is<unsigned short>());
        CHECK(int_val.is<unsigned long>());
        CHECK(int_val.is<unsigned long long>());
        CHECK(int_val.is<unsigned char>());
        
        // Test with actual unsigned value
        auto uint_val = doc["uint_value"];
        CHECK(uint_val.is<unsigned int>());
        CHECK(uint_val.is<unsigned long>());
        
        // Test floating point types
        auto float_val = doc["float_value"];
        auto double_val = doc["double_value"];
        CHECK(float_val.is<float>());
        CHECK(float_val.is<double>());
        CHECK(double_val.is<float>());
        CHECK(double_val.is<double>());
        
        // Test string types
        auto string_val = doc["string_value"];
        CHECK(string_val.is<const char*>());
        // Note: char* is not supported by ArduinoJson, must use const char*
        CHECK(string_val.is<FLArduinoJson::JsonString>());
        
        // Test array types
        auto array_val = doc["array_value"];
        CHECK(array_val.is<FLArduinoJson::JsonArray>());
        CHECK(array_val.is<FLArduinoJson::JsonArrayConst>());
        
        // Test object types
        auto object_val = doc["object_value"];
        CHECK(object_val.is<FLArduinoJson::JsonObject>());
        CHECK(object_val.is<FLArduinoJson::JsonObjectConst>());
        
        // Test edge cases
        auto large_int = doc["large_int"];
        auto small_int = doc["small_int"];
        CHECK(large_int.is<int>());
        CHECK(large_int.is<long>());
        CHECK(small_int.is<int>());
        CHECK(small_int.is<long>());
        
        // Test type mixing (should be false)
        CHECK_FALSE(int_val.is<bool>());
        CHECK_FALSE(int_val.is<const char*>());
        CHECK_FALSE(int_val.is<FLArduinoJson::JsonArray>());
        CHECK_FALSE(string_val.is<int>());
        CHECK_FALSE(string_val.is<bool>());
        CHECK_FALSE(bool_true.is<int>());
        CHECK_FALSE(array_val.is<FLArduinoJson::JsonObject>());
    }
    
    SUBCASE("Testing with directly created values") {
        fl::JsonDocument doc;
        
        // Create various types directly
        doc["direct_int"] = 123;
        doc["direct_uint"] = 4294967295U;
        doc["direct_long"] = 1234567890L;
        doc["direct_float"] = 3.14f;
        doc["direct_double"] = 3.14159;
        doc["direct_bool"] = true;
        doc["direct_string"] = "test string";
        
        CHECK(doc["direct_int"].is<int>());
        CHECK(doc["direct_uint"].is<unsigned int>());
        CHECK(doc["direct_long"].is<long>());
        CHECK(doc["direct_float"].is<float>());
        CHECK(doc["direct_double"].is<double>());
        CHECK(doc["direct_bool"].is<bool>());
        CHECK(doc["direct_string"].is<const char*>());
    }
    
    SUBCASE("Testing problematic types") {
        fl::JsonDocument doc;
        
        // Test various integer sizes
        doc["int8"] = int8_t(127);
        doc["int16"] = int16_t(32767);
        doc["int32"] = int32_t(2147483647);
        doc["int64"] = int64_t(9223372036854775807LL);
        
        doc["uint8"] = uint8_t(255);
        doc["uint16"] = uint16_t(65535);
        doc["uint32"] = uint32_t(4294967295U);
        doc["uint64"] = uint64_t(18446744073709551615ULL);
        
        // Test sized integer types
        CHECK(doc["int8"].is<int8_t>());
        CHECK(doc["int16"].is<int16_t>());
        CHECK(doc["int32"].is<int32_t>());
        CHECK(doc["int64"].is<int64_t>());
        
        CHECK(doc["uint8"].is<uint8_t>());
        CHECK(doc["uint16"].is<uint16_t>());
        CHECK(doc["uint32"].is<uint32_t>());
        CHECK(doc["uint64"].is<uint64_t>());
        
        // Also test if they match the standard types
        CHECK(doc["int8"].is<signed char>());
        CHECK(doc["int16"].is<short>());
        CHECK(doc["int32"].is<int>());
        CHECK(doc["int64"].is<long long>());
        
        CHECK(doc["uint8"].is<unsigned char>());
        CHECK(doc["uint16"].is<unsigned short>());
        CHECK(doc["uint32"].is<unsigned int>());
        CHECK(doc["uint64"].is<unsigned long long>());
    }
    
    SUBCASE("Testing enum types") {
        enum TestEnum { VALUE_A = 1, VALUE_B = 2 };
        
        fl::JsonDocument doc;
        doc["enum_val"] = VALUE_A;
        doc["enum_class_val"] = 1;  // Store as int
        
        // Enums are treated as integers internally
        CHECK(doc["enum_val"].is<TestEnum>());
        CHECK(doc["enum_val"].is<int>());
        CHECK(doc["enum_class_val"].as<int>() == 1);
    }
    
    SUBCASE("Testing specific failure cases") {
        // This subcase documents types that would fail to compile if used with is<T>()
        // We can't actually test these because they cause compilation errors
        
        // Types that fail to compile with is<T>():
        // - char (must use signed char or unsigned char)
        // - char* (must use const char*)
        // - fl::string (custom string types not supported)
        // - std::string (custom string types not supported)
        // - nullptr_t (use isNull() method instead)
        // - User-defined types
        
        // Instead, let's verify the alternatives work
        fl::JsonDocument doc;
        doc["test"] = "hello";
        
        // Verify const char* works instead of char*
        CHECK(doc["test"].is<const char*>());
        
        // Verify signed/unsigned char work instead of char
        doc["byte"] = 65;
        CHECK(doc["byte"].is<signed char>());
        CHECK(doc["byte"].is<unsigned char>());
        
        // Verify isNull() works for null checking
        doc["null"] = nullptr;
        CHECK(doc["null"].isNull());
        CHECK_FALSE(doc["test"].isNull());
    }
    
    SUBCASE("Testing fl::getJsonType() as alternative to is<T>()") {
        // Test the pattern shown in the user's code
        const char* jsonStr = R"({
            "timestamp": 1234567890,
            "array": [1, 2, 3],
            "object": {"key": "value"},
            "string": "hello",
            "float": 3.14,
            "bool": true,
            "null": null
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        bool result = fl::parseJson(jsonStr, &doc, &error);
        REQUIRE(result);
        REQUIRE(error.empty());
        
        // Test getJsonType for various types
        auto timestampVar = doc["timestamp"];
        CHECK(fl::getJsonType(timestampVar) == fl::JSON_INTEGER);
        CHECK(timestampVar.as<uint32_t>() == 1234567890);
        
        auto arrayVar = doc["array"];
        CHECK(fl::getJsonType(arrayVar) == fl::JSON_ARRAY);
        
        auto objectVar = doc["object"];
        CHECK(fl::getJsonType(objectVar) == fl::JSON_OBJECT);
        
        auto stringVar = doc["string"];
        CHECK(fl::getJsonType(stringVar) == fl::JSON_STRING);
        
        auto floatVar = doc["float"];
        CHECK(fl::getJsonType(floatVar) == fl::JSON_FLOAT);
        
        auto boolVar = doc["bool"];
        CHECK(fl::getJsonType(boolVar) == fl::JSON_BOOLEAN);
        
        auto nullVar = doc["null"];
        CHECK(fl::getJsonType(nullVar) == fl::JSON_NULL);
        
        // Test that getJsonType works where is<T>() might have issues
        // For example, distinguishing between integer types
        doc["small_int"] = 42;
        doc["large_int"] = 2147483647;
        doc["uint_val"] = 4294967295U;
        
        CHECK(fl::getJsonType(doc["small_int"]) == fl::JSON_INTEGER);
        CHECK(fl::getJsonType(doc["large_int"]) == fl::JSON_INTEGER);
        CHECK(fl::getJsonType(doc["uint_val"]) == fl::JSON_INTEGER);
        
        // All integer types return JSON_INTEGER regardless of size
        CHECK(doc["small_int"].as<int>() == 42);
        CHECK(doc["large_int"].as<int>() == 2147483647);
        CHECK(doc["uint_val"].as<uint32_t>() == 4294967295U);
    }
}

#else
TEST_CASE("JSON is<T>() method - JSON disabled") {
    FL_WARN("JSON support is disabled");
}
#endif
