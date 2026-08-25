// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

/// @file fl/chipsets/analog.h
/// @brief Three-pin PWM controller for non-addressable RGB LEDs and strips.

#include "cpixel_ledcontroller.h"
#include "fl/stl/noexcept.h"
#include "fl/system/pin.h"

namespace fl {

/// Drives one non-addressable RGB LED or strip from three PWM pins.
///
/// The first CRGB value attached to the controller sets the color of the
/// entire output. FastLED brightness, correction, and temperature adjustments
/// are applied before the PWM duty cycles are written.
template <u8 RED_PIN, u8 GREEN_PIN, u8 BLUE_PIN>
class AnalogRGBController : public CPixelLEDController<RGB> {
  public:
    static constexpr u32 PWM_FREQUENCY_HZ = 500;

    void init() FL_NO_EXCEPT override {
        fl::pinMode(RED_PIN, PinMode::Output);
        fl::pinMode(GREEN_PIN, PinMode::Output);
        fl::pinMode(BLUE_PIN, PinMode::Output);
        fl::setPwmFrequency(RED_PIN, PWM_FREQUENCY_HZ);
        fl::setPwmFrequency(GREEN_PIN, PWM_FREQUENCY_HZ);
        fl::setPwmFrequency(BLUE_PIN, PWM_FREQUENCY_HZ);
        writeRgb(0, 0, 0);
    }

  protected:
    void showPixels(PixelController<RGB>& pixels) FL_NO_EXCEPT override {
        if (!pixels.has(1)) {
            writeRgb(0, 0, 0);
            return;
        }

        writeRgb(pixels.loadAndScale0(), pixels.loadAndScale1(),
                 pixels.loadAndScale2());
    }

    virtual void writeRgb(u8 red, u8 green, u8 blue) FL_NO_EXCEPT {
        fl::analogWrite(RED_PIN, red);
        fl::analogWrite(GREEN_PIN, green);
        fl::analogWrite(BLUE_PIN, blue);
    }
};

}  // namespace fl

/// FastLED chipset-style wrapper for three-pin PWM RGB output.
/// @see fl::AnalogRGBController
template <fl::u8 RED_PIN, fl::u8 GREEN_PIN, fl::u8 BLUE_PIN>
class ANALOG_RGB
    : public fl::AnalogRGBController<RED_PIN, GREEN_PIN, BLUE_PIN> {};
