# FastLED Platform: Teensy 4.x (i.MX RT1062)

Teensy 4.0/4.1 (IMXRT1062) support.

## Files (quick pass)
- `fastled_arm_mxrt1062.h`: Aggregator; includes pin/SPI/clockless and helpers.
- `fastpin_arm_mxrt1062.h`: Pin helpers.
- `fastspi_arm_mxrt1062.h`: SPI backend.
- `clockless_arm_mxrt1062.h`: Single-lane clockless driver.
- `block_clockless_arm_mxrt1062.h`: Block/multi-lane clockless.
- `octows2811_controller.h`: OctoWS2811 integration.
- `led_sysdefs_arm_mxrt1062.h`: System defines for RT1062.

Notes:
- Very high CPU frequency; DWT-based timing and interrupt thresholds are critical for stability.
 - OctoWS2811 and SmartMatrix can offload large parallel outputs; ensure pin mappings and DMA settings match board wiring.
