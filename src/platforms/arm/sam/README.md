# FastLED Platform: SAM (Arduino Due)

Atmel/Microchip SAM3X (Arduino Due) support.

## Files (quick pass)
- `fastled_arm_sam.h`: Aggregator; includes pin/SPI and clockless.
- `fastpin_arm_sam.h`: Pin helpers.
- `fastspi_arm_sam.h`: SPI backend.
- `clockless_arm_sam.h`: Clockless driver.
- `clockless_block_arm_sam.h`: Block/multi-lane clockless driver.
- `led_sysdefs_arm_sam.h`: System defines for SAM.

Notes:
- Older Cortex-M3; ensure interrupt windows are respected to avoid jitter.
 - `FASTLED_USE_PROGMEM=0` recommended; long-running ISRs may require reducing strip length or frame rate.
