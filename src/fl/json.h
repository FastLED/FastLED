#pragma once

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

// Parses a JSON string into a JsonDocument.
bool parseJson(const char *json, JsonDocument *doc, string *error = nullptr);

// Serializes a JsonDocument to a string.
void toJson(const JsonDocument &doc, string *jsonBuffer);

} // namespace fl
