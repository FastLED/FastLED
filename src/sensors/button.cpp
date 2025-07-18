

#include "fl/stdint.h"

#include "fl/memory.h"
#include "fl/ui.h"

#include "fl/assert.h"
#include "fl/namespace.h"
#include "sensors/button.h"
#include "sensors/digital_pin.h"

namespace fl {

ButtonLowLevel::ButtonLowLevel(int pin, ButtonStrategy strategy)
    : mPin(pin) {
    setStrategy(strategy);
}

ButtonLowLevel::~ButtonLowLevel() {}

bool ButtonLowLevel::highLowFloating() {
    // FastLED doesn't have reliable support for pullups/pulldowns.
    // So we instead use a strategy where the pin is set to high, then
    // checked if it's high, then set to low, and then checked if it's low
    // if this is the case, then the pin is floating and thefore the button is
    // not being pressed.
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
    // FastLED doesn't have reliable support for pullups/pulldowns.
    // So we instead use a strategy where the pin is set to high, then
    // checked if it's high, then set to low, and then checked if it's low
    // if this is the case, then the pin is floating and thefore the button is
    // not being pressed. return (mStrategy == kHighLowFloating) ?
    // highLowFloating() :
    //        (mStrategy == kPullUp) ? mPin.high() : // not implemented yet
    //        (mStrategy == kPullDown) ? !mPin.high() : // not implemented yet
    //        false; // unknown strategy, return false
    switch (mStrategy) {
    case kHighLowFloating:
        return highLowFloating();
    case kPullUp:
        return mPin.high(); // not implemented yet
    default:
        FASTLED_ASSERT(false, "Unknown ButtonLowLevel strategy");
        return false; // unknown strategy, return false
    }
}

Button::Button(int pin, ButtonStrategy strategy)
    : mButton(pin, strategy), mListener(this) {}

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
        mPin.setPinMode(DigitalPin::kInput); // Set pin to input mode
        break;
    case kPullUp:
        mPin.setPinMode(
            DigitalPin::kInputPullup); // Set pin to input pullup mode
        break;
    default:
        // Unknown strategy, do nothing
        FASTLED_ASSERT(false, "Unknown ButtonLowLevel strategy");
        break;
    }
}

} // namespace fl
