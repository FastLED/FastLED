# FastLED Platform: Teensy LC (KL26)

Teensy LC (MKL26Z64) support.

## Files (quick pass)
- `fastled_arm_kl26.h`: Aggregator; includes pin/SPI/clockless.
- `fastpin_arm_kl26.h`: Pin helpers.
- `fastspi_arm_kl26.h`: SPI output backend.
- `clockless_arm_kl26.h`: Clockless driver for LC.
- `ws2812serial_controller.h` (via k20 include): Serial WS2812 controller reuse.
- `led_sysdefs_arm_kl26.h`: System defines for KL26.

Notes:
- LC has tighter timing/memory limits; keep critical sections minimal.
 - Verify `FASTLED_USE_PROGMEM` and interrupt policy per toolchain; defaults may differ between cores.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `1` on LC cores.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK`.
- **`FASTLED_SPI_BYTE_ONLY`**: Present in sysdefs indicating SPI byte granularity optimization.

Define before including `FastLED.h`.
