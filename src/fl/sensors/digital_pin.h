#pragma once

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(DigitalPinImpl);

// A simple digital pin that provides a unified interface across all platforms
// using fl/pin.h. Supports digital I/O, pullup/pulldown resistors.
// Note: Analog mode is not supported by this class (use fl::analogRead directly).
class DigitalPin {
  public:
    enum Mode {
        kInput = 0,
        kOutput,
        kInputPullup,
        kInputPulldown,
    };

    DigitalPin(int pin);
    ~DigitalPin() FL_NOEXCEPT;
    DigitalPin(const DigitalPin &other) FL_NOEXCEPT;
    DigitalPin &operator=(const DigitalPin &other) FL_NOEXCEPT;

    DigitalPin(DigitalPin &&other) FL_NOEXCEPT = delete;

    void setPinMode(Mode mode);
    bool high() const;  // true if high, false if low
    void write(bool is_high);
  private:
    DigitalPinImplPtr mImpl;
};

}  // namespace fl
