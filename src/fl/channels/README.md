# FastLED Channels API

## Overview

The Channels API provides a modern, platform-independent interface for driving multiple LED strips in parallel with hardware-accelerated timing. It abstracts the underlying DMA and parallel I/O hardware, making it easier to write portable code that works across different microcontroller platforms.

## Architecture

The Channels API consists of four main components:

1. **IChannel** - Individual LED strip controller interface
2. **IChannelGroup** - Coordinator for multiple channels sharing the same chipset timing
3. **IChannelEngine** - Platform-specific DMA/hardware engine (e.g., ESP32-P4 PARLIO)
4. **IChannelManager** - Central factory and coordinator for all channels

## Key Concepts

### Channel Configuration

Each channel is configured with:
- **Chipset timing** - T1/T2/T3 timings for WS2812, SK6812, etc.
- **LED data buffer** - Array of CRGB pixel data
- **RGBW settings** - Optional RGBW conversion
- **Color correction** - Color temperature and LED strip corrections
- **Dithering** - Temporal dithering for smooth color transitions

### Multi-Channel Configuration

The `MultiChannelConfig` helper allows applying settings to multiple channels at once:

```cpp
MultiChannelConfig config;
config.add(channel1_config)
      .add(channel2_config)
      .setCorrection(TypicalLEDStrip)
      .setTemperature(Tungsten100W)
      .setDither(BINARY_DITHER);
```

## Minimal Example: 4 Parallel LED Strips

```cpp
#include "FastLED.h"

#define NUM_LEDS 60

// LED data arrays for each strip
CRGB strip1[NUM_LEDS];
CRGB strip2[NUM_LEDS];
CRGB strip3[NUM_LEDS];
CRGB strip4[NUM_LEDS];

fl::vector<IChannelPtr> channels;

// ParlioEngine is a platform-specific ChannelEngine for ESP32-P4.


void setup() {
    // Create timing configuration for WS2812
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();

    // Create individual channel configurations
    fl::ChannelConfig config1(timing, fl::span<const CRGB>(strip1, NUM_LEDS));
    fl::ChannelConfig config2(timing, fl::span<const CRGB>(strip2, NUM_LEDS));
    fl::ChannelConfig config3(timing, fl::span<const CRGB>(strip3, NUM_LEDS));
    fl::ChannelConfig config4(timing, fl::span<const CRGB>(strip4, NUM_LEDS));

    // Optional: Apply settings to all channels
    fl::MultiChannelConfig multiConfig({
        fl::make_shared<fl::ChannelConfig>(config1),
        fl::make_shared<fl::ChannelConfig>(config2),
        fl::make_shared<fl::ChannelConfig>(config3),
        fl::make_shared<fl::ChannelConfig>(config4)
    });

    multiConfig.setCorrection(TypicalLEDStrip)
               .setTemperature(Tungsten100W);

    FastLED.addLeds<ParlioEngine>(multiConfig, &channels /*optional*/);
}

void loop() {
    // Set different colors on each strip
    fill_solid(strip1, NUM_LEDS, CRGB::Red);
    fill_solid(strip2, NUM_LEDS, CRGB::Green);
    fill_solid(strip3, NUM_LEDS, CRGB::Blue);
    fill_solid(strip4, NUM_LEDS, CRGB::Yellow);

    // Show updates all channels
    FastLED.show();
    delay(1000);

    // Rainbow effect across all strips
    static uint8_t hue = 0;
    for(int i = 0; i < NUM_LEDS; i++) {
        strip1[i] = CHSV(hue + (i * 4), 255, 255);
        strip2[i] = CHSV(hue + (i * 4) + 64, 255, 255);
        strip3[i] = CHSV(hue + (i * 4) + 128, 255, 255);
        strip4[i] = CHSV(hue + (i * 4) + 192, 255, 255);
    }
    hue++;

    FastLED.show();  // Show updates all channels
    delay(20);
}
```


### "Chipset timing mismatch"

All strips in a channel group must use the same chipset timing. If you need to mix chipsets (e.g., WS2812 + SK6812), create separate channel groups with different timing configurations.

## API Status

**Current Status**: Experimental / Under Development

The Channels API is actively being developed. Interface stability is not guaranteed. For production code, consider using the traditional `FastLED.addLeds()` API until this interface stabilizes.

## See Also

- `channel.h` - IChannel interface definition
- `channel_manager.h` - IChannelManager factory interface
- `channel_config.h` - Configuration structures
- `channel_engine.h` - Platform engine interface
- `chipset_timing_config.h` - Chipset timing definitions
- `examples/BlinkParallel.ino` - Traditional parallel LED example
