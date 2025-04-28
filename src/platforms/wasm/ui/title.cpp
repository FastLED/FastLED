#ifdef __EMSCRIPTEN__

#include "../js.h"
#include "ui_internal.h"
#include "ui_manager.h"
#include "title.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN

jsTitleImpl::jsTitleImpl(const Str& text) : mText(text) {
    jsUiInternal::UpdateFunction update_fcn;
    jsUiInternal::ToJsonFunction to_json_fcn = jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject& json) {
        static_cast<jsTitleImpl*>(this)->toJson(json);
    });
    mInternal = jsUiInternalPtr::New("title", update_fcn, to_json_fcn);
    jsUiManager::addComponent(mInternal);
}

jsTitleImpl::~jsTitleImpl() {
}


void jsTitleImpl::toJson(FLArduinoJson::JsonObject& json) const {
    json["name"] = mInternal->name();
    json["type"] = "title";
    json["group"] = mGroup.c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}


FASTLED_NAMESPACE_END

#endif