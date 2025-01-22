#define FASTLED_INTERNAL
#include "FastLED.h"

#include "sensors/pir.h"
#include "fl/strstream.h"
#include "fastpin.h"
#include "fl/warn.h"


namespace fl {

namespace {
  int g_counter = 0;
  Str getButtonName(const char* button_name) {
    if (button_name) {
      return Str(button_name);
    }
    int count = g_counter++;
    if (count == 0) {
      return Str("PIR");
    }
    StrStream s;
    s << "Pir " << g_counter++;
    return s.str();
  }
}


Pir::Pir(int pin, const char* button_name): mButton(getButtonName(button_name).c_str()), mPin(pin) {
    mPin.setPinMode(DigitalPin::kInput);
}

bool Pir::detect() {
    return mPin.high() || mButton.clicked();
}

PirAdvanced::PirAdvanced(int pin, uint32_t latchMs, uint32_t risingTime, uint32_t fallingTime) 
    : mPir(pin)
    , mLatchMs(latchMs)
    , mRisingTime(risingTime)
    , mFallingTime(fallingTime)
    , mLastTrigger(0)
    , mLastState(false) {

    if (mRisingTime + mFallingTime > mLatchMs) {
        FASTLED_WARN("PirAdvanced: risingTime + fallingTime must be less than latchMs");
        mRisingTime = mLatchMs / 2;
        mFallingTime = mLatchMs / 2;
    }
}

bool PirAdvanced::detect(uint32_t now) {
    bool currentState = mPir.detect();
    
    if (currentState && !mLastState) {
        mLastTrigger = now;
    }
    
    mLastState = currentState;
    return (now - mLastTrigger) < mLatchMs;
}

uint8_t PirAdvanced::transition(uint32_t now) {
    detect(now);
    uint32_t elapsed = now - mLastTrigger;
    
    uint8_t out = 0;
    if (elapsed < mRisingTime) {
        // Rising phase - alpha goes from 0 to 255
        out = MAX(mLastTransition, (elapsed * 255) / mRisingTime);
    } 
    else if (elapsed >= mLatchMs - mFallingTime && elapsed < mLatchMs) {
        // Falling phase - alpha goes from 255 to 0
        uint32_t fallingElapsed = elapsed - (mLatchMs - mFallingTime);
        out = 255 - ((fallingElapsed * 255) / mFallingTime);
    }
    else if (elapsed >= mRisingTime && elapsed < mLatchMs - mFallingTime) {
        // Fully on
        out = 255;
    }
    mLastTransition = out;
    // Outside latch period
    return out;
}

}  // namespace fl
