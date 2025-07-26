#include "help.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"
#include "fl/json.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonHelpImpl::JsonHelpImpl(const string &markdownContent): mMarkdownContent(markdownContent) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
        });
    mInternal = fl::make_shared<JsonUiInternal>("help", update_fcn, to_json_fcn);
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl::~JsonHelpImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonHelpImpl &JsonHelpImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonHelpImpl::markdownContent() const { return mMarkdownContent; }

void JsonHelpImpl::toJson(fl::json2::Json &json) const {
    json.set("name", mInternal->name());
    json.set("type", "help");
    json.set("group", mInternal->groupName());
    json.set("id", mInternal->id());
    json.set("markdownContent", markdownContent());
}

const string &JsonHelpImpl::name() const { return mInternal->name(); }

const fl::string &JsonHelpImpl::groupName() const { return mInternal->groupName(); }

void JsonHelpImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
