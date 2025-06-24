#include "description.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonDescriptionImpl::JsonDescriptionImpl(const string &text): mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonDescriptionImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New("description", update_fcn, to_json_fcn);
    addJsonUiComponent(mInternal);
}

JsonDescriptionImpl::~JsonDescriptionImpl() {}

JsonDescriptionImpl &JsonDescriptionImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDescriptionImpl::text() const { return mText; }

void JsonDescriptionImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = mInternal->name();
    json["type"] = "description";
    json["group"] = mInternal->groupName().c_str();
    json["id"] = mInternal->id();
    json["text"] = text();
}

const string &JsonDescriptionImpl::name() const { return mInternal->name(); }

const fl::string &JsonDescriptionImpl::groupName() const { return mInternal->groupName(); }

void JsonDescriptionImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
