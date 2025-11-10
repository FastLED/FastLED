# FastLED Channels API

## Overview

The Channels API provides a modern, platform-independent interface for driving multiple LED strips in parallel with hardware-accelerated timing. It abstracts the underlying DMA and parallel I/O hardware, making it easier to write portable code that works across different microcontroller platforms.

## Architecture

The Channels API consists of two main components:

1. **Channel** - Individual LED strip controller with factory methods
2. **ChannelEngine** - Platform-specific DMA/hardware engine (e.g., ESP32-P4 PARLIO)

## Key Concepts

### Channel Configuration

Each channel is configured with:
- **Pin number** - GPIO pin for the LED strip
- **Chipset timing** - T1/T2/T3 timings for WS2812, SK6812, etc.
- **LED data buffer** - Array of CRGB pixel data (as fl::span<CRGB>)
- **RGB order** - Color channel ordering (RGB, GRB, BRG, etc.)
- **LED settings** - Grouped settings including:
  - **Color correction** - Color temperature and LED strip corrections
  - **Dithering** - Temporal dithering for smooth color transitions
  - **RGBW settings** - Optional RGBW conversion

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
#define PIN1 16
#define PIN2 17
#define PIN3 18
#define PIN4 19

// LED data arrays for each strip
CRGB strip1[NUM_LEDS];
CRGB strip2[NUM_LEDS];
CRGB strip3[NUM_LEDS];
CRGB strip4[NUM_LEDS];

void setup() {
    // Create timing configuration for WS2812
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();

    // Optional: Configure LED settings (defaults shown)
    fl::LEDSettings settings;
    settings.correction = UncorrectedColor;
    settings.temperature = UncorrectedTemperature;
    settings.ditherMode = BINARY_DITHER;
    settings.rgbw = RgbwInvalid::value();

    // Create individual channel configurations
    fl::ChannelConfig config1(PIN1, timing, fl::span<CRGB>(strip1, NUM_LEDS), RGB, settings);
    fl::ChannelConfig config2(PIN2, timing, fl::span<CRGB>(strip2, NUM_LEDS), RGB, settings);
    fl::ChannelConfig config3(PIN3, timing, fl::span<CRGB>(strip3, NUM_LEDS), RGB, settings);
    fl::ChannelConfig config4(PIN4, timing, fl::span<CRGB>(strip4, NUM_LEDS), RGB, settings);

    // Create channels using platform-specific engine (e.g., ParlioEngine for ESP32-P4)
    auto channel1 = fl::Channel::create<ParlioEngine>(config1);
    auto channel2 = fl::Channel::create<ParlioEngine>(config2);
    auto channel3 = fl::Channel::create<ParlioEngine>(config3);
    auto channel4 = fl::Channel::create<ParlioEngine>(config4);

    // Add channels to FastLED (they're automatically in the controller list)
    FastLED.addLedChannel(channel1);
    FastLED.addLedChannel(channel2);
    FastLED.addLedChannel(channel3);
    FastLED.addLedChannel(channel4);
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

    // When FastLED.show() is called:
    //   1. Engine's beginTransmission() is called with all channels
    //      - May block if poll() returns BUSY or DRAINING (previous frame still transmitting)
    //      - Queues all channel data for transmission
    //      - Starts DMA transmission
    //
    // When FastLED.show() returns, the engine will be in DRAINING, READY, or ERROR state:
    //   - DRAINING: Transmission queued/started and still in progress
    //   - READY: Transmission already complete
    //   - ERROR: An error occurred during transmission (check getLastError())
    //
    // The engine manages state transitions internally via poll() state machine.
    // Advanced users can call poll() directly for non-blocking state queries.
    FastLED.show();  // Show updates all channels
    delay(20);
}
```


## Channel Engine States

The `IChannelEngine` uses a 4-state machine to manage non-blocking LED transmission:

- **READY** - Hardware idle; ready to accept `beginTransmission()` non-blocking
- **BUSY** - Active: channels transmitting or queued (scheduler still enqueuing)
- **DRAINING** - All channels submitted; DMA still transmitting; `beginTransmission()` will block
- **ERROR** - Engine encountered an error; check `getLastError()` for details

**Typical state flow**: **READY** → `beginTransmission()` → **BUSY** → (all queued) → **DRAINING** → (transmission done) → **READY**

**Note**: Some implementations may skip the **BUSY** state entirely if they use internal mechanisms (like ISRs) to asynchronously queue pending channels as the hardware becomes ready.

If an error occurs at any point, the engine transitions to **ERROR** state.

### Non-Blocking Show API

The channel engine provides a non-blocking `poll()` API for advanced use cases:

```cpp
// Start transmission (may block if previous frame still transmitting)
engine->beginTransmission(channels);

// Poll until ready (non-blocking)
IChannelEngine::EngineState state;
while ((state = engine->poll()) != IChannelEngine::EngineState::READY) {
    if (state == IChannelEngine::EngineState::ERROR) {
        // Handle error
        Serial.println(engine->getLastError().c_str());
        break;
    }
    // Do other work while DMA transmits...
    // Examples: process input, compute next frame, handle network, etc.
}
```

**How it works:**
- `beginTransmission()` - Queues channels and starts DMA transmission (may block if poll() returns BUSY or DRAINING)
- `poll()` - Non-blocking state check; returns current engine state and may advance state machine
- `getLastError()` - Returns error description string when poll() returns ERROR state
- User code can interleave computation while DMA hardware handles LED updates asynchronously

**Blocking Behavior:**
- `FastLED.show()` may or may not block, depending on engine state
- When `FastLED.show()` returns, all engines are guaranteed to be in **DRAINING**, **READY**, or **ERROR** state
- This means transmission has been queued/started and may still be transmitting (DRAINING), already complete (READY), or encountered an error (ERROR)
- Most users don't need to interact with the engine directly - the poll() API is for advanced scenarios requiring fine-grained control over CPU/DMA parallelism

### "Chipset timing mismatch"

All strips managed by a single engine must use compatible chipset timing. If you need to mix chipsets (e.g., WS2812 + SK6812), you'll need to use separate engines for each timing configuration.

## API Status

**Current Status**: Experimental / Under Development

The Channels API is actively being developed. Interface stability is not guaranteed. For production code, consider using the traditional `FastLED.addLeds()` API until this interface stabilizes.

## See Also

- `channel.h` - Channel class definition with factory methods
- `channel_config.h` - Configuration structures
- `channel_engine.h` - Platform engine interface with non-blocking poll() API
- `chipset_timing_config.h` - Chipset timing definitions
- `examples/BlinkParallel.ino` - Traditional parallel LED example
