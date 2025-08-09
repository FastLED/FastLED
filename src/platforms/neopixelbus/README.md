# FastLED Platform: neopixelbus

NeoPixelBus integration providing a FastLED‑compatible clockless controller implemented on top of the NeoPixelBus library.

## Files (quick pass)
- `clockless.h`: Adapter layer. Provides `ClocklessController` and `NeoPixelBusLikeClocklessT` templates that translate FastLED pixel data to NeoPixelBus (`Begin/Show`, method selection per platform, color feature mapping). Includes an RGBW variant (`NeoPixelBusRGBWController`).

## Method and color selection
- Method auto‑selection: `NeoPixelBusMethodSelector<DATA_PIN>::DefaultMethod` picks a default per platform (e.g., `NeoEsp32Rmt0800KbpsMethod` on ESP32, `NeoAvr800KbpsMethod` on AVR, etc.).
- Color feature mapping: `NeoPixelBusColorFeature<RGB_ORDER>` maps FastLED’s `EOrder` to NeoPixelBus features (`NeoGrbFeature`, `NeoRgbFeature`, ...).

## Behavior and constraints
- Recreates the bus if pixel count changes (to match FastLED buffer size).
- `CanShow()` checked before sending; avoids overlapping updates.
- RGBW variant extracts a basic white channel (min(R,G,B)) before sending.

## Notes
- Good option when you want NeoPixelBus’s platform backends or advanced features, while keeping FastLED’s API on top.

## Defaults per platform

Method selection examples (subject to NeoPixelBus version/platform):

- ESP32: `NeoEsp32Rmt0800KbpsMethod` for GRB @ 800 Kbps (WS2812‑class)
- ESP8266: `NeoEsp8266BitBang800KbpsMethod` or UART‑based variants depending on pin
- AVR: `NeoAvr800KbpsMethod`

RGB order mapping:

- FastLED `RGB` → NeoPixelBus `NeoRgbFeature`
- FastLED `GRB` → NeoPixelBus `NeoGrbFeature`
- FastLED `RGBW` (emulated white) → NeoPixelBus `NeoGrbwFeature` path in the RGBW controller

Note: Actual defaults may change with NeoPixelBus releases; `NeoPixelBusMethodSelector<DATA_PIN>::DefaultMethod` is the source of truth at compile time.

## Optional feature defines

- **`FASTLED_USE_NEOPIXEL_BUS`**: Default `0` (unless building docs). When `1`, enables the NeoPixelBus adapter; requires `NeoPixelBus.h`. If the header is missing, the adapter disables itself with an error.

Define before including `FastLED.h`.
