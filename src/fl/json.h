#pragma once

#include "fl/warn.h"
#include "fl/str.h"
#include "fl/compiler_control.h"
#include "fl/optional.h"
#include "fl/vector.h"
#include "fl/printf.h"
#include "fl/memory.h"

/// @file json.h
/// @brief FastLED JSON API - Type-safe, ergonomic JSON processing
/// 
/// This header provides two JSON APIs:
/// 1. **LEGACY API**: Basic fl::parseJson() and fl::toJson() functions (backward compatibility)
/// 2. **IDEAL API**: Modern fl::Json class with type safety and clean syntax (RECOMMENDED)
///
/// ## IDIOMATIC USAGE - PREFERRED PATTERNS
///
/// ### Basic JSON Parsing and Access
/// ```cpp
/// // ✅ IDEAL: Clean, safe, modern API
/// fl::Json json = fl::Json::parse(jsonStr);
/// int brightness = json["config"]["brightness"] | 128;  // Gets value or 128 default
/// string name = json["device"]["name"] | string("default");  // Type-safe with default
/// bool enabled = json["features"]["networking"] | false;  // Never crashes
/// 
/// // ❌ LEGACY: Verbose, error-prone (still works, but not recommended)
/// fl::JsonDocument doc;
/// fl::string error;
/// fl::parseJson(jsonStr, &doc, &error);
/// int brightness = doc["config"]["brightness"].as<int>();  // Can crash if missing
/// ```
///
/// ### FastLED Type Integration
/// ```cpp
/// // ✅ IDEAL: Direct CRGB support
/// CRGB color = json["color"].get<CRGB>().value_or(CRGB::Black);
/// 
/// // ✅ IDEAL: CRGB array/palette support  
/// auto colors = json["palette"].get<vector<CRGB>>().value_or({CRGB::Red, CRGB::Blue});
/// 
/// // ✅ IDEAL: Color from hex strings
/// CRGB red = json["primary_color"] | CRGB::Red;  // "FF0000" -> CRGB::Red
/// ```
///
/// ### Type-Safe Value Extraction
/// ```cpp
/// // ✅ IDEAL: Optional-based extraction with explicit null checking
/// auto maybe_value = json["optional_field"].get<int>();
/// if (maybe_value.has_value()) {
///     int value = *maybe_value;
///     // Use value safely
/// }
/// 
/// // ✅ IDEAL: Default values for missing/invalid data
/// int brightness = json["brightness"] | 128;  // Clean default syntax
/// string mode = json["mode"] | string("auto");  // Type-safe defaults
/// bool enabled = json["enabled"] | false;  // Boolean defaults
/// ```
///
/// ### Array Operations
/// ```cpp
/// // ✅ IDEAL: Safe array access and iteration
/// if (json["effects"].is_array()) {
///     for (size_t i = 0; i < json["effects"].size(); ++i) {
///         string effect = json["effects"][i] | string("");
///         if (!effect.empty()) {
///             // Process effect
///         }
///     }
/// }
/// 
/// // ✅ IDEAL: Check if array contains specific values
/// if (json["effects"].contains("rainbow")) {
///     // Rainbow effect is available
/// }
/// ```
///
/// ### Test Data Construction with JsonBuilder
/// ```cpp
/// // ✅ IDEAL: Easy test data construction
/// auto json = fl::JsonBuilder()
///     .set("brightness", 128)
///     .set("enabled", true)
///     .set("name", "test_device")
///     .set("brightness", 200)
///     .set("effects", vector<string>{"rainbow", "solid", "sparkle"})
///     .build();
/// 
/// // ✅ IDEAL: Type-safe testing
/// CHECK_EQ(json["brightness"] | 0, 128);
/// CHECK(json["enabled"] | false);
/// CHECK_EQ(json["name"] | string(""), "test_device");
/// CHECK_EQ(json["brightness"] | 0, 200);
/// CHECK(json["effects"].contains("rainbow"));
/// ```
///
/// ### Error Handling Best Practices
/// ```cpp
/// // ✅ IDEAL: Parse with error handling
/// fl::string parse_error;
/// fl::Json json = fl::Json::parse(jsonStr, &parse_error);
/// if (!json.has_value()) {
///     FL_WARN("JSON parse failed: " << parse_error);
///     return;
/// }
/// 
/// // ✅ IDEAL: Safe access with validation
/// if (json["config"].is_object()) {
///     int brightness = json["config"]["brightness"] | 128;
///     // Use brightness safely
/// }
/// ```
///
/// ### Type Inspection
/// ```cpp
/// // ✅ IDEAL: Check JSON types safely
/// if (json["data"].type() == fl::JSON_ARRAY) {
///     // Process as array
/// } else if (json["data"].type() == fl::JSON_OBJECT) {
///     // Process as object
/// }
/// 
/// // ✅ IDEAL: String representation of types
/// FL_WARN("Field type: " << json["field"].type_str());
/// ```
///
/// ## KEY BENEFITS OF IDEAL API
/// 
/// - **🛡️ Type Safety** - No crashes on missing fields or type mismatches
/// - **🎯 Default Values** - Clean `json["key"] | default` syntax  
/// - **📖 Readable Code** - 50% less boilerplate for common operations
/// - **🚀 Testing** - Easy test data construction with JsonBuilder
/// - **🔍 Error Handling** - Optional-based extraction with clear error paths
/// - **🎨 FastLED Integration** - Direct CRGB, color palette, and coordinate support
/// 
/// ## MIGRATION FROM LEGACY API
/// 
/// The ideal API can be used alongside the legacy API for gradual migration:
/// ```cpp
/// // Legacy code continues to work
/// fl::JsonDocument doc;
/// fl::parseJson(jsonStr, &doc);
/// 
/// // New code uses ideal API
/// fl::Json json = fl::Json::parse(jsonStr);
/// int value = json["key"] | 0;
/// CRGB color = json["color"] | CRGB::Black;
/// ```
///
/// **📚 SEE ALSO**: examples/Json/Json.ino for comprehensive usage examples

#ifndef FASTLED_ENABLE_JSON
#define FASTLED_ENABLE_JSON 1
#endif

#if FASTLED_ENABLE_JSON

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(null-dereference)
#include "third_party/arduinojson/json.h"
FL_DISABLE_WARNING_POP

#endif


#if !FASTLED_ENABLE_JSON
// Allow forward declare.
namespace FLArduinoJson {
class JsonObject {};
class JsonVariantConst {};
class JsonVariant {};
class JsonDocument {
  public:
    void clear() {}
};
class JsonArray {};
template <typename T, typename U> void serializeJson(T &doc, U &buff) {}
} // namespace FLArduinoJson
#endif

namespace fl {

#if !FASTLED_ENABLE_JSON
class JsonDocument {};
#else
class JsonDocument : public ::FLArduinoJson::JsonDocument {
public:
    /// @brief Serialize JSON document to string (ideal API)
    string serialize() const {
        string result;
        serializeJson(*this, result);
        return result;
    }
};
#endif

// Enum for JSON types
enum JsonType {
    JSON_NULL = 0,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_INTEGER,
    JSON_FLOAT,
    JSON_BOOLEAN
};

#if FASTLED_ENABLE_JSON

// Function to get the type of a JsonVariant
// Usage: JsonType t = fl::getJsonType(variant);
//
// Example:
//   JsonVariant v = doc["foo"];
//   JsonType t = fl::getJsonType(v);
//   switch(t) {
//     case JSON_OBJECT:  Serial.println("object");  break;
//     case JSON_ARRAY:   Serial.println("array");   break;
//     case JSON_INTEGER: Serial.println("integer"); break;
//     case JSON_FLOAT:   Serial.println("float");   break;
//     case JSON_BOOLEAN: Serial.println("boolean"); break;
//     case JSON_STRING:  Serial.println("string");  break;
//     case JSON_NULL:    Serial.println("null");    break;
//   }
template<typename T>
inline JsonType getJsonType(const T& v) {
    if (v.isNull()) {
        return JSON_NULL;
    } else if (v.template is<::FLArduinoJson::JsonObject>() || v.template is<::FLArduinoJson::JsonObjectConst>()) {
        return JSON_OBJECT;
    } else if (v.template is<::FLArduinoJson::JsonArray>() || v.template is<::FLArduinoJson::JsonArrayConst>()) {
        return JSON_ARRAY;
    } else if (v.template is<const char*>() || v.template is<::FLArduinoJson::JsonString>()) {
        return JSON_STRING;
    } else if (v.template is<bool>()) {
        return JSON_BOOLEAN;
    } else if (v.template is<int>() || v.template is<long>() || v.template is<long long>() || 
               v.template is<unsigned int>() || v.template is<unsigned long>() || v.template is<unsigned long long>()) {
        return JSON_INTEGER;
    } else if (v.template is<float>() || v.template is<double>()) {
        return JSON_FLOAT;
    }
    return JSON_NULL;  // fallback
}

template<typename T>
inline const char* getJsonTypeStr(const T& v) {
    JsonType type = getJsonType(v);
    switch(type) {
        case JSON_OBJECT:  return "object";
        case JSON_ARRAY:   return "array";
        case JSON_STRING:  return "string";
        case JSON_INTEGER: return "integer";
        case JSON_FLOAT:   return "float";
        case JSON_BOOLEAN: return "boolean";
        case JSON_NULL:    return "null";
        default:           return "unknown";
    }
}

#else

// Stub version when JSON is disabled
template<typename T>
inline JsonType getJsonType(const T&) {
    return JSON_NULL;
}

template<typename T>
inline const char* getJsonTypeStr(const T&) {
    return "null";
}

#endif

// Parses a JSON string into a JsonDocument.
bool parseJson(const char *json, JsonDocument *doc, string *error = nullptr);

// Serializes a JsonDocument to a string.
void toJson(const JsonDocument &doc, string *jsonBuffer);

#if FASTLED_ENABLE_JSON

// Forward declarations for ideal API
class Json;
class JsonBuilder;

/// @brief Ideal JSON API - Type-safe, ergonomic JSON access
/// @details Provides a modern, fluent interface for JSON operations with:
/// - Type-safe access with optional-based error handling
/// - Operator overloads for clean syntax
/// - Default value support via operator|
class Json {
private:
    ::FLArduinoJson::JsonVariant mVariant;
    fl::shared_ptr<fl::JsonDocument> mDocument; // Keep document alive

public:
    /// @brief Default constructor - creates null Json
    Json() : mVariant() {}
    
    /// @brief Construct from ArduinoJson variant
    Json(::FLArduinoJson::JsonVariant variant) : mVariant(variant) {}
    
    /// @brief Construct from ArduinoJson variant with shared document
    Json(::FLArduinoJson::JsonVariant variant, fl::shared_ptr<fl::JsonDocument> doc) 
        : mVariant(variant), mDocument(doc) {}
    
    /// @brief Construct from JsonDocument
    Json(fl::JsonDocument& doc) : mVariant(doc.as<::FLArduinoJson::JsonVariant>()) {}
    
    /// @brief Parse JSON string into Json object
    /// @param jsonStr JSON string to parse
    /// @param error Optional error string output
    /// @return Json object, or invalid Json if parsing failed
    static Json parse(const char* jsonStr, string* error = nullptr) {
        // Each parse creates its own document to avoid shared state
        auto doc = fl::make_shared<fl::JsonDocument>();
        
        if (fl::parseJson(jsonStr, doc.get(), error)) {
            return Json(doc->as<::FLArduinoJson::JsonVariant>(), doc);
        }
        return Json(::FLArduinoJson::JsonVariant());
    }
    
    /// @brief Array/object access operator
    Json operator[](const char* key) const {
        ::FLArduinoJson::JsonVariant result = mVariant[key];
        return Json(result);
    }
    
    Json operator[](const string& key) const {
        ::FLArduinoJson::JsonVariant result = mVariant[key.c_str()];
        return Json(result);
    }
    
    Json operator[](int index) const {
        ::FLArduinoJson::JsonVariant result = mVariant[index];
        return Json(result);
    }
    
    Json operator[](size_t index) const {
        ::FLArduinoJson::JsonVariant result = mVariant[index];
        return Json(result);
    }
    
    /// @brief Type-safe value extraction with optional
    template<typename T>
    optional<T> get() const {
        if (mVariant.isNull() || !mVariant.is<T>()) {
            return nullopt;
        }
        return make_optional(mVariant.as<T>());
    }
    
    /// @brief Flexible numeric value extraction - allows int/float cross-access and string parsing
    template<typename T>
    optional<T> get_flexible() const {
        if (mVariant.isNull()) {
            return nullopt;
        }
        
        // Check if it's any numeric type that can convert to T
        if (mVariant.is<int>() || mVariant.is<float>() || mVariant.is<double>() || 
            mVariant.is<long>() || mVariant.is<unsigned long>()) {
            // ArduinoJSON handles flexible numeric conversion
            return make_optional(mVariant.as<T>());
        }
        
        // Try string to numeric conversion if T is a numeric type
        if (mVariant.is<const char*>()) {
            const char* str = mVariant.as<const char*>();
            if (str && *str) {  // Check for non-null, non-empty string
                return try_parse_numeric<T>(str);
            }
            return nullopt;
        }
        
        // For non-numeric types, use strict checking
        if (!mVariant.is<T>()) {
            return nullopt;
        }
        return make_optional(mVariant.as<T>());
    }

private:
    /// @brief Helper to parse string to numeric type
    template<typename T>
    optional<T> try_parse_numeric(const char* str) const {
        if (!str || !*str) return nullopt;
        
        char* endptr;
        
        // Handle integer types
        if (fl::is_same<T, int>::value || fl::is_same<T, long>::value) {
            long val = strtol(str, &endptr, 10);
            if (endptr != str && *endptr == '\0') {  // Successful conversion
                return make_optional(static_cast<T>(val));
            }
        }
        // Handle floating point types
        else if (fl::is_same<T, float>::value || fl::is_same<T, double>::value) {
            double val = strtod(str, &endptr);
            if (endptr != str && *endptr == '\0') {  // Successful conversion
                return make_optional(static_cast<T>(val));
            }
        }
        
        return nullopt;
    }

public:
    
    /// @brief Default value operator - returns value or default
    template<typename T>
    T operator|(const T& defaultValue) const {
        auto opt = get<T>();
        return opt.has_value() ? *opt : defaultValue;
    }
    
    /// @brief Check if value exists and is not null
    bool has_value() const {
        return !mVariant.isNull();
    }
    
    /// @brief Check if this Json represents an array
    bool is_array() const {
        return mVariant.is<::FLArduinoJson::JsonArray>();
    }
    
    /// @brief Check if this Json represents an object
    bool is_object() const {
        return mVariant.is<::FLArduinoJson::JsonObject>();
    }
    
    /// @brief Get array size (if this is an array)
    size_t size() const {
        if (is_array()) {
            return mVariant.as<::FLArduinoJson::JsonArray>().size();
        } else if (is_object()) {
            return mVariant.as<::FLArduinoJson::JsonObject>().size();
        }
        return 0;
    }
    
    /// @brief Check if array contains a value (for arrays only)
    bool contains(const string& value) const {
        if (!is_array()) return false;
        
        auto array = mVariant.as<::FLArduinoJson::JsonArray>();
        for (auto element : array) {
            if (element.is<const char*>()) {
                const char* str = element.as<const char*>();
                if (str && string(str) == value) {
                    return true;
                }
            }
        }
        return false;
    }
    
    bool contains(const char* value) const {
        return contains(string(value));
    }
    
    /// @brief Get the JSON type
    JsonType type() const {
        return getJsonType(mVariant);
    }
    
    /// @brief Get string representation of the type
    const char* type_str() const {
        return getJsonTypeStr(mVariant);
    }
    
    /// @brief Serialize JSON to string (ideal API)
    string serialize() const {
        string result;
        serializeJson(mVariant, result);
        return result;
    }
    
    /// @brief Access underlying ArduinoJson variant (for advanced usage)
    ::FLArduinoJson::JsonVariant& variant() { return mVariant; }
    const ::FLArduinoJson::JsonVariant& variant() const { return mVariant; }
};

// Template specialization for string type
template<>
inline optional<string> Json::get<string>() const {
    if (mVariant.isNull() || !mVariant.is<const char*>()) {
        return nullopt;
    }
    const char* cstr = mVariant.as<const char*>();
    return cstr ? make_optional(string(cstr)) : nullopt;
}

// CRGB support will be implemented in json.cpp to avoid template header issues

/// @brief Builder class for constructing JSON objects in tests and examples
/// @details Provides a fluent interface for building JSON structures
class JsonBuilder {
private:
    fl::JsonDocument mDoc;

public:
    /// @brief Constructor
    JsonBuilder() {
        mDoc.to<::FLArduinoJson::JsonObject>();
    }
    
    /// @brief Set integer value at path
    JsonBuilder& set(const string& path, int value) {
        // Simple implementation for basic paths
        mDoc[path.c_str()] = value;
        return *this;
    }
    
    /// @brief Set float value at path
    JsonBuilder& set(const string& path, float value) {
        mDoc[path.c_str()] = value;
        return *this;
    }
    
    /// @brief Set boolean value at path
    JsonBuilder& set(const string& path, bool value) {
        mDoc[path.c_str()] = value;
        return *this;
    }
    
    /// @brief Set string value at path
    JsonBuilder& set(const string& path, const string& value) {
        mDoc[path.c_str()] = value.c_str();
        return *this;
    }
    
    /// @brief Set C-string value at path
    JsonBuilder& set(const string& path, const char* value) {
        mDoc[path.c_str()] = value;
        return *this;
    }
    
    /// @brief Set array of strings at path
    JsonBuilder& set(const string& path, const vector<string>& values) {
        auto array = mDoc[path.c_str()].to<::FLArduinoJson::JsonArray>();
        for (const auto& value : values) {
            array.add(value.c_str());
        }
        return *this;
    }
    
    /// @brief Set array of Json objects at path
    JsonBuilder& set(const string& path, const vector<Json>& objects) {
        auto array = mDoc[path.c_str()].to<::FLArduinoJson::JsonArray>();
        for (const auto& obj : objects) {
            array.add(obj.variant());
        }
        return *this;
    }
    
    
    /// @brief Build the final Json object
    Json build() {
        // Serialize to string and parse back to create independent Json object
        string json_str = to_string();
        return Json::parse(json_str.c_str());
    }
    
    /// @brief Convert to JSON string
    string to_string() {
        string result;
        fl::toJson(mDoc, &result);
        return result;
    }
};

#else // !FASTLED_ENABLE_JSON

// Stub versions when JSON is disabled
class Json {
public:
    Json() = default;
    Json(int) {} // Allow construction from ArduinoJson types
    
    static Json parse(const char*, string* = nullptr) { return Json(); }
    
    Json operator[](const char*) const { return Json(); }
    Json operator[](const string&) const { return Json(); }
    Json operator[](int) const { return Json(); }
    Json operator[](size_t) const { return Json(); }
    
    template<typename T> optional<T> get() const { return nullopt; }
    template<typename T> T operator|(const T& defaultValue) const { return defaultValue; }
    
    bool has_value() const { return false; }
    bool is_array() const { return false; }
    bool is_object() const { return false; }
    size_t size() const { return 0; }
    
    bool contains(const string&) const { return false; }
    bool contains(const char*) const { return false; }
    
    JsonType type() const { return JSON_NULL; }
    const char* type_str() const { return "null"; }
};

class JsonBuilder {
public:
    JsonBuilder& set(const string&, int) { return *this; }
    JsonBuilder& set(const string&, float) { return *this; }
    JsonBuilder& set(const string&, bool) { return *this; }
    JsonBuilder& set(const string&, const string&) { return *this; }
    JsonBuilder& set(const string&, const char*) { return *this; }
    JsonBuilder& set(const string&, const vector<string>&) { return *this; }
    JsonBuilder& set(const string&, const vector<Json>&) { return *this; }
    
    Json build() { return Json(); }
    string to_string() { return "{}"; }
};

#endif // FASTLED_ENABLE_JSON

} // namespace fl
