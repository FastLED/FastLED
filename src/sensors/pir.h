#pragma once

#include <stdint.h>

#include "digital_pin.h"
#include "fl/ptr.h"
#include "ui.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN
class InputPin;
FASTLED_NAMESPACE_END

namespace fl {

// A passive infrared sensor common on amazon.
// Instantiating this class will create a ui Button when
// compiling using the FastLED web compiler.
class Pir {
  public:
    Pir(int pin);
    bool detect();
    operator bool() { return detect(); }

  private:
    Button mButton;
    DigitalPin mPin;
};

// An advanced PIR that incorporates time to allow for latching and transitions fx.
class PirLatching {
  public:
    PirLatching(int pin, uint32_t latchMs = 0, uint32_t risingTime = 0, uint32_t fallingTime = 0);
    bool detect(uint32_t now);
    uint8_t transition(uint32_t now);  // Returns alpha value 0-255
    void activate(uint32_t now) { mLastTrigger = now; }

  private:
    Pir mPir;
    uint32_t mLatchMs;
    uint32_t mRisingTime;
    uint32_t mFallingTime; 
    uint32_t mLastTrigger;
    bool mLastState;
};

} // namespace fl
