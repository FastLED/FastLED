# FastLED Platform: STM32

STM32 family support (e.g., F1 series).

## Files (quick pass)
- `fastled_arm_stm32.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_stm32.h`, `fastpin_arm_stm_legacy.h`, `fastpin_arm_stm_new.h`: Pin helpers/variants.
- `clockless_arm_stm32.h`: Clockless driver for STM32.
- `armpin.h`: Generic ARM pin template utilities.
- `cm3_regs.h`: CM3 register helpers.
- `led_sysdefs_arm_stm32.h`: System defines for STM32.

Notes:
- Some families prefer different pin backends (legacy/new); choose the appropriate header for your core.
 - Many STM32 Arduino cores map `pinMode` to HAL; direct register variants in `fastpin_*` may differ across series.

## Optional feature defines

- **`FASTLED_ALLOW_INTERRUPTS`**: Default `0`. Clockless timing on STM32 typically runs with interrupts off for stability.
- **`FASTLED_ACCURATE_CLOCK`**: Enabled automatically when interrupts are allowed to keep timing math correct.
- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model / memory‑mapped flash).
- **`FASTLED_NO_PINMAP`**: Defined to indicate no PROGMEM‑backed pin maps are used on this platform.
- **`FASTLED_NEEDS_YIELD`**: Defined to signal scheduling/yield points might be required in some cores.

Place defines before including `FastLED.h`.
