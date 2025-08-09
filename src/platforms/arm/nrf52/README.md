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
