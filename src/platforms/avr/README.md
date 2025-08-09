# FastLED Platform: AVR

## Optional feature defines

- `FASTLED_ALLOW_INTERRUPTS` (0|1): Allow brief interrupts during show; default 0. Increase only if ISRs are short. Pairs with `INTERRUPT_THRESHOLD` (cycles, default 2).
- `FASTLED_USE_PROGMEM` (0|1): Enable PROGMEM storage; default 1.
- `FASTLED_FORCE_SOFTWARE_PINS` (defined): Force software pin access instead of hardware-optimized access in `fastpin_avr*`.
- `FASTLED_FORCE_SOFTWARE_SPI` (defined): Force software SPI instead of hardware SPI.
- `NO_CORRECTION` (defined): Disable color correction on constrained boards (e.g., Digispark) if needed.
- `FASTLED_SLOW_CLOCK_ADJUST` (defined): Timing nudge for 8 MHz builds (set automatically in some controllers).
