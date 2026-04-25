#include "fl/ui/dropdown.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIDropdown::UIDropdown(const char *name, fl::span<fl::string> options) FL_NOEXCEPT
    : mImpl(fl::string(name), options), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIDropdown::UIDropdown(const char *name, fl::initializer_list<fl::string> options) FL_NOEXCEPT
    : mImpl(name, options), mListener(this) {
    mListener.addToEngineEventsOnce();
}

UIDropdown::~UIDropdown() FL_NOEXCEPT {}

void UIDropdown::Listener::onBeginFrame() FL_NOEXCEPT {
    UIDropdown &owner = *mOwner;

    // Check the next button if one is attached (via IButtonInput interface)
    bool shouldAdvance = false;
    if (owner.mNextButton) {
        if (owner.mNextButton->clicked()) {
            shouldAdvance = true;
        }
    }

    if (shouldAdvance) {
        owner.nextOption();
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

} // namespace fl
