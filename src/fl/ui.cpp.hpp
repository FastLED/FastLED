#include "fl/ui.h"
#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

// UIElement constructor
UIElement::UIElement() {}

// UISlider constructor
UISlider::UISlider(const char *name, float value, float min, float max, float step)
    : mImpl(name, value, min, max, step), mListener(this) {
    mListener.addToEngineEventsOnce();
}

// UIButton constructor
UIButton::UIButton(const char *name) : mImpl(name), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIButton::~UIButton() {}

// UICheckbox constructor
UICheckbox::UICheckbox(const char *name, bool value)
    : mImpl(name, value), mLastFrameValue(false), mLastFrameValueValid(false), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UICheckbox::~UICheckbox() {}

// UINumberField constructor
UINumberField::UINumberField(const char *name, double value, double min, double max)
    : mImpl(name, value, min, max), mListener(this), mLastFrameValue(0), mLastFrameValueValid(false) {
    mListener.addToEngineEventsOnce();
}

UINumberField::~UINumberField() {}

// UITitle constructors
#if FASTLED_USE_JSON_UI
UITitle::UITitle(const char *name) : mImpl(fl::string(name), fl::string(name)) {}
#else
UITitle::UITitle(const char *name) : mImpl(name) {}
#endif

UITitle::~UITitle() {}

// UIDescription constructor
UIDescription::UIDescription(const char *name) : mImpl(name) {}
UIDescription::~UIDescription() {}

// UIHelp constructor
UIHelp::UIHelp(const char *markdownContent) : mImpl(markdownContent) {}
UIHelp::~UIHelp() {}

// UIAudio constructors
UIAudio::UIAudio(const char *name) : mImpl(name) {}
UIAudio::UIAudio(const char *name, const fl::AudioConfig& config) : mImpl(name, config) {}
UIAudio::~UIAudio() {}

// UIDropdown constructors
UIDropdown::UIDropdown(const char *name, fl::span<fl::string> options)
    : mImpl(fl::string(name), options), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIDropdown::UIDropdown(const char *name, fl::initializer_list<fl::string> options)
    : mImpl(name, options), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIDropdown::~UIDropdown() {}

// UIGroup constructors
UIGroup::UIGroup(const fl::string& groupName) : mImpl(groupName.c_str()) {}
UIGroup::~UIGroup() {}

void UISlider::setValue(float value) {
    float oldValue = mImpl.value();
    if (value != oldValue) {
        mImpl.setValue(value);
        // Update the last frame value to keep state consistent
        mLastFrameValue = value;
        mLastFrameValueValid = true;
        // Invoke callbacks to notify listeners (including JavaScript components)
        mCallbacks.invoke(*this);
    }
}

void UISlider::Listener::onBeginFrame() {
    UISlider &owner = *mOwner;
    if (!owner.mLastFrameValueValid) {
        owner.mLastFrameValue = owner.value();
        owner.mLastFrameValueValid = true;
        return;
    }
    float value = owner.value();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(*mOwner);
        owner.mLastFrameValue = value;
    }
}

void UIButton::Listener::onBeginFrame() {
    bool clicked_this_frame = mOwner->clicked();
    bool pressed_this_frame = mOwner->isPressed();

    // Check the real button if one is attached
    if (mOwner->mRealButton) {
        if (mOwner->mRealButton->isPressed()) {
            clicked_this_frame = true;
            pressed_this_frame = true;
            //mOwner->click(); // Update the UI button state
        }
    }

    // Detect press event (was not pressed, now is pressed)
    if (pressed_this_frame && !mPressedLastFrame) {
        mOwner->mPressCallbacks.invoke();
    }

    // Detect release event (was pressed, now is not pressed)
    if (!pressed_this_frame && mPressedLastFrame) {
        mOwner->mReleaseCallbacks.invoke();
    }

    mPressedLastFrame = pressed_this_frame;

    const bool clicked_changed = (clicked_this_frame != mClickedLastFrame);
    mClickedLastFrame = clicked_this_frame;
    if (clicked_changed) {
        // FASTLED_WARN("Button: " << mOwner->name() << " clicked: " <<
        // mOwner->clicked());
        mOwner->mCallbacks.invoke(*mOwner);
    }
    // mOwner->mCallbacks.invoke(*mOwner);
}

void UICheckbox::Listener::onBeginFrame() {
    UICheckbox &owner = *mOwner;
    if (!owner.mLastFrameValueValid) {
        owner.mLastFrameValue = owner.value();
        owner.mLastFrameValueValid = true;
        return;
    }
    bool value = owner.value();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(owner);
        owner.mLastFrameValue = value;
    }
}

void UINumberField::Listener::onBeginFrame() {
    UINumberField &owner = *mOwner;
    if (!owner.mLastFrameValueValid) {
        owner.mLastFrameValue = owner.value();
        owner.mLastFrameValueValid = true;
        return;
    }
    double value = owner.value();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(owner);
        owner.mLastFrameValue = value;
    }
}

void UIDropdown::Listener::onBeginFrame() {
    UIDropdown &owner = *mOwner;
    
    // Check the next button if one is attached
    bool shouldAdvance = false;
    if (owner.mNextButton) {
        if (owner.mNextButton->clicked()) {
            shouldAdvance = true;
        }
    }
    
    // If the next button was clicked, advance to the next option
    if (shouldAdvance) {
        owner.nextOption();
        // The option change will be detected below and callbacks will be invoked
    }
    
    if (!owner.mLastFrameValueValid) {
        owner.mLastFrameValue = owner.as_int();
        owner.mLastFrameValueValid = true;
        return;
    }
    int value = owner.as_int();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(owner);
        owner.mLastFrameValue = value;
    }
}

} // end namespace fl

FL_DISABLE_WARNING_POP
