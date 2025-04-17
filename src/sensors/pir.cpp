#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fastpin.h"
#include "fl/strstream.h"
#include "fl/warn.h"
#include "sensors/pir.h"

namespace fl {

namespace {
int g_counter = 0;
Str getButtonName(const char *button_name) {
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
} // namespace

Pir::Pir(int pin, const char* button_name): mButton(getButtonName(button_name).c_str()), mPin(pin) {
    mPin.setPinMode(DigitalPin::kInput);
}

bool Pir::detect() {
    return mPin.high() || mButton.clicked();
}


PirAdvanced::PirAdvanced(int pin, uint32_t latchMs, uint32_t risingTime,
                         uint32_t fallingTime)
    : mPir(pin), mRamp(risingTime, latchMs, fallingTime) {
}

bool PirAdvanced::detect(uint32_t now) {
    bool currentState = mPir.detect();
    if (currentState && !mLastState) {
        mRamp.trigger(now);
    }
    mLastState = currentState;
    return mRamp.isActive(now);
}

uint8_t PirAdvanced::transition(uint32_t now) {
    // ensure detect() logic runs so we trigger on edges
    detect(now);
    return mRamp.update(now);
}

} // namespace fl
