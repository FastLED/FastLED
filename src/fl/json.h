THIS SHOULD BE A LINTER ERROR#pragma once

#ifndef FASTLED_ENABLE_JSON
#ifdef __AVR__
#define FASTLED_ENABLE_JSON 0
#else
#define FASTLED_ENABLE_JSON 1
#endif
#endif

#if FASTLED_ENABLE_JSON
#include "third_party/arduinojson/json.h"
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

#if !FASTLED_ENABLE_JSON
class JsonDocument {};
#else
class JsonDocument : public ::FLArduinoJson::JsonDocument {};
#endif

// Enum for JSON serialized types
enum SerializedType {
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
// Usage: SerializedType t = fl::jsonType(variant);
//
// Example:
//   JsonVariant v = doc["foo"];
//   SerializedType t = fl::jsonType(v);
//   switch(t) {
//     case JSON_OBJECT:  Serial.println("object");  break;
//     case JSON_ARRAY:   Serial.println("array");   break;
//     case JSON_INTEGER: Serial.println("integer"); break;
//     case JSON_FLOAT:   Serial.println("float");   break;
//     case JSON_BOOLEAN: Serial.println("boolean"); break;
//     case JSON_STRING:  Serial.println("string");  break;
//     default:           Serial.println("null");    break;
//   }
template<typename T>
inline SerializedType jsonType(const T& v) {
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

#else

// Stub version when JSON is disabled
template<typename T>
inline SerializedType jsonType(const T&) {
    return JSON_NULL;
}

#endif

// Parses a JSON string into a JsonDocument.
bool parseJson(const char *json, JsonDocument *doc, string *error = nullptr);

// Serializes a JsonDocument to a string.
void toJson(const JsonDocument &doc, string *jsonBuffer);

} // namespace fl

#if FASTLED_ENABLE_JSON

// Extended JsonVariant types with built-in type() method
// 
// These provide the exact .type() method syntax requested:
//
// Usage Option 1: Use JsonVariantWithType directly
//   JsonVariantWithType v = doc["foo"];  
//   SerializedType t = v.type();
//
// Usage Option 2: Use AUTO_JSON_TYPE macro for existing JsonVariant
//   JsonVariant v = doc["foo"];
//   SerializedType t = AUTO_JSON_TYPE(v).type();
//
namespace FLArduinoJson {

// Helper class to add type() method to JsonVariant
template<typename Base>
class JsonVariantTypeExtension : public Base {
public:
    using Base::Base;  // Inherit constructors
    
    fl::SerializedType type() const {
        return fl::jsonType(*this);
    }
};

// Convenient typedefs
using JsonVariantWithType = JsonVariantTypeExtension<JsonVariant>;
using JsonVariantConstWithType = JsonVariantTypeExtension<JsonVariantConst>;

}

// Macro to provide convenient .type() syntax for existing JsonVariant objects
// Usage: AUTO_JSON_TYPE(variant) gives you a version with .type() method
#define AUTO_JSON_TYPE(v) (::FLArduinoJson::JsonVariantTypeExtension<decltype(v)>(v))

#endif
