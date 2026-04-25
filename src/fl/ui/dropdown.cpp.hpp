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

    // Seed baseline before any first-frame advance so a nextOption() triggered
    // on the very first frame still produces a change callback.
    if (!owner.mLastFrameValueValid) {
        owner.mLastFrameValue = owner.as_int();
        owner.mLastFrameValueValid = true;
    }

    // Check the next button if one is attached (via IButtonInput interface)
    if (owner.mNextButton && owner.mNextButton->clicked()) {
        owner.nextOption();
    }

    int value = owner.as_int();
    if (value != owner.mLastFrameValue) {
        owner.mCallbacks.invoke(owner);
        owner.mLastFrameValue = value;
    }
}

} // namespace fl
