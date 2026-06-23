// IWYU pragma: private

#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/stl/assert.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Definition of the internal class that was previously in button_internal.h
class JsonUiButtonInternal : public JsonUiInternal {
private:
    bool mPressed;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial pressed state.
    JsonUiButtonInternal(const fl::string& name, bool pressed = false)
 FL_NOEXCEPT : JsonUiInternal(name), mPressed(pressed) {}

    // Override toJson to serialize the button's data directly.
    void toJson(fl::json& json) const FL_NOEXCEPT override {
        json.set("name", name());
        json.set("type", "button");
        json.set("group", groupName());
        json.set("id", id());
        json.set("pressed", mPressed);
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::json& json) FL_NOEXCEPT override {
        mPressed = json | false;
    }

    // Accessors for the button state.
    bool isPressed() const FL_NOEXCEPT { return mPressed; }
    void setPressed(bool pressed) FL_NOEXCEPT { mPressed = pressed; }
};

JsonButtonImpl::JsonButtonImpl(const string &name) FL_NOEXCEPT {
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

JsonButtonImpl &JsonButtonImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

bool JsonButtonImpl::clicked() const FL_NOEXCEPT { return mClickedHappened; }

const string &JsonButtonImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

void JsonButtonImpl::toJson(fl::json &json) const FL_NOEXCEPT {
    mInternal->toJson(json);
}

bool JsonButtonImpl::isPressed() const FL_NOEXCEPT {
    if (!mInternal) {
        FL_ASSERT(false, "JsonButtonImpl::isPressed() called before initialization");
        return false;  // Return default value if not initialized
    }
    return mInternal->isPressed();
}

int JsonButtonImpl::clickedCount() const FL_NOEXCEPT { return mClickedCount; }

fl::string JsonButtonImpl::groupName() const FL_NOEXCEPT { return mInternal->groupName(); }

void JsonButtonImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT { mInternal->setGroup(groupName); }

void JsonButtonImpl::click() FL_NOEXCEPT { 
    mInternal->setPressed(true); 
}

void JsonButtonImpl::Updater::init(JsonButtonImpl *owner) FL_NOEXCEPT {
    mOwner = owner;
    fl::EngineEvents::addListener(this);
}

JsonButtonImpl::Updater::~Updater() { fl::EngineEvents::removeListener(this); }

void JsonButtonImpl::Updater::onPlatformPreLoop2() FL_NOEXCEPT {
    bool currentPressed = mOwner->mInternal->isPressed();
    mOwner->mClickedHappened =
        currentPressed && (currentPressed != mOwner->mPressedLast);
    mOwner->mPressedLast = currentPressed;
    if (mOwner->mClickedHappened) {
        mOwner->mClickedCount++;
    }
}

int JsonButtonImpl::id() const FL_NOEXCEPT {
    return mInternal->id();
}

} // namespace fl
