

#include "fl/stl/stdint.h"

#include "fl/ui.h"

#include "fl/stl/assert.h"
#include "fl/sensors/button.h"
#include "fl/sensors/digital_pin.h"

namespace fl {

ButtonLowLevel::ButtonLowLevel(int pin, ButtonStrategy strategy)
    : mPin(pin) {
    setStrategy(strategy);
}

ButtonLowLevel::~ButtonLowLevel() {}

bool ButtonLowLevel::highLowFloating() {
    // High-low floating detection: Set pin to high, check if high,
    // set pin to low, check if low. If both conditions are true,
    // the pin is floating and therefore the button is not pressed.
    mPin.setPinMode(DigitalPin::kOutput);
    mPin.write(true); // set pin to high
    mPin.setPinMode(DigitalPin::kInput);
    const bool was_high = mPin.high(); // check if pin is high
    mPin.setPinMode(DigitalPin::kOutput);
    mPin.write(false); // set pin to low
    mPin.setPinMode(DigitalPin::kInput);
    const bool was_low = !mPin.high(); // check if pin is low
    const bool floating =
        was_high && was_low; // if both are true, then the pin is floating
    const bool pressed =
        !floating; // if the pin is floating, then the button is not pressed
    return pressed;
}

bool ButtonLowLevel::isPressed() {
    switch (mStrategy) {
    case kHighLowFloating:
        return highLowFloating();
    case kPullUp:
        // Active-low: Button pulls pin to ground when pressed
        return !mPin.high();
    case kPullDown:
        // Active-high: Button pulls pin to VCC when pressed
        return mPin.high();
    default:
        FASTLED_ASSERT(false, "Unknown ButtonLowLevel strategy");
        return false;
    }
}

Button::Button(int pin, ButtonStrategy strategy)
    : mButton(pin, strategy), mListener(this), mPressedLastFrame(false), mClickedThisFrame(false) {}

void Button::Listener::onEndFrame() {
    const bool pressed_curr_frame = mOwner->mButton.isPressed();
    const bool pressed_last_frame = mOwner->mPressedLastFrame;
    const bool changed_this_frame = pressed_curr_frame != pressed_last_frame;
    mOwner->mPressedLastFrame = pressed_curr_frame;
    if (changed_this_frame && pressed_curr_frame) {
        mOwner->mClickedThisFrame = true;
        mOwner->mOnClickCallbacks.invoke();
    }
}

Button::Listener::Listener(Button *owner) : mOwner(owner) {
    addToEngineEventsOnce();
}

Button::Listener::~Listener() {
    if (added) {
        EngineEvents::removeListener(this);
    }
}

void Button::Listener::addToEngineEventsOnce() {
    if (added) {
        return;
    }
    EngineEvents::addListener(this, 1);  // One high priority so that it runs before UI elements.
    added = true;
}

int Button::onClick(function<void()> callback) {
    int id = mOnClickCallbacks.add(callback);
    return id;
}



void ButtonLowLevel::setStrategy(ButtonStrategy strategy) {
    mStrategy = strategy;
    switch (mStrategy) {
    case kHighLowFloating:
        mPin.setPinMode(DigitalPin::kInput);
        break;
    case kPullUp:
        mPin.setPinMode(DigitalPin::kInputPullup);
        break;
    case kPullDown:
        mPin.setPinMode(DigitalPin::kInputPulldown);
        break;
    default:
        FASTLED_ASSERT(false, "Unknown ButtonLowLevel strategy");
        break;
    }
}

} // namespace fl
