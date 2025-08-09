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
