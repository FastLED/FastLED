#pragma once

#include "fl/stdint.h"

#include "digital_pin.h"
#include "fl/memory.h"
#include "fl/ui.h"
#include "fl/time_alpha.h"

#include "fl/namespace.h"


namespace fl {

// A passive infrared sensor common on amazon.
// For best results set the PIR to maximum sensitive and minimum delay time before retrigger.
// Instantiating this class will create a ui UIButton when
// compiling using the FastLED web compiler.
class PirLowLevel {
  public:
    PirLowLevel(int pin);
    bool detect();
    operator bool() { return detect(); }

  private:

    DigitalPin mPin;
};

// An passive infrared sensor that incorporates time to allow for latching and transitions fx. This is useful
// for detecting motion and the increasing the brightness in response to motion.
// Example:
//   #define PIR_LATCH_MS 15000  // how long to keep the PIR sensor active after a trigger
//   #define PIR_RISING_TIME 1000  // how long to fade in the PIR sensor
//   #define PIR_FALLING_TIME 1000  // how long to fade out the PIR sensor
//   Pir pir(PIN_PIR, PIR_LATCH_MS, PIR_RISING_TIME, PIR_FALLING_TIME);
//   void loop() {
//      uint8_t bri = pir.transition(millis());
//      FastLED.setBrightness(bri * brightness.as<float>());
//   }

class Pir {
public:
    /// @param pin         GPIO pin for PIR sensor
    /// @param latchMs     total active time (ms)
    /// @param risingTime  ramp‑up duration (ms)
    /// @param fallingTime ramp‑down duration (ms)
    Pir(int pin,
                uint32_t latchMs     = 5000,
                uint32_t risingTime  = 1000,
                uint32_t fallingTime = 1000,
                const char* button_name = nullptr);

    /// Returns true if the PIR is “latched on” (within latchMs of last trigger).
    bool detect(uint32_t now);

    /// Returns a 0–255 ramp value:
    ///  • ramps 0→255 over risingTime
    ///  • holds 255 until latchMs–fallingTime
    ///  • ramps 255→0 over fallingTime
    /// Outside latch period returns 0.
    uint8_t transition(uint32_t now);

    /// Manually start the latch cycle (e.g. on startup)
    void activate(uint32_t now) { mRamp.trigger(now); }

private:
    PirLowLevel        mPir;
    TimeRamp  mRamp;
    bool       mLastState = false;
    UIButton mButton;
    
};

} // namespace fl
