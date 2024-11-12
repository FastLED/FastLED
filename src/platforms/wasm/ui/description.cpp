#ifdef __EMSCRIPTEN__

#include "../js.h"
#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

jsDescription::jsDescription(const char* name) {
    mInternal = jsUiInternalRef(new jsUiInternal("description", name));
}

jsDescription::~jsDescription() {
}

const char* jsDescription::name() const {
    return mInternal->name();
}

void jsDescription::toJson(FLArduinoJson::JsonObject& json) const {
    mInternal->toJson(json);
    if (mGroup.length() > 0) {
        json["group"] = mGroup.c_str();
    }
}

FASTLED_NAMESPACE_END

#endif