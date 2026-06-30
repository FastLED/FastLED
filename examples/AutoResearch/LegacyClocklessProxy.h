// LegacyClocklessProxy.h - Runtime-to-template pin dispatch for legacy addLeds API
//
// Maps a runtime pin number and chipset name to compile-time clockless template
// instantiations via a switch statement. This allows AutoResearch testing of the
// legacy template API path: CHIPSET<PIN> -> ClocklessControllerImpl ->
// ClocklessIdf5 -> Channel.

#pragma once

#include <FastLED.h>
#include "fl/stl/unique_ptr.h"

enum class LegacyClocklessChipset : uint8_t {
    WS2812B,
    SK6812,
};

#if defined(FL_IS_TEENSY_4X)
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_22 1
#else
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_22 0
#endif

// GPIO6/7/8 on the classic ESP32 (esp32dev / WROOM modules) are
// reserved for SPI flash (SD2/SD3/CMD/CLK/SD0/SD1 — pins 6-11) and
// FastLED's `FASTLED_UNUSABLE_PIN_MASK` for esp32dev marks pins
// {6, 7, 8, 9, 10, 11, 20} as `validpin() == false`. Instantiating any
// clockless driver bound to those pins fails the FastLED static_assert
// at compile time. Other ESP32 variants (S2/S3/C-series, P4) repurpose
// GPIO6-8 and accept them as valid output pins, and every non-ESP
// family treats them as normal digital pins. The classic-ESP32
// sentinel is `FL_IS_ESP_32DEV` (defined in `src/platforms/esp/is_esp.h`);
// the prior #3447 attempt used `FL_IS_ESP_32` which is not a real macro,
// so the gate never fired and esp32dev compile still hit the
// static_assert. LegacyClocklessProxy iterates pins 0-8 so all three
// of 6/7/8 must be excluded for the classic ESP32 build.
#if defined(FL_IS_ESP_32DEV)
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_6 0
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_7 0
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_8 0
#else
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_6 1
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_7 1
#define AUTORESEARCH_LEGACY_SUPPORTS_PIN_8 1
#endif

inline const char* legacyClocklessChipsetName(LegacyClocklessChipset chipset) {
    switch (chipset) {
        case LegacyClocklessChipset::WS2812B:
            return "WS2812B";
        case LegacyClocklessChipset::SK6812:
            return "SK6812";
    }
    return "UNKNOWN";
}

inline bool legacyClocklessChipsetFromName(const fl::string& name,
                                           LegacyClocklessChipset* out) {
    if (name == "WS2812B" || name == "WS2812") {
        if (out) *out = LegacyClocklessChipset::WS2812B;
        return true;
    }
    if (name == "SK6812") {
        if (out) *out = LegacyClocklessChipset::SK6812;
        return true;
    }
    return false;
}

/// @brief Proxy that creates a legacy clockless controller from runtime inputs.
///
/// The legacy FastLED API requires compile-time template pin parameters:
///   FastLED.addLeds<WS2812B, PIN>(leds, numLeds)
///
/// This proxy uses a switch statement to dispatch supported AutoResearch legacy
/// pin and chipset values to the corresponding template instantiation, enabling
/// testing of the full legacy code path. Pins 0-8 are historical coverage pins.
/// On Teensy 4.x, pin 22 is also included for the current ObjectFLED loopback
/// wiring. Other platforms may mark pin 22 invalid at compile time, so avoid
/// instantiating that template there.
///
/// Destructor deletes the controller, which on ESP32 (SKETCH_HAS_LARGE_MEMORY)
/// automatically calls removeFromDrawList() via ~CLEDController().
class LegacyClocklessProxy {
    fl::unique_ptr<CLEDController> mController;

    template<typename Controller>
    fl::unique_ptr<CLEDController> createTyped(CRGB* leds, int numLeds,
                                               bool rgbw) {
        auto c = fl::make_unique<Controller>();
        FastLED.addLeds(c.get(), leds, numLeds);
        if (rgbw) {
            c->setRgbw(RgbwDefault());
        }
        return c;
    }

    template<int PIN>
    fl::unique_ptr<CLEDController> create(CRGB* leds, int numLeds,
                                          LegacyClocklessChipset chipset,
                                          bool rgbw) {
        switch (chipset) {
            case LegacyClocklessChipset::WS2812B:
                return createTyped<WS2812B<PIN, RGB>>(leds, numLeds, rgbw);
            case LegacyClocklessChipset::SK6812:
                return createTyped<SK6812<PIN, RGB>>(leds, numLeds, rgbw);
        }
        return nullptr;
    }

public:
    LegacyClocklessProxy(
        int pin, CRGB* leds, int numLeds,
        LegacyClocklessChipset chipset = LegacyClocklessChipset::WS2812B,
        bool rgbw = false) {
        switch (pin) {
            case 0: mController = create<0>(leds, numLeds, chipset, rgbw); break;
            case 1: mController = create<1>(leds, numLeds, chipset, rgbw); break;
            case 2: mController = create<2>(leds, numLeds, chipset, rgbw); break;
            case 3: mController = create<3>(leds, numLeds, chipset, rgbw); break;
            case 4: mController = create<4>(leds, numLeds, chipset, rgbw); break;
            case 5: mController = create<5>(leds, numLeds, chipset, rgbw); break;
#if AUTORESEARCH_LEGACY_SUPPORTS_PIN_6
            case 6: mController = create<6>(leds, numLeds, chipset, rgbw); break;
#endif
#if AUTORESEARCH_LEGACY_SUPPORTS_PIN_7
            case 7: mController = create<7>(leds, numLeds, chipset, rgbw); break;
#endif
#if AUTORESEARCH_LEGACY_SUPPORTS_PIN_8
            case 8: mController = create<8>(leds, numLeds, chipset, rgbw); break;
#endif
#if AUTORESEARCH_LEGACY_SUPPORTS_PIN_22
            case 22: mController = create<22>(leds, numLeds, chipset, rgbw); break;
#endif
            default: break;  // mController stays nullptr
        }
    }

    ~LegacyClocklessProxy() = default;

    // Non-copyable (unique_ptr already enforces this, but be explicit)
    LegacyClocklessProxy(const LegacyClocklessProxy&) = delete;
    LegacyClocklessProxy& operator=(const LegacyClocklessProxy&) = delete;

    /// @brief Check if the proxy was successfully created
    bool valid() const { return mController != nullptr; }
};
