#pragma once

/// @file clockless_rp_pio_auto.h
/// @brief Automatic parallel grouping clockless LED driver for RP2040/RP2350
///
/// This file provides an automatic parallel LED strip driver that intelligently
/// groups consecutive GPIO pins for parallel output using PIO state machines.
///
/// ## Overview
///
/// Unlike the manual ParallelClocklessController, this driver:
/// - Works with standard `FastLED.addLeds()` API
/// - Automatically detects consecutive GPIO pins
/// - Groups them for parallel output (2, 4, or 8 pins)
/// - Falls back to sequential output for non-consecutive pins
/// - Uses a single PIO state machine and DMA channel per group
///
/// ## Usage Example
///
/// ```cpp
/// #include <FastLED.h>
///
/// #define NUM_LEDS_PER_STRIP 100
///
/// // Standard FastLED arrays
/// CRGB leds1[NUM_LEDS_PER_STRIP];
/// CRGB leds2[NUM_LEDS_PER_STRIP];
/// CRGB leds3[NUM_LEDS_PER_STRIP];
/// CRGB leds4[NUM_LEDS_PER_STRIP];
///
/// void setup() {
///     // Just use standard addLeds() - automatic parallel grouping!
///     FastLED.addLeds<WS2812, 2, GRB>(leds1, NUM_LEDS_PER_STRIP);
///     FastLED.addLeds<WS2812, 3, GRB>(leds2, NUM_LEDS_PER_STRIP);  // Groups with pin 2
///     FastLED.addLeds<WS2812, 4, GRB>(leds3, NUM_LEDS_PER_STRIP);  // Groups with pins 2-3
///     FastLED.addLeds<WS2812, 5, GRB>(leds4, NUM_LEDS_PER_STRIP);  // Groups with pins 2-4
/// }
///
/// void loop() {
///     // Standard FastLED.show() - all 4 strips output in parallel!
///     fill_rainbow(leds1, NUM_LEDS_PER_STRIP, millis() / 10);
///     FastLED.show();
/// }
/// ```
///
/// ## Automatic Grouping Rules
///
/// The driver automatically detects consecutive pin groups:
/// - **Consecutive pins** (2, 3, 4, 5): Grouped into single 4-lane parallel output
/// - **Non-consecutive pins** (2, 5, 10): Each uses separate sequential output
/// - **Mixed groups**: Some parallel, some sequential based on consecutiveness
///
/// Valid parallel groups: 2, 4, or 8 consecutive pins
/// Other configurations fall back to sequential (non-parallel) output.
///
/// ## Pin Requirements
///
/// **For Parallel Output (RP2040 PIO hardware limitation):**
/// - Pins MUST be consecutive GPIO numbers
/// - Valid: GPIO 2-3 (2 pins), GPIO 10-13 (4 pins), GPIO 2-9 (8 pins)
/// - Invalid: GPIO 2,4,6 (non-consecutive - will use sequential fallback)
///
/// This is a **hardware limitation** of the PIO `out pins, N` instruction.

#include "fl/compiler_control.h"
#include "fl/int.h"
#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "pixel_iterator.h"

namespace fl {

/// @brief Helper class for RP2040 PIO parallel output
///
/// This class interfaces between the controller and the singleton group manager.
/// Each controller instance has its own RP2040_PIO_Parallel helper that knows
/// its pin number and delegates to the global group for actual output.
class RP2040_PIO_Parallel {
  public:
    void beginShowLeds(int data_pin, int nleds, bool is_rgbw);
    void showPixels(u8 data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

/// @brief Base clockless controller for RP2040 with runtime pin
///
/// This base class allows dynamic pin selection at runtime.
/// Template parameter RGB_ORDER controls color ordering.
///
/// @tparam RGB_ORDER Color order (GRB, RGB, etc.)
template <EOrder RGB_ORDER = RGB>
class ClocklessController_RP2040_PIO_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    RP2040_PIO_Parallel mRP2040_PIO;
    int mPin;

  public:
    ClocklessController_RP2040_PIO_WS2812Base(int pin) : mPin(pin) {}

    void init() override {}

    virtual u16 getMaxRefreshRate() const { return 400; }

  protected:
    /// @brief Begin show cycle - queue this strip
    virtual void* beginShowLeds(int nleds) override {
        void* data = Base::beginShowLeds(nleds);
        mRP2040_PIO.beginShowLeds(mPin, nleds, this->getRgbw().active());
        return data;
    }

    /// @brief Write pixel data to buffer
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mRP2040_PIO.showPixels(mPin, pixel_iterator);
    }

    /// @brief End show cycle - trigger actual output
    virtual void endShowLeds(void* data) override {
        Base::endShowLeds(data);
        mRP2040_PIO.endShowLeds();
    }
};

/// @brief Compile-time pin clockless controller for RP2040
///
/// This template specialization provides compile-time pin checking and
/// conforms to the standard FastLED API for `addLeds<WS2812, PIN>()`.
///
/// @tparam DATA_PIN GPIO pin number (compile-time constant)
/// @tparam RGB_ORDER Color order (GRB, RGB, etc.)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_RP2040_PIO_WS2812
    : public ClocklessController_RP2040_PIO_WS2812Base<RGB_ORDER> {
  private:
    typedef ClocklessController_RP2040_PIO_WS2812Base<RGB_ORDER> Base;

  public:
    ClocklessController_RP2040_PIO_WS2812() : Base(DATA_PIN) {}

    void init() override {}

    virtual u16 getMaxRefreshRate() const { return 400; }
};

} // namespace fl
