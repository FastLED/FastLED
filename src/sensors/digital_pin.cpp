
#include <stdint.h>

#include "fl/ui.h"
#include "fl/ptr.h"

#include "namespace.h"
#include "digital_pin.h"



#if !defined(USE_ARDUINO) && __has_include(<Arduino.h>)
#define USE_ARDUINO 1
#else
#define USE_ARDUINO 0
#endif


#if USE_ARDUINO
#include <Arduino.h>  // ok include
#else
// Fallback
#define FASTLED_INTERNAL
#include "FastLED.h"
#include "fastpin.h"
#endif


namespace fl {


#if USE_ARDUINO
class DigitalPinImpl : public Referent {
  public:
    DigitalPinImpl(int DigitalPin) : mDigitalPin(DigitalPin) {}
   ~DigitalPinImpl() = default;

    void setPinMode(DigitalPin::Mode mode) {
        switch (mode) {
            case DigitalPin::kInput:
                ::pinMode(mDigitalPin, INPUT);
                break;
            case DigitalPin::kOutput:
                ::pinMode(mDigitalPin, OUTPUT);
                break;
        }
    }
    bool digitalRead() { return HIGH == ::digitalRead(mDigitalPin); }
    void digitalWrite(bool value) { ::digitalWrite(mDigitalPin, value ? HIGH : LOW); }

  private:
    int mDigitalPin;
};

#else
class DigitalPinImpl : public Referent {
  public:
    DigitalPinImpl(int pin) : mPin(pin) {}
    ~DigitalPinImpl() = default;

    void setPinMode(DigitalPin::Mode mode) {
        switch (mode) {
            case DigitalPin::kInput:
                mPin.setInput();
                break;
            case DigitalPin::kOutput:
                mPin.setOutput();
                break;
        }
    }

    bool digitalRead() { return mPin.hival(); }
    void digitalWrite(bool value) { value ? mPin.hi(): mPin.lo(); }
    // define pin
    Pin mPin;
};
#endif


DigitalPin::DigitalPin(int DigitalPin) {
    mImpl = DigitalPinImplPtr::New(DigitalPin);
}
DigitalPin::~DigitalPin() = default;
DigitalPin::DigitalPin(const DigitalPin &other) = default;

DigitalPin& DigitalPin::operator=(const DigitalPin &other) = default;

void DigitalPin::setPinMode(Mode mode) {
    mImpl->setPinMode(mode);
}

bool DigitalPin::high() const {
    return mImpl->digitalRead();
}

void DigitalPin::write(bool is_high) {
    mImpl->digitalWrite(is_high);
}

}  // namespace fl