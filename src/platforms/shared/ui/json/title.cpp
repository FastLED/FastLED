#include "title.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"
#include "fl/json2.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonTitleImpl::JsonTitleImpl(const string &text) : mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>("title", update_fcn, to_json_fcn);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonTitleImpl::~JsonTitleImpl() {}

JsonTitleImpl &JsonTitleImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

void JsonTitleImpl::toJson(fl::json2::Json &json) const {
    json.set("name", mInternal->name());
    json.set("type", "title");
    json.set("group", mInternal->groupName());
    json.set("id", mInternal->id());
    json.set("text", text());
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

const fl::string &JsonTitleImpl::groupName() const { return mInternal->groupName(); }

const fl::string &JsonTitleImpl::text() const { return mText; }

void JsonTitleImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif