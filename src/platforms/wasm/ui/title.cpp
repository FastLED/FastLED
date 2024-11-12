#ifdef __EMSCRIPTEN__

#include "../js.h"
#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

jsTitle::jsTitle(const char* name) {
    mInternal = jsUiInternalRef::New("title", name);
}

jsTitle::~jsTitle() {
}

const char* jsTitle::name() const {
    return mInternal->name();
}

void jsTitle::toJson(FLArduinoJson::JsonObject& json) const {
    mInternal->toJson(json);
    if (mGroup.length() > 0) {
        json["group"] = mGroup.c_str();
    }
}

FASTLED_NAMESPACE_END

#endif