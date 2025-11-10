# FastLED Platform: Teensy 4.x (i.MX RT1062)

Teensy 4.0/4.1 (IMXRT1062) support.

## Files
- `fastled_arm_mxrt1062.h`: Aggregator; includes pin/SPI/clockless and helpers.
- `fastpin_arm_mxrt1062.h`: Pin helpers.
- `fastspi_arm_mxrt1062.h`: SPI backend.
- `clockless_arm_mxrt1062.h`: Single-lane clockless driver.
- `block_clockless_arm_mxrt1062.h`: Block/multi-lane clockless.
- `octows2811_controller.h`: OctoWS2811 integration.
- `clockless_objectfled.h/cpp`: Legacy single-controller ObjectFLED wrapper.
- `led_sysdefs_arm_mxrt1062.h`: System defines for RT1062.

## Multi-Strip LED Control

The ObjectFLED driver provides high-performance multi-strip LED control using DMA-driven bit transposition on Teensy 4.0/4.1.

### Features
- **Up to 42 parallel strips** (Teensy 4.1) or 16 strips (Teensy 4.0)
- **Automatic chipset timing** - WS2812, SK6812, WS2811, WS2813, WS2815 support
- **DMA-driven** - Minimal CPU overhead during transmission

### Usage
Use the Channel API for new projects. See `fl/channels/channel.h` for documentation.

### Performance
- 100 LEDs: ~3ms transmission time
- 200 LEDs: ~6ms transmission time
- Achieves 60fps easily with 500+ LEDs per instance

## Notes
- Very high CPU frequency; DWT-based timing and interrupt thresholds are critical for stability.
- OctoWS2811 and SmartMatrix can offload large parallel outputs; ensure pin mappings and DMA settings match board wiring.
- For multi-strip LED projects on Teensy 4.x, see the Channel API documentation.
