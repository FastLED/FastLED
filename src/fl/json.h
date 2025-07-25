#pragma once

#include "fl/warn.h"
#include "fl/str.h"
#include "fl/compiler_control.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/type_traits.h"

#include "fl/sketch_macros.h"

#ifndef FASTLED_ENABLE_JSON
#ifdef SKETCH_HAS_LOTS_OF_MEMORY
#define FASTLED_ENABLE_JSON 1
#else
#define FASTLED_ENABLE_JSON 0
#endif
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

class string;
class JsonImpl; // Forward declaration for PIMPL

#if !FASTLED_ENABLE_JSON
class JsonDocument {};
#else
class JsonDocument : public ::FLArduinoJson::JsonDocument {};
#endif

// Modern PIMPL-based JSON class for better build performance
class Json {
private:
    fl::shared_ptr<JsonImpl> mImpl;
    
public:
    Json();
    Json(const Json& other) = default;
    Json& operator=(const Json& other) = default;

    // Assignment operators for primitive types
    Json& operator=(int value);
    Json& operator=(float value);
    Json& operator=(bool value);
    Json& operator=(const char* value);
    Json& operator=(const fl::string& value);
    
    // Static factory methods
    static Json parse(const char* jsonStr);
    static Json createArray();
    static Json createObject();
    
    // Basic type checks
    bool has_value() const;
    bool is_object() const;
    bool is_array() const;
    
    // Template is<T>() method for 1:1 FLArduinoJson API compatibility
    template<typename T>
    bool is() const {
        return isTypeMatch<T>();
    }
    
private:
    // Type matching implementations (SFINAE-based for C++11 compatibility)
    template<typename T>
    typename fl::enable_if<fl::is_same<T, int>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, float>::value, bool>::type 
    isTypeMatch() const { return is_float(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, double>::value, bool>::type 
    isTypeMatch() const { return is_float(); }  // Treat double as float
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, bool>::type 
    isTypeMatch() const { return is_bool(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, const char*>::value, bool>::type 
    isTypeMatch() const { return is_string(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, char*>::value, bool>::type 
    isTypeMatch() const { return is_string(); }
    
    // For FLArduinoJson type compatibility (when migrating existing code)
    template<typename T>
    typename fl::enable_if<fl::is_same<T, JsonDocument>::value, bool>::type 
    isTypeMatch() const { return is_object(); }  // Treat JsonDocument as object
    
#if FASTLED_ENABLE_JSON
    // FLArduinoJson type specializations for perfect API compatibility
    template<typename T>
    typename fl::enable_if<fl::is_same<T, ::FLArduinoJson::JsonObject>::value, bool>::type 
    isTypeMatch() const { return is_object(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, ::FLArduinoJson::JsonObjectConst>::value, bool>::type 
    isTypeMatch() const { return is_object(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, ::FLArduinoJson::JsonArray>::value, bool>::type 
    isTypeMatch() const { return is_array(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, ::FLArduinoJson::JsonArrayConst>::value, bool>::type 
    isTypeMatch() const { return is_array(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, ::FLArduinoJson::JsonString>::value, bool>::type 
    isTypeMatch() const { return is_string(); }
#endif
    
    // Additional integer types that FLArduinoJson supports
    template<typename T>
    typename fl::enable_if<fl::is_same<T, long>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, long long>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, unsigned int>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, unsigned long>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, unsigned long long>::value, bool>::type 
    isTypeMatch() const { return is_int(); }
    
    // Fallback for unknown types
    template<typename T>
    typename fl::enable_if<!fl::is_same<T, int>::value && 
                          !fl::is_same<T, float>::value &&
                          !fl::is_same<T, double>::value && 
                          !fl::is_same<T, bool>::value &&
                          !fl::is_same<T, const char*>::value &&
                          !fl::is_same<T, char*>::value &&
                          !fl::is_same<T, JsonDocument>::value, bool>::type 
    isTypeMatch() const { return false; }

public:
    // Keep individual methods for internal use and additional clarity
    bool is_string() const;
    bool is_int() const;
    bool is_float() const;
    bool is_bool() const;
    
    // FLArduinoJson compatibility - as<T>() methods (inline for template compatibility)
    template<typename T>
    T as() const {
        return (*this) | T{};
    }
    
    // Helper methods for type-specific access
    fl::string as_string() const { return getStringValue(); }
    int as_int() const { return getIntValue(); }
    float as_float() const { return getFloatValue(); }
    bool as_bool() const { return getBoolValue(); }
    
    // Access operators
    Json operator[](const char* key) const;
    Json operator[](int index) const;
    
    // Modification methods for creating JSON
    void push_back(const Json& element); // For arrays
    void set(const char* key, const Json& value); // For objects
    void set(const char* key, int value); // For objects
    void set(const char* key, const char* value); // For objects
    void set(const char* key, const fl::string& value); // For objects
    void set(const char* key, float value); // For objects
    void set(const char* key, bool value); // For objects
    
    // Additional array methods for compatibility
    void push_back(int value);
    void push_back(float value);
    void push_back(bool value);
    void push_back(const char* value);
    void push_back(const fl::string& value);
    
    // Array element access by value (for simple addition)
    void add(const Json& element) { push_back(element); }
    void add(int value) { push_back(value); }
    void add(float value) { push_back(value); }
    void add(bool value) { push_back(value); }
    void add(const char* value) { push_back(value); }
    void add(const fl::string& value) { push_back(value); }
    
    // Nested object/array creation (FLArduinoJson compatibility)
    Json createNestedObject(const char* key);
    Json createNestedArray(const char* key);
    Json createNestedObject(); // For arrays
    Json createNestedArray();  // For arrays
    
    // Safe access with defaults (template in header for convenience)
    template<typename T>
    T operator|(const T& defaultValue) const {
        if (!has_value() || isNull()) {
            return defaultValue;
        }
        return getValueOrDefault(defaultValue);
    }

private:
    // Type-specific helper methods for operator|
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, T>::type 
    getValueOrDefault(const T& defaultValue) const {
        return getStringValue();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, int>::value, T>::type 
    getValueOrDefault(const T& defaultValue) const {
        return getIntValue();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, float>::value, T>::type 
    getValueOrDefault(const T& defaultValue) const {
        return getFloatValue();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, T>::type 
    getValueOrDefault(const T& defaultValue) const {
        return getBoolValue();
    }
    
    // Fallback for unsupported types
    template<typename T>
    typename fl::enable_if<!fl::is_same<T, fl::string>::value && 
                          !fl::is_same<T, int>::value && 
                          !fl::is_same<T, float>::value && 
                          !fl::is_same<T, bool>::value, T>::type 
    getValueOrDefault(const T& defaultValue) const {
        return defaultValue;
    }

public:
    
    // Value getters  
    fl::string getStringValue() const;
    int getIntValue() const;
    float getFloatValue() const;
    bool getBoolValue() const;
    bool isNull() const;
    
    // Array/Object size
    size_t getSize() const;
    
    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const;
    
    // Backward compatibility method
    fl::vector<fl::string> getObjectKeys() const { return keys(); }
    
    // Serialization
    fl::string serialize() const;
};

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

#endif

// Parses a JSON string into a JsonDocument.
bool parseJson(const char *json, JsonDocument *doc, string *error = nullptr);

// Serializes a JsonDocument to a string.
void toJson(const JsonDocument &doc, string *jsonBuffer);

} // namespace fl
