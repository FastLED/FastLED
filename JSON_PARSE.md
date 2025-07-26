# JSON Parsing with FLArduinoJson

This document outlines the new JSON parsing strategy for `fl::Json` and `fl::Json::Value` classes, leveraging the `FLArduinoJson` library. The existing native parsing functions will be renamed to `parse_native` for backward compatibility and to clearly distinguish them from the new `FLArduinoJson`-based parsing.

## Motivation

The existing native JSON parser in `fl::Json` is a custom implementation. While functional, `FLArduinoJson` (ArduinoJson library) is a robust and widely-used JSON parser that offers better performance, error handling, and compliance with JSON standards. By integrating `FLArduinoJson`, we aim to improve the reliability and efficiency of JSON parsing within the FastLED project.

The `FLArduinoJson` API can be verbose. This new parsing strategy will use `FLArduinoJson` internally to populate the `fl::Json` and `fl::Json::Value` structures, providing a cleaner and more idiomatic API for FastLED users.

## Proposed Changes

### 1. Rename Native Parsing Functions

The existing `parse_value` and `Value::parse` functions in `src/fl/json2.cpp` will be renamed to `parse_value_native` and `Value::parse_native` respectively. This ensures that any existing code relying on the native parser can continue to function by calling the `_native` suffixed versions.

**`src/fl/json2.cpp` (before - snippet):**

```cpp
// ... existing parse_value and related helper functions ...

fl::shared_ptr<Value> Value::parse(const fl::string& txt) {
    size_t pos = 0;
    return parse_value(txt, pos);
}
```

**`src/fl/json2.cpp` (after - snippet):**

```cpp
// ... existing parse_value and related helper functions (renamed to parse_value_native, parse_string_native, etc.) ...

fl::shared_ptr<Value> Value::parse_native(const fl::string& txt) {
    size_t pos = 0;
    return parse_value_native(txt, pos);
}
```

### 2. Implement New `Value::parse` using `FLArduinoJson`

A new `Value::parse` static method will be implemented. This method will utilize `FLArduinoJson` to parse the input JSON string and then recursively build the `fl::Json::Value` structure.

**`src/fl/json2.h` (add to `Value` class):**

```cpp
// ... inside class Value { ... }

    // New parsing factory using FLArduinoJson
    static fl::shared_ptr<Value> parse(const fl::string &txt);
```

**`src/fl/json2.cpp` (implementation of `Value::parse`):**

```cpp
#include "fl/json2.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "src/third_party/arduinojson/json.h" // Include FLArduinoJson

namespace fl {
namespace json2 {

// ... existing native parsing functions (renamed to parse_value_native, etc.) ...

// NOTE - YOU MUST USE EXHAUSTIVE SEARCHES FOR THE INTEGER TYPES, FLOATS AND DOUBLES.
// This is particularly important for Arduino platforms where different numeric types
// have specific memory and performance characteristics. When implementing the conversion
// from FLArduinoJson::JsonVariantConst to fl::Json::Value, we must explicitly check
// for all possible numeric types to ensure correct type handling on constrained platforms.

// Helper function to convert FLArduinoJson::JsonVariantConst to fl::Json::Value
// (Implementation notes - to be completed during actual implementation)
fl::shared_ptr<Value> convert_arduinojson_to_fljson2(const FLArduinoJson::JsonVariantConst& src) {
    // Implementation will need to handle:
    // 1. All integer types (int8_t, int16_t, int32_t, int64_t, uint32_t, etc.)
    // 2. Floating point types (float, double)
    // 3. Other JSON types (null, bool, string, array, object)
    // 4. Proper type conversion between FLArduinoJson and fl::Json types
    // 
    // Example structure (to be implemented):
    /*
    if (src.isNull()) {
        return fl::make_shared<Value>(nullptr);
    } else if (src.is<bool>()) {
        return fl::make_shared<Value>(src.as<bool>());
    } else if (src.is<int64_t>()) {
        // Handle 64-bit integers
        return fl::make_shared<Value>(src.as<int64_t>());
    } else if (src.is<int32_t>()) {
        // Handle 32-bit integers explicitly for platform compatibility
        return fl::make_shared<Value>(static_cast<int64_t>(src.as<int32_t>()));
    } else if (src.is<double>()) {
        // Handle double precision floats
        return fl::make_shared<Value>(src.as<double>());
    } else if (src.is<float>()) {
        // Handle single precision floats explicitly
        return fl::make_shared<Value>(static_cast<double>(src.as<float>()));
    } else if (src.is<const char*>()) {
        return fl::make_shared<Value>(fl::string(src.as<const char*>()));
    } else if (src.is<FLArduinoJson::JsonArrayConst>()) {
        Array arr;
        for (const auto& item : src.as<FLArduinoJson::JsonArrayConst>()) {
            arr.push_back(convert_arduinojson_to_fljson2(item));
        }
        return fl::make_shared<Value>(fl::move(arr));
    } else if (src.is<FLArduinoJson::JsonObjectConst>()) {
        Object obj;
        for (const auto& kv : src.as<FLArduinoJson::JsonObjectConst>()) {
            obj[fl::string(kv.key().c_str())] = convert_arduinojson_to_fljson2(kv.value());
        }
        return fl::make_shared<Value>(fl::move(obj));
    }
    */
    return fl::make_shared<Value>(nullptr); // Should not happen
}

fl::shared_ptr<Value> Value::parse(const fl::string& txt) {
    // Implementation notes:
    // 1. Determine the size of the JsonDocument needed.
    // 2. For embedded systems, consider using StaticJsonDocument with calculated size.
    // 3. Handle deserialization errors appropriately.
    // 4. Convert the parsed result to fl::Json::Value structure.
    // 
    // Example structure (to be implemented):
    /*
    // Determine the size of the JsonDocument needed.
    FLArduinoJson::DynamicJsonDocument doc(txt.length() * 2); // Heuristic size

    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());

    if (error) {
        FL_WARN("JSON parsing failed: " << error.c_str());
        return fl::make_shared<Value>(nullptr); // Return null on error
    }

    return convert_arduinojson_to_fljson2(doc.as<FLArduinoJson::JsonVariantConst>());
    */
    return fl::shared_ptr<Value>(); // Placeholder
}

// ... rest of the file ...

} // namespace json2
} // namespace fl
```

### 3. Update `Json::parse` to use `Value::parse`

The `Json::parse` static method will be updated to simply delegate to the new `Value::parse` method.

**`src/fl/json2.h` (no change needed, already delegates):**

```cpp
// ... inside class Json { ... }

    // Parsing factory method
    static Json parse(const fl::string &txt) {
        auto parsed = Value::parse(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
    }
```

## Testing Strategy

A comprehensive testing strategy is essential to ensure the new FLArduinoJson-based parser works correctly and maintains backward compatibility with the existing native parser.

### 1. Unit Tests for Basic Types

Create tests that cover all basic JSON types:
- Null values
- Boolean values (true/false)
- Integer types (positive, negative, zero, various sizes)
- Floating-point types (positive, negative, zero, scientific notation)
- String values (empty, with escape sequences, Unicode)
- Arrays (empty, with various element types)
- Objects (empty, with various value types)

### 2. Type Exhaustiveness Tests

Create specific tests to verify that all numeric types are correctly handled:
- Test with various integer sizes (int8_t, int16_t, int32_t, int64_t, uint32_t, etc.)
- Test with different floating-point representations (float, double)
- Verify type preservation during parsing and conversion
- Test edge cases for each numeric type (maximum/minimum values)

### 3. Backward Compatibility Tests

Ensure that the new parser produces results that are structurally and semantically equivalent to the native parser:
- Parse identical JSON strings with both parsers
- Compare the resulting structures
- Verify that all accessor methods work the same way
- Test round-trip serialization/deserialization

### 4. Error Handling Tests

Test various error conditions:
- Malformed JSON strings
- Invalid escape sequences
- Truncated input
- Memory exhaustion scenarios
- Unicode handling edge cases
- Type conversion errors

### 5. Performance Tests

Benchmark the new parser against the native implementation:
- Parsing time for various JSON sizes
- Memory usage during parsing
- Comparison with existing test cases

### 6. Integration Tests

Test the parser in real-world scenarios:
- Parsing configuration files
- Handling API responses
- Round-trip serialization/deserialization
- Edge cases from existing FastLED code

### Sample Test Implementation Plan

The following test structure should be implemented when the actual parser is developed:

```cpp
// In tests/test_json2_arduinojson.cpp
#include "test.h"
#include "fl/json2.h"

TEST_CASE("FLArduinoJson Integration Tests") {
    SUBCASE("Integer Type Exhaustiveness") {
        // Test various integer representations
        // fl::Json int64Json = fl::Json::parse("9223372036854775807"); // Max int64
        // REQUIRE(int64Json.is_int());
        // CHECK_EQ(int64Json | int64_t(0), 9223372036854775807LL);
        // 
        // Additional tests for other integer types...
    }
    
    SUBCASE("Float Type Exhaustiveness") {
        // Test various float representations
        // fl::Json doubleJson = fl::Json::parse("3.141592653589793");
        // REQUIRE(doubleJson.is_double());
        // CHECK_EQ(doubleJson | 0.0, 3.141592653589793);
        //
        // Additional tests for other float types...
    }
    
    SUBCASE("Backward Compatibility") {
        // const char* testJson = "{\"name\":\"FastLED\",\"version\":4,\"features\":[\"colors\",\"effects\"]}";
        // 
        // Parse with new FLArduinoJson-based parser
        // fl::Json newParserResult = fl::Json::parse(testJson);
        // 
        // Parse with native parser (for comparison)
        // fl::Json nativeParserResult = fl::Json::parse_native(testJson);
        // 
        // Verify they produce equivalent results
        // CHECK(newParserResult.to_string() == nativeParserResult.to_string());
    }
    
    SUBCASE("Error Handling") {
        // Test malformed JSON
        // fl::Json malformed = fl::Json::parse("{ invalid json }");
        // CHECK(malformed.is_null());
        // 
        // Test truncated JSON
        // fl::Json truncated = fl::Json::parse("{\"incomplete\":");
        // CHECK(truncated.is_null());
    }
}
```

## Considerations and Future Work

*   **Memory Management for `FLArduinoJson`:** The implementation should use `FLArduinoJson::StaticJsonDocument` with a pre-calculated size when possible, especially for embedded systems. This requires a mechanism to estimate the required document size or to allow the user to specify it.
*   **Error Handling:** The implementation should provide more granular error reporting than simply returning a null `Value`.
*   **Unicode Support:** `FLArduinoJson` handles Unicode escapes automatically, which improves upon the native parser's current limitations.
*   **Performance Benchmarking:** After implementation, benchmark the new `FLArduinoJson`-based parser against the `parse_native` functions to quantify the performance improvements.

This new parsing strategy will significantly enhance the JSON capabilities of FastLED by leveraging a robust and well-tested library while maintaining a clean and user-friendly API.
