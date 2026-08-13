#pragma once

// IWYU pragma: private

// Adapters from FastLED's type-safe pin API to the Chipintelli Arduino core.

#include "fl/stl/noexcept.h"
#include "fl/system/arduino.h"

namespace fl {
namespace platforms {

inline void pinMode(int pin, PinMode mode) FL_NO_EXCEPT {
    u8 arduinoMode = INPUT;
    switch (mode) {
        case PinMode::Input:
            arduinoMode = INPUT;
            break;
        case PinMode::Output:
            arduinoMode = OUTPUT;
            break;
        case PinMode::InputPullup:
            arduinoMode = INPUT_PULLUP;
            break;
        case PinMode::InputPulldown:
            arduinoMode = INPUT_PULLDOWN;
            break;
    }
    ::pinMode(static_cast<u8>(pin), arduinoMode);
}

inline void digitalWrite(int pin, PinValue value) FL_NO_EXCEPT {
    ::digitalWrite(static_cast<u8>(pin),
                   value == PinValue::High ? HIGH : LOW);
}

inline PinValue digitalRead(int pin) FL_NO_EXCEPT {
    return ::digitalRead(static_cast<u8>(pin)) ? PinValue::High
                                               : PinValue::Low;
}

inline u16 analogRead(int pin) FL_NO_EXCEPT {
    const int value = ::analogRead(static_cast<u8>(pin));
    return value > 0 ? static_cast<u16>(value) : 0U;
}

inline void analogWrite(int pin, u16 value) FL_NO_EXCEPT {
    ::analogWrite(static_cast<u8>(pin), static_cast<int>(value));
}

inline void setPwm16(int pin, u16 value) FL_NO_EXCEPT {
    ::analogWriteResolution(16);
    ::analogWrite(static_cast<u8>(pin), static_cast<int>(value));
    ::analogWriteResolution(8);
}

inline void setAdcRange(AdcRange /*range*/) FL_NO_EXCEPT {
    // CI13XX ADC reference selection is managed by the Arduino core.
}

inline bool needsPwmIsrFallback(int /*pin*/, u32 /*frequencyHz*/) {
    return false;
}

inline int setPwmFrequencyNative(int /*pin*/, u32 frequencyHz)
    FL_NO_EXCEPT {
    ::analogWriteFrequency(frequencyHz);
    return 0;
}

inline u32 getPwmFrequencyNative(int /*pin*/) FL_NO_EXCEPT { return 0; }

}  // namespace platforms
}  // namespace fl
