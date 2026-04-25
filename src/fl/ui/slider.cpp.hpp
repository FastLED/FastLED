#include "fl/ui/slider.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

UISlider::UISlider(const char *name, float value, float min, float max, float step) FL_NOEXCEPT
    : mImpl(name, value, min, max, step), mListener(this) {
    mListener.addToEngineEventsOnce();
}

void UISlider::setValue(float value) FL_NOEXCEPT {
    float oldValue = mImpl.value();
    if (value != oldValue) {
        mImpl.setValue(value);
        // Update the last frame value to keep state consistent.
        // Read back from mImpl in case setValue clamped to [min, max].
        mLastFrameValue = mImpl.value();
        mLastFrameValueValid = true;
        // Invoke callbacks to notify listeners (including JavaScript components)
        mCallbacks.invoke(*this);
    }
}

void UISlider::Listener::onBeginFrame() FL_NOEXCEPT {
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

} // namespace fl

FL_DISABLE_WARNING_POP
