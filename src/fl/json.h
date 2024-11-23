#pragma once

#include "third_party/arduinojson/json.h"

namespace fl {

class Str;

bool parseJson(const char* json, FLArduinoJson::JsonDocument* doc, Str* error = nullptr);
void toJson(const FLArduinoJson::JsonDocument& doc, Str* jsonBuffer);

} // namespace fl