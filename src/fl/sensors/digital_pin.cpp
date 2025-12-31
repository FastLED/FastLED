
#include "fl/stl/stdint.h"

#include "fl/ui.h"
#include "fl/stl/shared_ptr.h"  // For make_shared

#include "fl/sensors/digital_pin.h"

// Include fl/pin.h for pin API (includes platform implementations)
#include "fl/pin.h"

namespace fl {

class DigitalPinImpl {
  public:
    DigitalPinImpl(int pin) : mPin(pin) {}
    ~DigitalPinImpl() = default;

    void setPinMode(DigitalPin::Mode mode) {
        fl::PinMode pinMode;
        switch (mode) {
            case DigitalPin::kInput:
                pinMode = fl::PinMode::Input;
                break;
            case DigitalPin::kOutput:
                pinMode = fl::PinMode::Output;
                break;
            case DigitalPin::kInputPullup:
                pinMode = fl::PinMode::InputPullup;
                break;
            case DigitalPin::kInputPulldown:
                pinMode = fl::PinMode::InputPulldown;
                break;
            default:
                return;
        }
        fl::pinMode(mPin, pinMode);
    }

    bool high() {
        return fl::digitalRead(mPin) == fl::PinValue::High;
    }

    void write(bool value) {
        fl::digitalWrite(mPin, value ? fl::PinValue::High : fl::PinValue::Low);
    }

  private:
    int mPin;
};


DigitalPin::DigitalPin(int pin) {
    mImpl = fl::make_shared<DigitalPinImpl>(pin);
}
DigitalPin::~DigitalPin() = default;
DigitalPin::DigitalPin(const DigitalPin &other) = default;

DigitalPin& DigitalPin::operator=(const DigitalPin &other) = default;

void DigitalPin::setPinMode(Mode mode) {
    mImpl->setPinMode(mode);
}

bool DigitalPin::high() const {
    return mImpl->high();
}

void DigitalPin::write(bool is_high) {
    mImpl->write(is_high);
}

}  // namespace fl
