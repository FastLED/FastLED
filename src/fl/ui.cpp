

#include "fl/ui.h"
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

namespace fl {

void UISlider::setValue(float value) {

    if (value != Super::value()) {
        Super::setValue(value);
    }
}

void UISlider::Listener::onBeginFrame() {
    UISlider &owner = *mOwner;
    if (!owner.mLastFramevalueValid) {
        owner.mLastFrameValue = owner.value();
        owner.mLastFramevalueValid = true;
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
    
    // Check the real button if one is attached
    if (mOwner->mRealButton) {
        if (mOwner->mRealButton->isPressed()) {
            clicked_this_frame = true;
            //mOwner->click(); // Update the UI button state
        }
    }
    
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

} // end namespace fl

#pragma GCC diagnostic pop
