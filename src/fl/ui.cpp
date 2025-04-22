

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

void UISlider::onBeginFrame() {
    if (!mLastFramevalueValid) {
        mLastFrameValue = Super::value();
        mLastFramevalueValid = true;
        return;
    }
    float value = Super::value();
    if (value != mLastFrameValue) {
        mCallbacks.invoke(value);
        mLastFrameValue = value;
    }
}

} // end namespace fl

#pragma GCC diagnostic pop