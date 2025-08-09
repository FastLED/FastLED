# FastLED Platform: Nordic nRF52

## Optional feature defines

- `FASTLED_FORCE_SOFTWARE_SPI` (defined): Force software SPI (disables SPIM usage).
- `FASTLED_NRF52_SPIM` (NRF_SPIMx): Select SPIM instance (e.g., `NRF_SPIM0`).
- `FASTLED_NRF52_ENABLE_PWM_INSTANCE{0..3}` (defined): Enable one or more PWM instances used by the clockless engine.
- `FASTLED_NRF52_NEVER_INLINE` (defined): Adjust inlining policy for performance/size.
- `FASTLED_NRF52_DEBUGPRINT(...)` (macro): Hook for debug printing.
- `FASTLED_ALLOW_INTERRUPTS` (0|1): Default 1.
- `FASTLED_USE_PROGMEM` (0|1): Default 0.
