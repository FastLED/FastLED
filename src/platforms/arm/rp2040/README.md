# FastLED Platform: RP2040

Raspberry Pi Pico (RP2040) support.

## Files (quick pass)
- `fastled_arm_rp2040.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_rp2040.h`: Pin helpers.
- `clockless_arm_rp2040.h`: Clockless driver using PIO.
- `pio_asm.h`, `pio_gen.h`: PIO assembly and program generator for T1/T2/T3‑tuned clockless output.
- `led_sysdefs_arm_rp2040.h`: System defines for RP2040.

Notes:
- Uses PIO program assembled at runtime; ensure T1/T2/T3 match LED timing.
 - `clockless_arm_rp2040.h` configures wrap targets and delays via `pio_gen.h`; changes to timing require regenerating the program.

## Optional feature defines

- **`FASTLED_ALLOW_INTERRUPTS`**: Allow ISRs during show. Default `1`.
- **`FASTLED_ACCURATE_CLOCK`**: Enabled when interrupts are allowed to maintain timing math accuracy.
- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model).
- **Clockless driver selection/tuning**
  - **`FASTLED_RP2040_CLOCKLESS_PIO`**: Use PIO engine for clockless. Default `1`.
  - **`FASTLED_RP2040_CLOCKLESS_IRQ_SHARED`**: Share IRQ usage between PIO and other subsystems. Default `1`.
  - **`FASTLED_RP2040_CLOCKLESS_M0_FALLBACK`**: Fallback to a Cortex‑M0 timing loop if PIO is disabled/unavailable. Default `0`.

Define these before including `FastLED.h` in your sketch.
