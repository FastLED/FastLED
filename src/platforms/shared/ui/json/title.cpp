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

JsonTitleImpl &JsonTitleImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

void JsonTitleImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = mInternal->name();
    json["type"] = "title";
    json["group"] = mInternal->groupName().c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

const fl::string &JsonTitleImpl::groupName() const { return mInternal->groupName(); }

const fl::string &JsonTitleImpl::text() const { return mText; }

void JsonTitleImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
