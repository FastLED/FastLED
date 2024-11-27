#include "fl/json.h"
#include "fl/str.h"

namespace fl {

bool parseJson(const char* json, fl::JsonDocument* doc, Str* _error) {
    FLArduinoJson::DeserializationError error = deserializeJson(*doc, json);
    if (error) {
        if (_error) {
            *_error = error.c_str();
        }
        return false;
    }
    return true;
}

void toJson(const fl::JsonDocument& doc, Str* jsonBuffer) {
    serializeJson(doc, *jsonBuffer);
}

} // namespace fl