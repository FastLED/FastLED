

#include "title.h"
#include "ui_internal.h"
#include "platforms/wasm/ui/ui_deps.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

jsTitleImpl::jsTitleImpl(const Str &text) : mText(text) {
    jsUiInternal::UpdateFunction update_fcn;
    jsUiInternal::ToJsonFunction to_json_fcn =
        jsUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<jsTitleImpl *>(this)->toJson(json);
        });
    mInternal = jsUiInternalPtr::New("title", update_fcn, to_json_fcn);
    addUiComponent(mInternal);
}

jsTitleImpl::~jsTitleImpl() {}

void jsTitleImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = mInternal->name();
    json["type"] = "title";
    json["group"] = mInternal->groupName().c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}



} // namespace fl

#endif
