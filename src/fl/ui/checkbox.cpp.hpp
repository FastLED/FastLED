#include "fl/ui/checkbox.h"
#include "fl/stl/noexcept.h"

namespace fl {

UICheckbox::UICheckbox(const char *name, bool value) FL_NO_EXCEPT
    : mImpl(name, value), mLastFrameValue(false), mLastFrameValueValid(false), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UICheckbox::~UICheckbox() FL_NO_EXCEPT {}

void UICheckbox::Listener::onBeginFrame() FL_NO_EXCEPT {
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

} // namespace fl
