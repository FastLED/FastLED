# JSON Parsing with FLArduinoJson

This document outlines the new JSON parsing strategy for `fl::json2::Json` and `fl::json2::Value` classes, leveraging the `FLArduinoJson` library. The existing native parsing functions will be renamed to `parse_native` for backward compatibility and to clearly distinguish them from the new `FLArduinoJson`-based parsing.

## Motivation

The existing native JSON parser in `fl::json2` is a custom implementation. While functional, `FLArduinoJson` (ArduinoJson library) is a robust and widely-used JSON parser that offers better performance, error handling, and compliance with JSON standards. By integrating `FLArduinoJson`, we aim to improve the reliability and efficiency of JSON parsing within the FastLED project.

The `FLArduinoJson` API can be verbose. This new parsing strategy will use `FLArduinoJson` internally to populate the `fl::json2::Json` and `fl::json2::Value` structures, providing a cleaner and more idiomatic API for FastLED users.

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

A new `Value::parse` static method will be implemented. This method will utilize `FLArduinoJson` to parse the input JSON string and then recursively build the `fl::json2::Value` structure.

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

// Helper function to convert FLArduinoJson::JsonVariantConst to fl::json2::Value
fl::shared_ptr<Value> convert_arduinojson_to_fljson2(const FLArduinoJson::JsonVariantConst& src) {
    if (src.isNull()) {
        return fl::make_shared<Value>(nullptr);
    } else if (src.is<bool>()) {
        return fl::make_shared<Value>(src.as<bool>());
    } else if (src.is<int64_t>()) {
        return fl::make_shared<Value>(src.as<int64_t>());
    } else if (src.is<double>()) {
        return fl::make_shared<Value>(src.as<double>());
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
    return fl::make_shared<Value>(nullptr); // Should not happen
}

fl::shared_ptr<Value> Value::parse(const fl::string& txt) {
    // Determine the size of the JsonDocument needed.
    // This is a critical step for ArduinoJson to avoid memory issues.
    // For simplicity, we'll use a DynamicJsonDocument for now, but for production
    // code, a StaticJsonDocument with a pre-calculated size is preferred.
    // A good starting point for DynamicJsonDocument size is 1.5x the input string length,
    // but for complex JSON, it might need more.
    FLArduinoJson::DynamicJsonDocument doc(txt.length() * 2); // Heuristic size

    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());

    if (error) {
        FL_WARN("JSON parsing failed: " << error.c_str());
        return fl::make_shared<Value>(nullptr); // Return null on error
    }

    return convert_arduinojson_to_fljson2(doc.as<FLArduinoJson::JsonVariantConst>());
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

## Considerations and Future Work

*   **Memory Management for `FLArduinoJson`:** The current implementation uses `FLArduinoJson::DynamicJsonDocument` with a heuristic size. For optimal performance and memory usage, especially on embedded systems, it's highly recommended to use `FLArduinoJson::StaticJsonDocument` with a pre-calculated size. This would require a mechanism to estimate the required document size or to allow the user to specify it.
*   **Error Handling:** The current error handling simply logs a warning and returns a null `Value`. More granular error reporting could be implemented if needed.
*   **Unicode Escapes:** The native parser's `parse_string` has a `TODO` for unicode escapes. `FLArduinoJson` handles this automatically, so this is no longer a concern for the new `parse` method.
*   **Performance Benchmarking:** After implementation, it would be beneficial to benchmark the new `FLArduinoJson`-based parser against the `parse_native` functions to quantify the performance improvements.

This new parsing strategy will significantly enhance the JSON capabilities of FastLED by leveraging a robust and well-tested library while maintaining a clean and user-friendly API.
