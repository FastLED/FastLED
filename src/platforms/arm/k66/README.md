# FastLED Platform: Teensy 3.6 (K66)

Teensy 3.6 (MK66FX1M0) support.

## Files (quick pass)
- `fastled_arm_k66.h`: Aggregator; includes pin/SPI/clockless and reuses some K20 helpers.
- `fastpin_arm_k66.h`: Pin helpers.
- `fastspi_arm_k66.h`: SPI output backend.
- `clockless_arm_k66.h`: Single-lane clockless driver (DWT counter).
- `clockless_block_arm_k66.h`: Block/multi-lane variant.
- `led_sysdefs_arm_k66.h`: System defines for K66.

Notes:
- Similar to K20 timing model with higher clocks; observe interrupt thresholds for consistent output.
 - DWT cycle counter is used by clockless; ensure it is enabled early for precise timing.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `1` on some Teensy3 cores.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK`.

Define before including `FastLED.h`.
