#include "description.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json2.h"
#include "fl/json2.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonDescriptionImpl::JsonDescriptionImpl(const string &text): mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>("description", update_fcn, to_json_fcn);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl::~JsonDescriptionImpl() {}

JsonDescriptionImpl &JsonDescriptionImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDescriptionImpl::text() const { return mText; }

void JsonDescriptionImpl::toJson(fl::json2::Json &json) const {
    json.set("name", mInternal->name());
    json.set("type", "description");
    json.set("group", mInternal->groupName());
    json.set("id", mInternal->id());
    json.set("text", text());
}

const string &JsonDescriptionImpl::name() const { return mInternal->name(); }

const fl::string &JsonDescriptionImpl::groupName() const { return mInternal->groupName(); }

void JsonDescriptionImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
