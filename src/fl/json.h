#pragma once

#include "fl/warn.h"
#include "fl/str.h"
#include "fl/compiler_control.h"

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

class string;

#if !FASTLED_ENABLE_JSON
class JsonDocument {};
#else
class JsonDocument : public ::FLArduinoJson::JsonDocument {};
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

#endif

// Parses a JSON string into a JsonDocument.
bool parseJson(const char *json, JsonDocument *doc, string *error = nullptr);

// Serializes a JsonDocument to a string.
void toJson(const JsonDocument &doc, string *jsonBuffer);

} // namespace fl
