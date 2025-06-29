#include "help.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonHelpImpl::JsonHelpImpl(const string &markdownContent): mMarkdownContent(markdownContent) {
    JsonUiInternal::UpdateFunction update_fcn;
    JsonUiInternal::ToJsonFunction to_json_fcn =
        JsonUiInternal::ToJsonFunction([this](FLArduinoJson::JsonObject &json) {
            static_cast<JsonHelpImpl *>(this)->toJson(json);
        });
    mInternal = JsonUiInternalPtr::New("help", update_fcn, to_json_fcn);
    addJsonUiComponent(mInternal);
}

JsonHelpImpl::~JsonHelpImpl() {}

JsonHelpImpl &JsonHelpImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonHelpImpl::markdownContent() const { return mMarkdownContent; }

void JsonHelpImpl::toJson(FLArduinoJson::JsonObject &json) const {
    json["name"] = mInternal->name();
    json["type"] = "help";
    json["group"] = mInternal->groupName().c_str();
    json["id"] = mInternal->id();
    json["markdownContent"] = markdownContent();
}

const string &JsonHelpImpl::name() const { return mInternal->name(); }

const fl::string &JsonHelpImpl::groupName() const { return mInternal->groupName(); }

void JsonHelpImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif 
