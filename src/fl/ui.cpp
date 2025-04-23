

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
    UISlider& owner = *mOwner;
    if (!owner.mLastFramevalueValid) {
        owner.mLastFrameValue = owner.value();
        owner.mLastFramevalueValid = true;
        return;
    }
    float value = owner.value();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(value);
        owner.mLastFrameValue = value;
    }
}

} // end namespace fl

#pragma GCC diagnostic pop