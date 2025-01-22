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

namespace fl {

class Str;

#if !FASTLED_ENABLE_JSON
class JsonDocument {};
#else
class JsonDocument: public ::FLArduinoJson::JsonDocument {};
#endif

bool parseJson(const char* json, JsonDocument* doc, Str* error = nullptr);
void toJson(const JsonDocument& doc, Str* jsonBuffer);

} // namespace fl