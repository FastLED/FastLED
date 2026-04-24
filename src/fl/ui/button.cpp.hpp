#include "fl/ui/button.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIButton::UIButton(const char *name) FL_NOEXCEPT : mImpl(name), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIButton::~UIButton() FL_NOEXCEPT {}

void UIButton::Listener::onBeginFrame() FL_NOEXCEPT {
    bool clicked_this_frame = mOwner->clicked();
    bool pressed_this_frame = mOwner->isPressed();

    // Check the real button if one is attached (via IButtonInput interface)
    if (mOwner->mButtonInput) {
        if (mOwner->mButtonInput->isPressed()) {
            clicked_this_frame = true;
            pressed_this_frame = true;
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
        mOwner->mCallbacks.invoke(*mOwner);
    }
}

} // namespace fl
