#include "title.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonTitleImpl::JsonTitleImpl(const string &text) : mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonTitleImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New("title", update_fcn, to_json_fcn);
    addJsonUiComponent(mInternal);
}

JsonTitleImpl::~JsonTitleImpl() {}

void JsonTitleImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = mInternal->name();
    json["type"] = "title";
    json["group"] = mInternal->groupName().c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

} // namespace fl

#endif
