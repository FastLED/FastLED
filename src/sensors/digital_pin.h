#pragma once


#include <stdint.h>

#include "fl/ui.h"
#include "fl/ptr.h"

#include "fl/namespace.h"

namespace fl {

FASTLED_SMART_PTR(DigitalPinImpl);


// A simple digital pin. If we are compiling in an Arduino environment, then
// this class will bind to that. Otherwise it will fall back to the platform api.
// Note that this class does not support analog mode nor pullups/pulldowns.
class DigitalPin {
  public:
    enum Mode {
        kInput = 0,
        kOutput,
        // the fallback mechanism does not support pullups or pulldowns, so we don't either yet.
        // TODO: Add these for the fallback mechanism.
        // kInputPullup,
        // kInputPulldown,
    };

    DigitalPin(int pin);
    ~DigitalPin();
    DigitalPin(const DigitalPin &other);
    DigitalPin &operator=(const DigitalPin &other);

    DigitalPin(DigitalPin &&other) = delete;

    void setPinMode(Mode mode);
    bool high() const;  // true if high, false if low
    void write(bool is_high);
  private:
    DigitalPinImplPtr mImpl;
};

}  // namespace fl