#include "description.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonDescriptionImpl::JsonDescriptionImpl(const string &text): mText(text) {
    JsonUiInternal::UpdateFunction update_fcn;
    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this]() -> fl::Json {
            return static_cast<JsonDescriptionImpl *>(this)->toJson();
        });
    mInternal = fl::make_shared<JsonUiInternal>("description", update_fcn, 
                                       fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonDescriptionImpl::~JsonDescriptionImpl() {}

JsonDescriptionImpl &JsonDescriptionImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonDescriptionImpl::text() const { return mText; }

fl::Json JsonDescriptionImpl::toJson() const {
    return fl::JsonBuilder()
        .set("name", mInternal->name())
        .set("type", "description")
        .set("group", mInternal->groupName())
        .set("id", mInternal->id())
        .set("text", text())
        .build();
}

const string &JsonDescriptionImpl::name() const { return mInternal->name(); }

const fl::string &JsonDescriptionImpl::groupName() const { return mInternal->groupName(); }

void JsonDescriptionImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
