# FastLED Platform: RP2040 (Raspberry Pi Pico)

## Optional feature defines

- `FASTLED_RP2040_CLOCKLESS_PIO` (0|1): Use PIO+DMA non-blocking clockless output. Default 1.
- `FASTLED_RP2040_CLOCKLESS_IRQ_SHARED` (0|1): Install shared IRQ handler for DMA IRQ0. Default 1.
- `FASTLED_RP2040_CLOCKLESS_M0_FALLBACK` (0|1): Enable M0 blocking fallback when PIO unavailable. Default 0.
- `FASTLED_FORCE_SOFTWARE_SPI` (defined): Use software SPI instead of hardware.
- `FASTLED_FORCE_SOFTWARE_PINS` (defined): Force software pin access.
- `FASTLED_ALLOW_INTERRUPTS` (0|1): Default 1.
- `FASTLED_USE_PROGMEM` (0|1): Default 0.
