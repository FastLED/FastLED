// ok no header
/// @file ui_button_integration.cpp.hpp
/// @brief UIButton/UIDropdown <-> Button integration via IButtonInput interface.
/// Compiled in fl.sensors+ to keep concrete Button out of fl.cpp link chain.

#include "fl/ui.h"
#include "fl/sensors/button.h"

namespace fl {

void UIButton::addRealButton(fl::shared_ptr<IButtonInput> button) {
    // Stored directly; no concrete-type dependency. Caller may pass any
    // IButtonInput implementation (Button, mock, etc.).
    mButtonInput = button;
}

void UIDropdown::addNextButton(int pin) {
    mNextButton = fl::make_shared<Button>(pin);
}

} // namespace fl
