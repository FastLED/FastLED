#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/system/engine_events.h"
#include "fl/sensors/digital_pin.h"
#include "fl/ui.h"  // For IButtonInput
#include "fl/stl/noexcept.h"

namespace fl {

enum class ButtonStrategy {
    // High-low floating detection strategy: Sets pin to high, checks if high,
    // sets pin to low, checks if low. If both are true, pin is floating and
    // button is not pressed. Useful for detecting buttons without pull resistors.
    kHighLowFloating,

    // Active-low button strategy: Uses internal pull-up resistor.
    // Button connects pin to ground when pressed. Reading LOW = pressed.
    kPullUp,

    // Active-high button strategy: Uses internal pull-down resistor.
    // Button connects pin to VCC when pressed. Reading HIGH = pressed.
    kPullDown,
};

// A simple digital pin. If we are compiling in an Arduino environment, then
// this class will bind to that. Otherwise it will fall back to the platform
// api. Note that this class does not support analog mode nor pullups/pulldowns.
class ButtonLowLevel {
  public:
    ButtonLowLevel(int pin, ButtonStrategy strategy = ButtonStrategy::kHighLowFloating);
    ~ButtonLowLevel() FL_NOEXCEPT;
    ButtonLowLevel(const ButtonLowLevel &other) FL_NOEXCEPT = default;
    ButtonLowLevel &operator=(const ButtonLowLevel &other) FL_NOEXCEPT = delete;
    ButtonLowLevel(ButtonLowLevel &&other) FL_NOEXCEPT = delete;
    bool isPressed();

    bool highLowFloating();

    void setStrategy(ButtonStrategy strategy);

  private:
    fl::DigitalPin mPin;
    ButtonStrategy mStrategy = ButtonStrategy::kHighLowFloating;
};

// The default button type hooks into the FastLED EngineEvents to monitor
// whether the button is pressed or not. You do not need to run an update
// function. If you need more control, use ButtonLowLevel directly.
class Button : public IButtonInput {
  public:
    Button(int pin,
           ButtonStrategy strategy = ButtonStrategy::kHighLowFloating);

    int onClick(fl::function<void()> callback);
    void removeOnClick(int id) {
        mOnClickCallbacks.remove(id);
    }

    void setStrategy(ButtonStrategy strategy) {
        mButton.setStrategy(strategy);
    }

    bool isPressed() override {
        return mButton.isPressed();
    }

    bool clicked() override {
        return mClickedThisFrame;
    }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(Button *owner);
        ~Listener() FL_NOEXCEPT;
        void addToEngineEventsOnce();

        // We do an experiment here, what about listening to the end frame event
        // instea do of the begin frame event? This will put the activation of
        // this button **before** the next frame. I think this pattern should be
        // used for all UI elements, so that the button state is updated before
        // the next frame is drawn. This seems like the only way to do this, or
        // by using platform pre loop, but not all platforms support that.
        void onEndFrame() override;

      private:
        Button *mOwner;
        bool added = false;
    };

  private:
    ButtonLowLevel mButton;
    Listener mListener;
    bool mPressedLastFrame = false;  // Don't read this variale, it's used internally.
    bool mClickedThisFrame = false;  // This is true if clicked this frame.

    fl::function_list<void()> mOnClickCallbacks;
    // fl::function_list<void(Button&)> mOnChangedCallbacks;
};

} // namespace fl
