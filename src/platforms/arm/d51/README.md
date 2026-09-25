# FastLED Platform: ARM SAMD51 (d51)

SAMD51 (Feather/Itsy M4, Wio Terminal) support.

## Files (quick pass)
- `fastled_arm_d51.h`: Aggregator; includes `fastpin_arm_d51.h`, `clockless_arm_d51.h`, and SPI core where applicable.
- `fastpin_arm_d51.h`: Pin helpers for SAMD51.
- `led_sysdefs_arm_d51.h`: System defines for SAMD51.
- `clockless_arm_d51.h`: Legacy `ClocklessController` routed through `fl::SlimBridgeController` (#4593). Pixels are encoded into a `ChannelData` buffer and sent by the per-pin `ClocklessSamd51Driver` (`IChannelDriver`), which keeps the DWT cycle-counter bit loop.
- `init_channel_driver_samd51.cpp.hpp`: Registers the SERCOM SPI adapter (not yet used by `addLeds<APA102>`, which stays on the legacy `SPIOutput` path). The clockless driver self-registers via `ChannelManager::registry()` only when a clockless controller is instantiated, so sketches without clockless strips do not link it.
- `README.txt`: Historical notes on tested boards.

Notes:
- Higher clock speeds and interrupt policy can affect jitter; prefer short critical sections.
 - Typical settings: `FASTLED_USE_PROGMEM=0`; consider enabling interrupts with careful ISR timing.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK` when `1`.

Define before including `FastLED.h`.
