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

### Advanced Engine Selection

The Channels API automatically selects the best available hardware engine for your platform:

**Engine Priority (ESP32 platforms):**
1. **PARLIO** (Highest) - ESP32-P4, C6, H2: Parallel I/O with hardware timing
2. **SPI** (Medium) - ESP32, S2, S3: DMA-based SPI transmission
3. **RMT** (Fallback) - All ESP32 variants: Remote control peripheral

**Driver Control:**

You can override automatic selection for testing or performance tuning:

```cpp
#include "FastLED.h"

#define NUM_LEDS 300
#define DATA_PIN 23

CRGB leds[NUM_LEDS];

void setup() {
    // Standard API - add LED strip
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);

    // Force SPI engine exclusively (ESP32/S2/S3 only)
    FastLED.setExclusiveDriver("SPI");

    // Check which drivers are available
    for (size_t i = 0; i < FastLED.getDriverCount(); i++) {
        auto drivers = FastLED.getDriverInfos();
        auto& info = drivers[i];
        Serial.printf("Driver: %s, Priority: %d, Enabled: %s\n",
                      info.name.c_str(), info.priority,
                      info.enabled ? "yes" : "no");
    }
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
    FastLED.show();
    delay(20);
}
```

**Note:** Most users don't need `setExclusiveDriver()` - the automatic selection provides optimal performance for each platform.

## Advanced: Direct Engine Selection with Channel API

For advanced use cases requiring direct engine control, you can use the Channel API:

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_config.h"
#include "platforms/esp/32/drivers/parlio/channel_engine_parlio.h"

#define NUM_LEDS 60
#define PIN 16

CRGB leds[NUM_LEDS];

void setup() {
    // Create channel configuration
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(PIN, timing, fl::span<CRGB>(leds, NUM_LEDS), RGB);

    // Create channel with specific engine type
    auto channel = fl::Channel::create<fl::ChannelEnginePARLIO>(config);

    // Register with FastLED
    FastLED.addLedChannel(channel);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
    FastLED.show();
    delay(20);
}
```

**Available Engine Types:**
- `fl::ChannelEnginePARLIO` - ESP32-P4, C6, H2 (parallel I/O)
- `fl::ChannelEngineSpi` - ESP32, S2, S3 (SPI-based)
- `fl::ChannelEngineRMT` - RMT5 (ESP32-C6, H2, P4)
- `fl::ChannelEngineRMT4` - RMT4 (ESP32 classic, S2, S3)

**Note:** This advanced API is primarily for testing or special use cases. Most users should use `FastLED.addLeds<>()` which automatically selects the best engine.

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
    // Standard FastLED API - automatically selects best available engine
    // (PARLIO on ESP32-P4/C6/H2, SPI on ESP32/S2/S3, RMT as fallback)
    FastLED.addLeds<WS2812, PIN1>(strip1, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN2>(strip2, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN3>(strip3, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN4>(strip4, NUM_LEDS);

    // Optional: Force a specific driver for testing or performance tuning
    // FastLED.setExclusiveDriver("PARLIO");  // ESP32-P4/C6/H2 only
    // FastLED.setExclusiveDriver("SPI");     // ESP32/S2/S3 only
    // FastLED.setExclusiveDriver("RMT");     // All ESP32 variants
}

void loop() {
    // Set different colors on each strip
    fill_solid(strip1, NUM_LEDS, CRGB::Red);
    fill_solid(strip2, NUM_LEDS, CRGB::Green);
    fill_solid(strip3, NUM_LEDS, CRGB::Blue);
    fill_solid(strip4, NUM_LEDS, CRGB::Yellow);

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

    FastLED.show();
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
