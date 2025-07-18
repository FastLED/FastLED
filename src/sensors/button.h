#pragma once

#include "fl/stdint.h"

#include "fl/function_list.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/ui.h"
#include "sensors/digital_pin.h"

namespace fl {

enum ButtonStrategy {

    // FastLED doesn't have reliable support for pullups/pulldowns.
    // So we instead use a strategy where the pin is set to high, then
    // checked if it's high, then set to low, and then checked if it's low
    // if this is the case, then the pin is floating and thefore the button
    // is not
    // being pressed.
    kHighLowFloating,
    kPullUp,

};

// A simple digital pin. If we are compiling in an Arduino environment, then
// this class will bind to that. Otherwise it will fall back to the platform
// api. Note that this class does not support analog mode nor pullups/pulldowns.
class ButtonLowLevel {
  public:
    ButtonLowLevel(int pin, ButtonStrategy strategy = kHighLowFloating);
    ~ButtonLowLevel();
    ButtonLowLevel(const ButtonLowLevel &other) = default;
    ButtonLowLevel &operator=(const ButtonLowLevel &other) = delete;
    ButtonLowLevel(ButtonLowLevel &&other) = delete;
    bool isPressed();

    bool highLowFloating();

    void setStrategy(ButtonStrategy strategy);

  private:
    fl::DigitalPin mPin;
    ButtonStrategy mStrategy = kHighLowFloating;
};

// The default button type hooks into the FastLED EngineEvents to monitor
// whether the button is pressed or not. You do not need to run an update
// function. If you need more control, use ButtonLowLevel directly.
class Button {
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

    bool isPressed() {
        return mButton.isPressed();
    }

    bool clicked() const {
        // If we have a real button, check if it's pressed
        return mClickedThisFrame;
    }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(Button *owner);
        ~Listener();
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

    fl::FunctionList<void> mOnClickCallbacks;
    // fl::FunctionList<void(Button&)> mOnChangedCallbacks;
};

} // namespace fl
