# FastLED Platform: ARM SAMD51 (d51)

SAMD51 (Feather/Itsy M4, Wio Terminal) support.

## Files (quick pass)
- `fastled_arm_d51.h`: Aggregator; includes `fastpin_arm_d51.h`, `clockless_arm_d51.h`, and SPI core where applicable.
- `fastpin_arm_d51.h`: Pin helpers for SAMD51.
- `led_sysdefs_arm_d51.h`: System defines for SAMD51.
- `clockless_arm_d51.h`: Clockless WS281x driver for SAMD51.
- `README.txt`: Historical notes on tested boards.

Notes:
- Higher clock speeds and interrupt policy can affect jitter; prefer short critical sections.
 - Typical settings: `FASTLED_USE_PROGMEM=0`; consider enabling interrupts with careful ISR timing.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK` when `1`.

Define before including `FastLED.h`.
