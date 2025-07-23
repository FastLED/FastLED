#include "title.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonTitleImpl::JsonTitleImpl(const string &text) : mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this]() -> fl::Json {
            return static_cast<JsonTitleImpl *>(this)->toJson();
        });
    mInternal = fl::make_shared<JsonUiInternal>("title", update_fcn,
                                       fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonTitleImpl::~JsonTitleImpl() {}

JsonTitleImpl &JsonTitleImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

fl::Json JsonTitleImpl::toJson() const {
    return fl::JsonBuilder()
        .set("name", mInternal->name())
        .set("type", "title")
        .set("group", mInternal->groupName())
        .set("id", mInternal->id())
        .set("text", text())
        .build();
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

const fl::string &JsonTitleImpl::groupName() const { return mInternal->groupName(); }

const fl::string &JsonTitleImpl::text() const { return mText; }

void JsonTitleImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
