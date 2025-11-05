# FastLED Platform: Teensy 4.x (i.MX RT1062)

Teensy 4.0/4.1 (IMXRT1062) support.

## Files
- `fastled_arm_mxrt1062.h`: Aggregator; includes pin/SPI/clockless and helpers.
- `fastpin_arm_mxrt1062.h`: Pin helpers.
- `fastspi_arm_mxrt1062.h`: SPI backend.
- `clockless_arm_mxrt1062.h`: Single-lane clockless driver.
- `block_clockless_arm_mxrt1062.h`: Block/multi-lane clockless.
- `octows2811_controller.h`: OctoWS2811 integration.
- `clockless_objectfled.h/cpp`: Legacy single-controller ObjectFLED wrapper (prefer bulk_objectfled.h for new code)
- `bulk_objectfled.h`: **NEW** - BulkClockless specialization for ObjectFLED (multi-strip DMA driver).
- `led_sysdefs_arm_mxrt1062.h`: System defines for RT1062.

## BulkClockless ObjectFLED Driver (NEW)

The `bulk_objectfled.h` driver provides high-performance multi-strip LED control using DMA-driven bit transposition on Teensy 4.0/4.1.

### Features
- **Up to 42 parallel strips** (Teensy 4.1) or 16 strips (Teensy 4.0)
- **Automatic chipset timing** - WS2812, SK6812, WS2811, WS2813, WS2815 support
- **Per-strip settings** - Independent color correction, temperature, dither per strip
- **RGBW support** - SK6812 RGBW with automatic white channel extraction
- **Multiple instances** - Different chipsets per instance (serial execution)
- **Dynamic add/remove** - Add or remove strips at runtime
- **DMA-driven** - Minimal CPU overhead during transmission

### Usage
```cpp
#include <FastLED.h>

#define NUM_LEDS 100
CRGB strip1[NUM_LEDS];
CRGB strip2[NUM_LEDS];
CRGB strip3[NUM_LEDS];

void setup() {
    auto& bulk = FastLED.addBulkLeds<WS2812_CHIPSET, ObjectFLED>({
        {2, strip1, NUM_LEDS, ScreenMap()},
        {5, strip2, NUM_LEDS, ScreenMap()},
        {9, strip3, NUM_LEDS, ScreenMap()}
    });

    // Optional: Per-strip color correction
    bulk.get(2)->setCorrection(TypicalLEDStrip);
    bulk.get(5)->setCorrection(TypicalSMD5050);
}

void loop() {
    fill_rainbow(strip1, NUM_LEDS, 0, 255 / NUM_LEDS);
    FastLED.show();
}
```

### Features
1. **✅ Mixed-length strips** - Strips can have different LED counts! Shorter strips are padded with black automatically during DMA transposition
2. **Serial execution** - Multiple instances transmit serially (not parallel)
3. **Clockless only** - Does not support SPI chipsets (APA102, SK9822)

See `examples/SpecialDrivers/Teensy/ObjectFLED/TeensyMassiveParallel/` for an example.

### Performance
- 100 LEDs: ~3ms transmission time
- 200 LEDs: ~6ms transmission time
- Multiple instances: Sum of individual transmission times
- Achieves 60fps easily with 500+ LEDs per instance

## Notes
- Very high CPU frequency; DWT-based timing and interrupt thresholds are critical for stability.
- OctoWS2811 and SmartMatrix can offload large parallel outputs; ensure pin mappings and DMA settings match board wiring.
- **BulkClockless<CHIPSET, OFLED>** is the recommended driver for multi-strip LED projects on Teensy 4.x (supports mixed-length strips!).
