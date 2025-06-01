#pragma once

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/ui.h"
#include "sensors/digital_pin.h"

namespace fl {

// A simple digital pin. If we are compiling in an Arduino environment, then
// this class will bind to that. Otherwise it will fall back to the platform
// api. Note that this class does not support analog mode nor pullups/pulldowns.
class Button {
  public:
    enum Strategy {
        // FastLED doesn't have reliable support for pullups/pulldowns.
        // So we instead use a strategy where the pin is set to high, then
        // checked if it's high, then set to low, and then checked if it's low
        // if this is the case, then the pin is floating and thefore the button
        // is not
        // being pressed.
        kHighLowFloating,
        kPullUp,
    };

    Button(int pin, Strategy strategy = kHighLowFloating);
    ~Button();
    Button(const Button &other) = default;
    Button &operator=(const Button &other) = delete;
    Button(Button &&other) = delete;
    bool isPressed();

    bool highLowFloating();

  private:
    fl::DigitalPin mPin;
    Strategy mStrategy = kHighLowFloating;
};

// This advanced button hooks into the FastLED beginFrame() engine
// events and will detect button clicks, presses, and releases.
// You can hook in callbacks to the button state changes for
// onClick(), onPress(), and onRelease().
class ButtonAdvanced {
  public:
    ButtonAdvanced(int pin,
                   Button::Strategy strategy = Button::kHighLowFloating);

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(ButtonAdvanced *owner);
        ~Listener();
        void addToEngineEventsOnce();
        void onBeginFrame() override;

      private:
        ButtonAdvanced *mOwner;
        bool added = false;
        bool mClickedLastFrame = false;
    };

  private:
    Button mButton;
    Listener mListener;
};

} // namespace fl
