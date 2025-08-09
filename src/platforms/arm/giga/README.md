# FastLED Platform: Arduino GIGA (STM32H747)

Support for Arduino GIGA R1 based on STM32H747.

## Files (quick pass)
- `fastled_arm_giga.h`: Aggregator; includes `fastpin_arm_giga.h`, `clockless_arm_giga.h`.
- `fastpin_arm_giga.h`: Pin helpers for GIGA.
- `led_sysdef_arm_giga.h`: System defines (interrupts, PROGMEM policy, F_CPU, cli/sei aliases).
- `clockless_arm_giga.h`: Clockless WS281x driver for GIGA.
- `armpin.h`: ARM-style pin template utilities.

Notes:
- Uses ARM irq enable/disable wrappers for timing-critical sections.
 - `led_sysdef_arm_giga.h` sets `FASTLED_USE_PROGMEM=0`, enables interrupts, and defines `F_CPU` (e.g., 480MHz) for timing math.
