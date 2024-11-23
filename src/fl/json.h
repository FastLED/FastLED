#pragma once

#include "third_party/arduinojson/json.h"

namespace fl {

class Str;
class JsonDocument: public ::FLArduinoJson::JsonDocument {};

bool parseJson(const char* json, JsonDocument* doc, Str* error = nullptr);
void toJson(const JsonDocument& doc, Str* jsonBuffer);

} // namespace fl