#ifdef __EMSCRIPTEN__

#include "../js.h"
#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

jsTitle::jsTitle(const Str& text) : mText(text) {
    jsUiInternal::UpdateFunction update_fcn;
    jsUiInternal::ToJsonFunction to_json_fcn;
    mInternal = jsUiInternalRef::New("title", update_fcn, to_json_fcn);
}

jsTitle::~jsTitle() {
}


void jsTitle::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = mInternal->name();
    json["group"] = mGroup.c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}


FASTLED_NAMESPACE_END

#endif