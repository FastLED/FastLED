// LegacyClocklessProxy.h - Runtime-to-template pin dispatch for legacy addLeds API
//
// Maps a runtime pin number (0-8) to compile-time WS2812B<PIN, RGB> template
// instantiations via a switch statement. This allows validation testing of the
// legacy template API path: WS2812B<PIN> -> WS2812Controller800Khz ->
// ClocklessControllerImpl -> ClocklessIdf5 -> Channel

#pragma once

#include <FastLED.h>
#include "fl/stl/unique_ptr.h"

/// @brief Proxy that creates a legacy WS2812B controller from a runtime pin number.
///
/// The legacy FastLED API requires compile-time template pin parameters:
///   FastLED.addLeds<WS2812B, PIN>(leds, numLeds)
///
/// This proxy uses a switch statement to dispatch runtime pin values (0-8) to
/// the corresponding template instantiation, enabling validation testing of the
/// full legacy code path.
///
/// Destructor deletes the controller, which on ESP32 (SKETCH_HAS_LOTS_OF_MEMORY)
/// automatically calls removeFromDrawList() via ~CLEDController().
class LegacyClocklessProxy {
    fl::unique_ptr<CLEDController> mController;

    template<int PIN>
    fl::unique_ptr<CLEDController> create(CRGB* leds, int numLeds) {
        auto c = fl::make_unique<WS2812B<PIN, RGB>>();
        FastLED.addLeds(c.get(), leds, numLeds);
        return c;
    }

public:
    LegacyClocklessProxy(int pin, CRGB* leds, int numLeds) {
        switch (pin) {
            case 0: mController = create<0>(leds, numLeds); break;
            case 1: mController = create<1>(leds, numLeds); break;
            case 2: mController = create<2>(leds, numLeds); break;
            case 3: mController = create<3>(leds, numLeds); break;
            case 4: mController = create<4>(leds, numLeds); break;
            case 5: mController = create<5>(leds, numLeds); break;
            case 6: mController = create<6>(leds, numLeds); break;
            case 7: mController = create<7>(leds, numLeds); break;
            case 8: mController = create<8>(leds, numLeds); break;
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
