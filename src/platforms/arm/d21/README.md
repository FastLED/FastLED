# FastLED Platform: ARM SAMD21 (d21)

SAMD21 (Arduino Zero-class) support.

## Files (quick pass)
- `fastled_arm_d21.h`: Aggregator for SAMD21; includes pin and clockless headers.
- `fastpin_arm_d21.h`: Pin helpers for direct GPIO access on SAMD21.
- `led_sysdefs_arm_d21.h`: SAMD21 system defines (interrupt policy, PROGMEM policy, etc.).
- `clockless_arm_d21.h`: Legacy `ClocklessController` routed through `fl::SlimBridgeController` (#4593). Pixels are encoded into a `ChannelData` buffer and sent by the per-pin `ClocklessSamd21Driver` (`IChannelDriver`), which drives the cycle-accurate `arm/common/m0clockless.h` core with identity scale/dither.
- `init_channel_driver_samd21.cpp.hpp`: Registers the SERCOM SPI adapter. The clockless driver self-registers via `ChannelManager::registry()` only when a clockless controller is instantiated, so sketches without clockless strips do not link it.

Notes:
- Clockless timing uses the M0/M0+ delay macros; ensure `F_CPU` and interrupt settings match board configuration.

### Compile-time expectations
- `FASTLED_USE_PROGMEM`: 0 (per ARM SAMD guidance)
- `FASTLED_ALLOW_INTERRUPTS`: driver tolerates short windows; long ISRs may force a retry/punt

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK` when `1`.

Define before including `FastLED.h`.
