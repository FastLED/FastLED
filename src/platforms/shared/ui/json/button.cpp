#include "fl/json.h"
#include "fl/json2.h"
#include "fl/namespace.h"

#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON



namespace fl {

JsonButtonImpl::JsonButtonImpl(const string &name) : mPressed(false) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::json2::Json &value) {
            this->updateInternal(value);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this](fl::json2::Json &json) {
            this->toJson(json);
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

void JsonButtonImpl::toJson(fl::json2::Json &json) const {
    json.set("name", name());
    json.set("group", mInternal->groupName());
    json.set("type", "button");
    json.set("id", mInternal->id());
    json.set("pressed", mPressed);
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

void JsonButtonImpl::updateInternal(
    const fl::json2::Json &value) {
    mPressed = value | false;
}

} // namespace fl

#endif // __EMSCRIPTEN__
