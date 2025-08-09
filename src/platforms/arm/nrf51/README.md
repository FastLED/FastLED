# FastLED Platform: nRF51

Nordic nRF51 family support.

## Files (quick pass)
- `fastled_arm_nrf51.h`: Aggregator; includes pin/SPI/clockless.
- `fastpin_arm_nrf51.h`: Pin helpers.
- `fastspi_arm_nrf51.h`: SPI backend.
- `clockless_arm_nrf51.h`: Clockless driver for nRF51.
- `led_sysdefs_arm_nrf51.h`: System defines for nRF51.

Notes:
- Lower-clock Cortex-M0; carefully budget interrupt windows when using clockless.
 - Prefer fewer parallel lanes or shorter strips with clockless to maintain timing margins.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`.
- **`FASTLED_ALL_PINS_HARDWARE_SPI`**: Enabled by default when not forcing software SPI.

Define before including `FastLED.h`.
