#include "fl/json.h"
#include "fl/str.h"

namespace fl {

bool parseJson(const char* json, fl::JsonDocument* doc, Str* _error) {
    #if !FASTLED_ENABLE_JSON
    if (_error) {
        *_error = "JSON not enabled";
    }
    return false;
    #else
    FLArduinoJson::DeserializationError error = deserializeJson(*doc, json);
    if (error) {
        if (_error) {
            *_error = error.c_str();
        }
        return false;
    }
    return true;
    #endif
}

void toJson(const fl::JsonDocument& doc, Str* jsonBuffer) {
    #if !FASTLED_ENABLE_JSON
    return;
    #else
    serializeJson(doc, *jsonBuffer);
    #endif
}

} // namespace fl