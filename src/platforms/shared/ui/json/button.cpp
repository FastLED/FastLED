#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonButtonImpl::JsonButtonImpl(const string &name) : mPressed(false) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::Json &json) {
            static_cast<JsonButtonImpl *>(this)->updateInternal(json);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this]() -> fl::Json {
            return static_cast<JsonButtonImpl *>(this)->toJson();
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
    mUpdater.init(this);
}

JsonButtonImpl::~JsonButtonImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonButtonImpl &JsonButtonImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

bool JsonButtonImpl::clicked() const { return mClickedHappened; }

const string &JsonButtonImpl::name() const { return mInternal->name(); }

fl::Json JsonButtonImpl::toJson() const {
    return fl::JsonBuilder()
        .set("name", name())
        .set("group", mInternal->groupName())
        .set("type", "button")
        .set("id", mInternal->id())
        .set("pressed", mPressed)
        .build();
}

bool JsonButtonImpl::isPressed() const {
    return mPressed;
}

int JsonButtonImpl::clickedCount() const { return mClickedCount; }

const fl::string &JsonButtonImpl::groupName() const { return mInternal->groupName(); }

void JsonButtonImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

void JsonButtonImpl::click() { mPressed = true; }

void JsonButtonImpl::Updater::init(JsonButtonImpl *owner) {
    mOwner = owner;
    fl::EngineEvents::addListener(this);
}

JsonButtonImpl::Updater::~Updater() { fl::EngineEvents::removeListener(this); }

void JsonButtonImpl::Updater::onPlatformPreLoop2() {
    mOwner->mClickedHappened =
        mOwner->mPressed && (mOwner->mPressed != mOwner->mPressedLast);
    mOwner->mPressedLast = mOwner->mPressed;
    if (mOwner->mClickedHappened) {
        mOwner->mClickedCount++;
    }
}
void JsonButtonImpl::updateInternal(const fl::Json &json) {
    // Use ideal JSON API directly - no conversion needed!
    bool newPressed = json | false;  // Gets bool value or false default
    mPressed = newPressed;
}

} // namespace fl

#endif // __EMSCRIPTEN__
