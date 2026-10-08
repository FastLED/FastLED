
#include "fl/stl/stdint.h"

#include "fl/ui/ui.h"
#include "fl/stl/shared_ptr.h"  // For make_shared

#include "fl/sensors/digital_pin.h"

// Include fl/pin.h for pin API (includes platform implementations)
#include "fl/system/pin.h"
#include "fl/stl/noexcept.h"

namespace fl {

class DigitalPinImpl {
  public:
    DigitalPinImpl(int pin) FL_NO_EXCEPT : mPin(pin) {}
    ~DigitalPinImpl() FL_NO_EXCEPT = default;

    void setPinMode(DigitalPin::Mode mode) FL_NO_EXCEPT {
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

    bool high() FL_NO_EXCEPT {
        return fl::digitalRead(mPin) == fl::PinValue::High;
    }

    void write(bool value) FL_NO_EXCEPT {
        fl::digitalWrite(mPin, value ? fl::PinValue::High : fl::PinValue::Low);
    }

  private:
    int mPin;
};


DigitalPin::DigitalPin(int pin) FL_NO_EXCEPT {
    mImpl = fl::make_shared<DigitalPinImpl>(pin);
}
DigitalPin::~DigitalPin() FL_NO_EXCEPT = default;
DigitalPin::DigitalPin(const DigitalPin &other) FL_NO_EXCEPT = default;

DigitalPin& DigitalPin::operator=(const DigitalPin &other) FL_NO_EXCEPT = default;

void DigitalPin::setPinMode(Mode mode) FL_NO_EXCEPT {
    mImpl->setPinMode(mode);
}

bool DigitalPin::high() const FL_NO_EXCEPT {
    return mImpl->high();
}

void DigitalPin::write(bool is_high) FL_NO_EXCEPT {
    mImpl->write(is_high);
}

}  // namespace fl
