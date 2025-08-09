# FastLED Platform: nRF52

Nordic nRF52 family support.

## Files (quick pass)
- `fastled_arm_nrf52.h`: Aggregator; includes pin/SPI/clockless and sysdefs.
- `fastpin_arm_nrf52.h`, `fastpin_arm_nrf52_variants.h`: Pin helpers/variants.
- `fastspi_arm_nrf52.h`: SPI backend.
- `clockless_arm_nrf52.h`: Clockless driver.
- `arbiter_nrf52.h`: PWM arbitration utility (selects/guards PWM instances for drivers).
- `led_sysdefs_arm_nrf52.h`: System defines for nRF52.

Notes:
- Requires `CLOCKLESS_FREQUENCY` definition in many setups; PWM resources may be shared and must be arbitrated.
 - `arbiter_nrf52.h` exposes a small API to acquire/release PWM instances safely across users; ensure ISR handlers are lightweight.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model).
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`.
- **`FASTLED_ALL_PINS_HARDWARE_SPI`**: Enabled by default unless forcing software SPI.
- **`FASTLED_NRF52_SPIM`**: Select SPIM instance (e.g., `NRF_SPIM0`).
- **`FASTLED_NRF52_ENABLE_PWM_INSTANCE0`**: Enable PWM instance used by clockless.
- **`FASTLED_NRF52_NEVER_INLINE`**: Controls inlining attribute via `FASTLED_NRF52_INLINE_ATTRIBUTE`.
- **`FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING`**: Limit pixels per string in PWM-encoded path (default 144).
- **`FASTLED_NRF52_PWM_ID`**: Select PWM instance index used.
- **`FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING`**: Suppress pin-map warnings for unverified boards.

Define before including `FastLED.h`.
