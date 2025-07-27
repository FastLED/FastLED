#include "fl/json.h"
#include "fl/json.h"
#include "fl/namespace.h"

#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/ui.h"

namespace fl {

// Definition of the internal class that was previously in button_internal.h
class JsonUiButtonInternal : public JsonUiInternal {
private:
    bool mPressed;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial pressed state.
    JsonUiButtonInternal(const fl::string& name, bool pressed = false)
        : JsonUiInternal(name), mPressed(pressed) {}

    // Override toJson to serialize the button's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "button");
        json.set("group", groupName());
        json.set("id", id());
        json.set("pressed", mPressed);
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::Json& json) override {
        mPressed = json | false;
    }

    // Accessors for the button state.
    bool isPressed() const { return mPressed; }
    void setPressed(bool pressed) { mPressed = pressed; }
};

JsonButtonImpl::JsonButtonImpl(const string &name) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiButtonInternal>(name, false);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
    mUpdater.init(this);
}

JsonButtonImpl::~JsonButtonImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonButtonImpl &JsonButtonImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

bool JsonButtonImpl::clicked() const { return mClickedHappened; }

const string &JsonButtonImpl::name() const { return mInternal->name(); }

void JsonButtonImpl::toJson(fl::Json &json) const {
    mInternal->toJson(json);
}

bool JsonButtonImpl::isPressed() const {
    return mInternal->isPressed();
}

int JsonButtonImpl::clickedCount() const { return mClickedCount; }

const fl::string &JsonButtonImpl::groupName() const { return mInternal->groupName(); }

void JsonButtonImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

void JsonButtonImpl::click() { 
    mInternal->setPressed(true); 
}

void JsonButtonImpl::Updater::init(JsonButtonImpl *owner) {
    mOwner = owner;
    fl::EngineEvents::addListener(this);
}

JsonButtonImpl::Updater::~Updater() { fl::EngineEvents::removeListener(this); }

void JsonButtonImpl::Updater::onPlatformPreLoop2() {
    bool currentPressed = mOwner->mInternal->isPressed();
    mOwner->mClickedHappened =
        currentPressed && (currentPressed != mOwner->mPressedLast);
    mOwner->mPressedLast = currentPressed;
    if (mOwner->mClickedHappened) {
        mOwner->mClickedCount++;
    }
}

int JsonButtonImpl::id() const {
    return mInternal->id();
}

} // namespace fl