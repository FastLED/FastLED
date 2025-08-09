# FastLED Platforms: ARM family

This folder contains ARM-based targets (Teensy, SAMD21/D51, RP2040, STM32, nRF5x, Renesas UNO R4, GIGA, etc.). See each subfolder for specifics.

## Common optional feature defines

- `FASTLED_ALLOW_INTERRUPTS` (0|1): Allow brief interrupts during show. Default varies by subplatform; enabling requires short ISRs.
- `INTERRUPT_THRESHOLD` (cycles/us): Grace window for ISR latency where supported.
- `FASTLED_USE_PROGMEM` (0|1): PROGMEM usage policy. Typically 0 on ARM; some Teensy variants use 1.
- `FASTLED_FORCE_SOFTWARE_PINS` (defined): Force software-driven pin writes (disables fast GPIO).
- `FASTLED_FORCE_SOFTWARE_SPI` (defined): Force software SPI instead of hardware peripherals.

Refer to each subfolder README for additional knobs (PIO/DMA, timers, pinmaps).
