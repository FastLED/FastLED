# FastLED Platform: Renesas (UNO R4)

Renesas RA4M1 (Arduino UNO R4) support.

## Files (quick pass)
- `fastled_arm_renesas.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_renesas.h`: Pin helpers.
- `clockless_arm_renesas.h`: Clockless driver.
- `led_sysdef_arm_renesas.h`: System defines for Renesas.

Notes:
- Interrupt policy and F_CPU definitions affect timing; keep critical sections tight.
 - Check UNO R4 core defines to ensure `FASTLED_USE_PROGMEM=0` and interrupt macros map to platform equivalents.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK` when interrupts are allowed.
- **`FASTLED_NO_PINMAP`**: Indicates no PROGMEM pin maps are used.

Place defines before including `FastLED.h`.
