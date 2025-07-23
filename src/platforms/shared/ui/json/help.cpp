#include "help.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonHelpImpl::JsonHelpImpl(const string &markdownContent): mMarkdownContent(markdownContent) {
    JsonUiInternal::UpdateFunction update_fcn;
    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this]() -> fl::Json {
            return static_cast<JsonHelpImpl *>(this)->toJson();
        });
    mInternal = fl::make_shared<JsonUiInternal>("help", update_fcn,
                                       fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonHelpImpl::~JsonHelpImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonHelpImpl &JsonHelpImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonHelpImpl::markdownContent() const { return mMarkdownContent; }

fl::Json JsonHelpImpl::toJson() const {
    return fl::JsonBuilder()
        .set("name", mInternal->name())
        .set("type", "help")
        .set("group", mInternal->groupName())
        .set("id", mInternal->id())
        .set("markdownContent", markdownContent())
        .build();
}

const string &JsonHelpImpl::name() const { return mInternal->name(); }

const fl::string &JsonHelpImpl::groupName() const { return mInternal->groupName(); }

void JsonHelpImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif 
