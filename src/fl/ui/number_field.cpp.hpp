#include "fl/ui/number_field.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

UINumberField::UINumberField(const char *name, double value, double min, double max) FL_NOEXCEPT
    : mImpl(name, value, min, max), mListener(this), mLastFrameValue(0), mLastFrameValueValid(false) {
    mListener.addToEngineEventsOnce();
}

UINumberField::~UINumberField() FL_NOEXCEPT {}

void UINumberField::Listener::onBeginFrame() FL_NOEXCEPT {
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

} // namespace fl

FL_DISABLE_WARNING_POP
