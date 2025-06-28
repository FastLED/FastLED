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
        CHECK(result);
        CHECK(error.empty());
        
        // Test null value
        FL_WARN("=== Testing null value ===");
        auto null_val = doc["null_value"];
        FL_WARN("null_val.isNull(): " << null_val.isNull());
        
        // Test boolean values
        FL_WARN("\n=== Testing boolean values ===");
        auto bool_true = doc["bool_true"];
        auto bool_false = doc["bool_false"];
        FL_WARN("bool_true.is<bool>(): " << bool_true.is<bool>());
        FL_WARN("bool_false.is<bool>(): " << bool_false.is<bool>());
        
        // Test integer types
        FL_WARN("\n=== Testing integer types ===");
        auto int_val = doc["int_positive"];
        FL_WARN("int_val value: " << int_val.as<int>());
        FL_WARN("int_val.is<int>(): " << int_val.is<int>());
        FL_WARN("int_val.is<short>(): " << int_val.is<short>());
        FL_WARN("int_val.is<long>(): " << int_val.is<long>());
        FL_WARN("int_val.is<long long>(): " << int_val.is<long long>());
        FL_WARN("int_val.is<signed char>(): " << int_val.is<signed char>());
        FL_WARN("int_val.is<char>(): NOT SUPPORTED - ArduinoJson doesn't support char type");
        
        // Test unsigned integer types
        FL_WARN("\n=== Testing unsigned integer types ===");
        FL_WARN("int_val.is<unsigned int>(): " << int_val.is<unsigned int>());
        FL_WARN("int_val.is<unsigned short>(): " << int_val.is<unsigned short>());
        FL_WARN("int_val.is<unsigned long>(): " << int_val.is<unsigned long>());
        FL_WARN("int_val.is<unsigned long long>(): " << int_val.is<unsigned long long>());
        FL_WARN("int_val.is<unsigned char>(): " << int_val.is<unsigned char>());
        
        // Test with actual unsigned value
        auto uint_val = doc["uint_value"];
        FL_WARN("\nuint_val value: " << uint_val.as<unsigned int>());
        FL_WARN("uint_val.is<unsigned int>(): " << uint_val.is<unsigned int>());
        FL_WARN("uint_val.is<unsigned long>(): " << uint_val.is<unsigned long>());
        
        // Test floating point types
        FL_WARN("\n=== Testing floating point types ===");
        auto float_val = doc["float_value"];
        auto double_val = doc["double_value"];
        FL_WARN("float_val.is<float>(): " << float_val.is<float>());
        FL_WARN("float_val.is<double>(): " << float_val.is<double>());
        FL_WARN("double_val.is<float>(): " << double_val.is<float>());
        FL_WARN("double_val.is<double>(): " << double_val.is<double>());
        
        // Test string types
        FL_WARN("\n=== Testing string types ===");
        auto string_val = doc["string_value"];
        FL_WARN("string_val.is<const char*>(): " << string_val.is<const char*>());
        FL_WARN("string_val.is<char*>(): NOT SUPPORTED - ArduinoJson requires const char*");
        FL_WARN("string_val.is<FLArduinoJson::JsonString>(): " << string_val.is<FLArduinoJson::JsonString>());
        
        // Test with fl::string - this will fail to compile
        FL_WARN("string_val.is<fl::string>(): NOT SUPPORTED - custom string types not supported");
        
        // Test array types
        FL_WARN("\n=== Testing array types ===");
        auto array_val = doc["array_value"];
        FL_WARN("array_val.is<FLArduinoJson::JsonArray>(): " << array_val.is<FLArduinoJson::JsonArray>());
        FL_WARN("array_val.is<FLArduinoJson::JsonArrayConst>(): " << array_val.is<FLArduinoJson::JsonArrayConst>());
        
        // Test object types
        FL_WARN("\n=== Testing object types ===");
        auto object_val = doc["object_value"];
        FL_WARN("object_val.is<FLArduinoJson::JsonObject>(): " << object_val.is<FLArduinoJson::JsonObject>());
        FL_WARN("object_val.is<FLArduinoJson::JsonObjectConst>(): " << object_val.is<FLArduinoJson::JsonObjectConst>());
        
        // Test edge cases
        FL_WARN("\n=== Testing edge cases ===");
        auto large_int = doc["large_int"];
        auto small_int = doc["small_int"];
        FL_WARN("large_int.is<int>(): " << large_int.is<int>());
        FL_WARN("large_int.is<long>(): " << large_int.is<long>());
        FL_WARN("small_int.is<int>(): " << small_int.is<int>());
        FL_WARN("small_int.is<long>(): " << small_int.is<long>());
        
        // Test type mixing
        FL_WARN("\n=== Testing type mixing (should be false) ===");
        FL_WARN("int_val.is<bool>(): " << int_val.is<bool>());
        FL_WARN("int_val.is<const char*>(): " << int_val.is<const char*>());
        FL_WARN("int_val.is<FLArduinoJson::JsonArray>(): " << int_val.is<FLArduinoJson::JsonArray>());
        FL_WARN("string_val.is<int>(): " << string_val.is<int>());
        FL_WARN("string_val.is<bool>(): " << string_val.is<bool>());
        FL_WARN("bool_true.is<int>(): " << bool_true.is<int>());
        FL_WARN("array_val.is<FLArduinoJson::JsonObject>(): " << array_val.is<FLArduinoJson::JsonObject>());
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
        doc["direct_cstring"] = "c string";
        
        FL_WARN("\n=== Testing directly created values ===");
        FL_WARN("direct_int.is<int>(): " << doc["direct_int"].is<int>());
        FL_WARN("direct_uint.is<unsigned int>(): " << doc["direct_uint"].is<unsigned int>());
        FL_WARN("direct_long.is<long>(): " << doc["direct_long"].is<long>());
        FL_WARN("direct_float.is<float>(): " << doc["direct_float"].is<float>());
        FL_WARN("direct_double.is<double>(): " << doc["direct_double"].is<double>());
        FL_WARN("direct_bool.is<bool>(): " << doc["direct_bool"].is<bool>());
        FL_WARN("direct_string.is<const char*>(): " << doc["direct_string"].is<const char*>());
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
        
        FL_WARN("\n=== Testing sized integer types ===");
        FL_WARN("int8.is<int8_t>(): " << doc["int8"].is<int8_t>());
        FL_WARN("int16.is<int16_t>(): " << doc["int16"].is<int16_t>());
        FL_WARN("int32.is<int32_t>(): " << doc["int32"].is<int32_t>());
        FL_WARN("int64.is<int64_t>(): " << doc["int64"].is<int64_t>());
        
        FL_WARN("uint8.is<uint8_t>(): " << doc["uint8"].is<uint8_t>());
        FL_WARN("uint16.is<uint16_t>(): " << doc["uint16"].is<uint16_t>());
        FL_WARN("uint32.is<uint32_t>(): " << doc["uint32"].is<uint32_t>());
        FL_WARN("uint64.is<uint64_t>(): " << doc["uint64"].is<uint64_t>());
        
        // Also test if they match the standard types
        FL_WARN("\n=== Testing sized types against standard types ===");
        FL_WARN("int8.is<signed char>(): " << doc["int8"].is<signed char>());
        FL_WARN("int16.is<short>(): " << doc["int16"].is<short>());
        FL_WARN("int32.is<int>(): " << doc["int32"].is<int>());
        FL_WARN("int64.is<long long>(): " << doc["int64"].is<long long>());
        
        FL_WARN("uint8.is<unsigned char>(): " << doc["uint8"].is<unsigned char>());
        FL_WARN("uint16.is<unsigned short>(): " << doc["uint16"].is<unsigned short>());
        FL_WARN("uint32.is<unsigned int>(): " << doc["uint32"].is<unsigned int>());
        FL_WARN("uint64.is<unsigned long long>(): " << doc["uint64"].is<unsigned long long>());
    }
    
    SUBCASE("Testing enum types") {
        enum TestEnum { VALUE_A = 1, VALUE_B = 2 };
        
        fl::JsonDocument doc;
        doc["enum_val"] = VALUE_A;
        doc["enum_class_val"] = 1;  // Store as int
        
        FL_WARN("\n=== Testing enum types ===");
        FL_WARN("enum_val.is<TestEnum>(): " << doc["enum_val"].is<TestEnum>());
        FL_WARN("enum_val.is<int>(): " << doc["enum_val"].is<int>());
        FL_WARN("enum_class_val stored as int: " << doc["enum_class_val"].as<int>());
        FL_WARN("enum types: NOT SUPPORTED - enums must be converted to/from int");
    }
    
    SUBCASE("Summary of findings") {
        FL_WARN("\n=== SUMMARY OF FINDINGS ===");
        FL_WARN("The following types are explicitly supported by ArduinoJson's is<T>():");
        FL_WARN("- Basic types: bool");
        FL_WARN("- Signed integers: int, long, long long, signed char");
        FL_WARN("- Unsigned integers: unsigned int, unsigned long, unsigned long long, unsigned char");
        FL_WARN("- Fixed-size integers: int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t");
        FL_WARN("- Floating point: float, double");
        FL_WARN("- Strings: const char*, JsonString");
        FL_WARN("- Containers: JsonArray, JsonArrayConst, JsonObject, JsonObjectConst");
        FL_WARN("\nTypes that are NOT supported by is<T>():");
        FL_WARN("- char (must use signed char or unsigned char)");
        FL_WARN("- char* (must use const char*)");
        FL_WARN("- Custom string types: fl::string, std::string");
        FL_WARN("- Enum types (must convert to/from integers)");
        FL_WARN("- User-defined types");
        FL_WARN("- nullptr_t (use isNull() method instead)");
    }
}

#else
TEST_CASE("JSON is<T>() method - JSON disabled") {
    FL_WARN("JSON support is disabled");
}
#endif
