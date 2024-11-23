#ifdef __EMSCRIPTEN__

#include "../js.h"
#include "ui_internal.h"
#include "ui_manager.h"

using namespace fl;


FASTLED_NAMESPACE_BEGIN

jsDescription::jsDescription(const Str& text) : mText(text) {
    jsUiInternal::UpdateFunction update_fcn;
    jsUiInternal::ToJsonFunction to_json_fcn = jsUiInternal::ToJsonFunction(this, [](void* self, FLArduinoJson::JsonObject& json) {
        static_cast<jsDescription*>(self)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New("description", update_fcn, to_json_fcn);
    jsUiManager::addComponent(mInternal);
}

jsDescription::~jsDescription() {
}


void jsDescription::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = mInternal->name();
    json["type"] = "description";
    json["group"] = mGroup.c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}

FASTLED_NAMESPACE_END

#endif