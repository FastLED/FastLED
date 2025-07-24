#include "fl/json.h"
#include "fl/str.h"
#include "fl/json_impl.h"

namespace fl {

bool parseJson(const char *json, fl::JsonDocument *doc, string *_error) {
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

void toJson(const fl::JsonDocument &doc, string *jsonBuffer) {
#if FASTLED_ENABLE_JSON
    serializeJson(doc, *jsonBuffer);
#endif
}

Json& Json::operator=(int value) {
    #if FASTLED_ENABLE_JSON
    if (mImpl) {
        mImpl->setValue(value);
    } else {
        FASTLED_WARN("Attempted to assign int to uninitialized fl::Json object.");
    }
    #endif
    return *this;
}

Json& Json::operator=(float value) {
    #if FASTLED_ENABLE_JSON
    if (mImpl) {
        mImpl->setValue(value);
    } else {
        FASTLED_WARN("Attempted to assign float to uninitialized fl::Json object.");
    }
    #endif
    return *this;
}

Json& Json::operator=(bool value) {
    #if FASTLED_ENABLE_JSON
    if (mImpl) {
        mImpl->setValue(value);
    } else {
        FASTLED_WARN("Attempted to assign bool to uninitialized fl::Json object.");
    }
    #endif
    return *this;
}

Json& Json::operator=(const char* value) {
    #if FASTLED_ENABLE_JSON
    if (mImpl) {
        mImpl->setValue(value);
    } else {
        FASTLED_WARN("Attempted to assign const char* to uninitialized fl::Json object.");
    }
    #endif
    return *this;
}

Json& Json::operator=(const fl::string& value) {
    #if FASTLED_ENABLE_JSON
    if (mImpl) {
        mImpl->setValue(value);
    } else {
        FASTLED_WARN("Attempted to assign fl::string to uninitialized fl::Json object.");
    }
    #endif
    return *this;
}

} // namespace fl
