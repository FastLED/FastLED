#pragma once

// IWYU pragma: private

/// Adapters from FastLED's type-safe pin API to Teknic's ClearCore Arduino
/// connector API. ClearCore pin numbers name connectors rather than linear MCU
/// GPIOs, so the generic SAMD pin mapper is not valid for this board.

#include "fl/stl/noexcept.h"
#include "fl/system/arduino.h"

namespace fl {
namespace platforms {

inline void pinMode(int pin, PinMode mode) FL_NO_EXCEPT {
    ::PinMode arduino_mode = INPUT;
    switch (mode) {
        case PinMode::Input:
            arduino_mode = INPUT;
            break;
        case PinMode::Output:
            arduino_mode = OUTPUT;
            break;
        case PinMode::InputPullup:
        case PinMode::InputPulldown:
            // ClearCore's connector API exposes INPUT and OUTPUT only.
            arduino_mode = INPUT;
            break;
    }
    ::pinMode(static_cast<pin_size_t>(pin), arduino_mode);
}

inline void digitalWrite(int pin, PinValue value) FL_NO_EXCEPT {
    ::digitalWrite(static_cast<pin_size_t>(pin),
                   value == PinValue::High ? HIGH : LOW);
}

inline PinValue digitalRead(int pin) FL_NO_EXCEPT {
    return ::digitalRead(static_cast<pin_size_t>(pin)) ? PinValue::High
                                                       : PinValue::Low;
}

inline u16 analogRead(int pin) FL_NO_EXCEPT {
    const int value = ::analogRead(static_cast<pin_size_t>(pin));
    return value > 0 ? static_cast<u16>(value) : 0U;
}

inline void analogWrite(int pin, u16 value) FL_NO_EXCEPT {
    ::analogWrite(static_cast<pin_size_t>(pin), value);
}

inline void setPwm16(int pin, u16 value) FL_NO_EXCEPT {
    ::analogWrite(static_cast<pin_size_t>(pin), value);
}

inline void setAdcRange(AdcRange /*range*/) FL_NO_EXCEPT {
    // ClearCore selects ADC units per analogRead() call.
}

inline bool needsPwmIsrFallback(int /*pin*/, u32 /*frequency_hz*/) {
    return true;
}

inline int setPwmFrequencyNative(int /*pin*/, u32 /*frequency_hz*/) {
    return -4;
}

inline u32 getPwmFrequencyNative(int /*pin*/) { return 0; }

} // namespace platforms
} // namespace fl
