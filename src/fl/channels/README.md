# FastLED Channels API

## Overview

The Channels API provides a modern, hardware-accelerated interface for driving multiple LED strips in parallel with DMA-based timing. It abstracts platform-specific hardware (PARLIO, RMT, SPI, I2S) into a unified interface that works across ESP32 variants and other microcontrollers.

**Key benefits:**
- **Parallel output** - Drive multiple LED strips simultaneously with hardware timing
- **Zero CPU overhead** - DMA-based transmission frees the CPU for other tasks
- **Automatic engine selection** - Platform-optimal hardware automatically chosen
- **Runtime reconfiguration** - Change LED settings without recreating channels

## Architecture

The system consists of two layers:

1. **Channel** - High-level LED strip controller with explicit configuration API
2. **ChannelEngine** - Low-level hardware driver (RMT, PARLIO, SPI, I2S, UART)

Users create `Channel` objects using the Channel API (recommended) or the template-based `FastLED.addLeds<>()` API (backwards compatible). The engine layer is managed automatically based on platform capabilities and priorities.

---

## Basic Usage

### Channel API (Recommended)

The Channel API provides a clean, explicit interface for creating and configuring LED strips:

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

#define NUM_LEDS 60
#define PIN1 16
#define PIN2 17

CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

void setup() {
    // Create channel configurations with names
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();

    fl::ChannelConfig config1("left_strip", fl::ClocklessChipset(PIN1, timing),
        fl::span<CRGB>(leds1, NUM_LEDS), RGB);
    fl::ChannelConfig config2("right_strip", fl::ClocklessChipset(PIN2, timing),
        fl::span<CRGB>(leds2, NUM_LEDS), RGB);

    // Register channels with FastLED
    auto ch1 = FastLED.add(config1);
    auto ch2 = FastLED.add(config2);

    Serial.printf("Created: %s and %s\n", ch1->name().c_str(), ch2->name().c_str());
}

void loop() {
    fill_solid(leds1, NUM_LEDS, CRGB::Red);
    fill_solid(leds2, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(1000);
}
```

**Benefits:**
- Explicit configuration - no hidden template magic
- Runtime flexibility - chipset and settings can be changed dynamically
- Named channels for debugging and logging
- Direct access to channel objects for advanced control

### Backwards Compatible API

For existing code, the template-based `FastLED.addLeds<>()` API is still supported:

```cpp
#include "FastLED.h"

CRGB leds1[60];
CRGB leds2[60];

void setup() {
    // Template-based API (creates channels internally)
    FastLED.addLeds<WS2812, 16>(leds1, 60);
    FastLED.addLeds<WS2812, 17>(leds2, 60);
}

void loop() {
    fill_solid(leds1, 60, CRGB::Red);
    FastLED.show();
    delay(1000);
}
```

This API is simpler for basic use cases but offers less flexibility than the Channel API.

---

## Hardware Engine Selection

The system automatically selects the best hardware engine based on platform capabilities:

### Engine Priority (ESP32)

Engines are tried in priority order until one accepts the channel:

| Engine | Priority | Platforms | Status |
|--------|----------|-----------|--------|
| **PARLIO** | Highest | ESP32-P4, C6, H2 | Parallel I/O with hardware timing |
| **RMT** | High | All ESP32 variants | Recommended default, reliable |
| **I2S** | Medium | ESP32-S3 | LCD_CAM via I80 bus (experimental) |
| **SPI** | Low | ESP32, S2, S3 | DMA-based, deprioritized due to reliability |
| **UART** | Lowest | All ESP32 variants | Wave8 encoding (not recommended) |

### Overriding Engine Selection

For testing or performance tuning, you can force a specific engine:

```cpp
void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(16, timing, leds, RGB);
    FastLED.add(config);

    // Force RMT engine exclusively
    FastLED.setExclusiveDriver("RMT");

    // Query available drivers
    for (size_t i = 0; i < FastLED.getDriverCount(); i++) {
        auto info = FastLED.getDriverInfos()[i];
        Serial.printf("%s: priority=%d, enabled=%s\n",
                      info.name.c_str(), info.priority,
                      info.enabled ? "yes" : "no");
    }
}
```

**When to override:**
- Testing different engines for performance comparison
- Debugging hardware-specific issues
- Forcing a specific engine for known-good behavior

**Default behavior is recommended** - automatic selection provides optimal performance and reliability.

---

## Advanced Features

### Channel Lifecycle Events

Register callbacks for channel lifecycle events:

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

CRGB leds[60];

void setup() {
    // Register event listeners via FastLED
    auto& events = FastLED.channelEvents();

    // Called when channel is created
    events.onChannelCreated.add([](const fl::Channel& ch) {
        Serial.printf("Channel created: %s\n", ch.name().c_str());
    });

    // Called when channel data is enqueued to engine
    events.onChannelEnqueued.add([](const fl::Channel& ch, const fl::string& engine) {
        Serial.printf("%s → %s\n", ch.name().c_str(), engine.c_str());
    });

    // Create channel (triggers onChannelCreated)
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config("my_strip", fl::ClocklessChipset(5, timing),
        fl::span<CRGB>(leds, 60), RGB);
    FastLED.add(config);
}

void loop() {
    fill_rainbow(leds, 60, 0, 255 / 60);
    FastLED.show();  // Triggers onChannelEnqueued
    delay(20);
}
```

**Available events:**
- `onChannelCreated` - After channel construction
- `onChannelAdded` - After adding to FastLED controller list
- `onChannelEnqueued` - After data enqueued to engine
- `onChannelConfigured` - After `applyConfig()` called
- `onChannelRemoved` - After removing from controller list
- `onChannelBeginDestroy` - Before channel destruction

**Use cases:**
- Debug logging and performance profiling
- Integration with monitoring systems
- Synchronization with external hardware

### Runtime Reconfiguration

Change LED settings at runtime without recreating channels using `applyConfig()`:

```cpp
fl::ChannelPtr channel;

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(16, timing, leds, GRB);
    channel = fl::Channel::create(config);
    FastLED.add(channel);
}

// Called from UI/network handler
void updateSettings(CRGB* newLeds, int count, EOrder order) {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions opts;
    opts.mCorrection = TypicalSMD5050;
    opts.mTemperature = Tungsten100W;
    opts.mDitherMode = DISABLE_DITHER;

    fl::ChannelConfig newConfig(16, timing, fl::span<CRGB>(newLeds, count), order, opts);
    channel->applyConfig(newConfig);
}
```

**What changes:**
- RGB order, LED buffer (pointer/size), color correction, temperature, dither mode, RGBW settings

**What stays the same:**
- Pin assignment, chipset timing, engine binding, channel ID

**Use cases:**
- Web-based LED controllers with live configuration
- User-adjustable color correction and RGB order
- Dynamic LED count/buffer switching

### Engine Affinity

Bind specific channels to specific engines (useful for mixing chipset timings in parallel):

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

#define NUM_LEDS 100

// Two strips with different chipset timings
CRGB ws2812_strip[NUM_LEDS];
CRGB ws2816_strip[NUM_LEDS];

void setup() {
    // WS2812 strips bound to RMT engine
    fl::ChannelOptions ws2812_opts;
    ws2812_opts.mAffinity = "RMT";
    auto timing_ws2812 = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    FastLED.add(fl::ChannelConfig(16, timing_ws2812,
        fl::span<CRGB>(ws2812_strip, NUM_LEDS), RGB, ws2812_opts));

    // WS2816 strips bound to SPI engine (transmits in parallel with RMT)
    fl::ChannelOptions ws2816_opts;
    ws2816_opts.mAffinity = "SPI";
    auto timing_ws2816 = fl::makeTimingConfig<fl::TIMING_WS2816>();
    FastLED.add(fl::ChannelConfig(18, timing_ws2816,
        fl::span<CRGB>(ws2816_strip, NUM_LEDS), RGB, ws2816_opts));
}

void loop() {
    // Different effects on each strip
    fill_rainbow(ws2812_strip, NUM_LEDS, 0, 255 / NUM_LEDS);
    fill_solid(ws2816_strip, NUM_LEDS, CRGB::Blue);

    FastLED.show();  // Both engines transmit simultaneously
    delay(20);
}
```

**Use cases:**
- Parallel transmission of different chipset timings (see "Mixing Chipset Timings" below)
- Testing specific engine implementations
- Debugging hardware-specific behavior

---

## API Status

**Channel API:** Recommended - Modern, flexible interface

The Channel API is the modern interface for FastLED. It provides explicit control over LED strip configuration and is recommended for new projects.

**Backwards Compatible API (`FastLED.addLeds<>()`):** Stable - Template-based convenience wrapper

The template-based API is maintained for backwards compatibility with existing code. It internally uses the Channel API but provides a simpler interface for basic use cases.

**Prefer Channel API when:**
- Building new projects from scratch
- Need runtime configuration (web UIs, MQTT control)
- Want explicit control over settings
- Working with dynamic LED strip configurations

---

## Low-Level Engine API

⚠️ **Advanced users only** - Most users don't need direct engine access. `FastLED.show()` handles everything automatically.

### Mixing Chipset Timings

Engines handle different chipset timings in two modes:

**Sequential (Default)** - Single engine transmits different timings one after another:

```cpp
// Automatic - no configuration needed
FastLED.addLeds<WS2812, 16>(leds1, 60);  // 800kHz timing
FastLED.addLeds<WS2816, 17>(leds2, 60);  // Different timing

FastLED.show();  // Sequential transmission through same engine
```

**Parallel (Explicit)** - Multiple engines transmit different timings simultaneously (see Engine Affinity example above).

### Engine States

Hardware engines use a 4-state machine for non-blocking DMA transmission:

| State | Description | `beginTransmission()` behavior |
|-------|-------------|-------------------------------|
| **READY** | Idle, ready to accept new data | Non-blocking |
| **BUSY** | Actively enqueueing channels | Blocks until DRAINING |
| **DRAINING** | All channels queued, DMA transmitting | Blocks until READY |
| **ERROR** | Hardware error occurred | Returns immediately |

**State flow:** READY → `beginTransmission()` → BUSY → DRAINING → READY

### Non-Blocking API

For advanced CPU/DMA parallelism (e.g., computing next frame while DMA transmits):

```cpp
#include "FastLED.h"
#include "fl/channels/bus_manager.h"

CRGB leds[300];

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(16, timing, fl::span<CRGB>(leds, 300), RGB);
    FastLED.add(config);
}

void computeNextFrame() {
    // Do CPU-intensive work while DMA transmits
    static uint8_t hue = 0;
    fill_rainbow(leds, 300, hue++, 255 / 300);
}

void loop() {
    // Get engine from ChannelBusManager
    auto& manager = fl::ChannelBusManager::instance();

    // Poll engine state
    while (true) {
        fl::IChannelEngine::EngineState state = manager.poll();

        if (state == fl::IChannelEngine::EngineState::READY ||
            state == fl::IChannelEngine::EngineState::DRAINING) {
            break;  // Transmission queued or complete
        }

        if (state == fl::IChannelEngine::EngineState::ERROR) {
            Serial.println(state.error.c_str());
            break;
        }

        // BUSY - do other work while DMA transmits
        computeNextFrame();
    }

    FastLED.show();
    delay(20);
}
```

**When to use:**
- High frame rate applications requiring CPU/DMA parallelism
- Custom transmission scheduling across multiple engines
- Fine-grained control over transmission timing

---

## Reference

**Headers:**
- `fl/channels/channel.h` - Channel class and factory methods
- `fl/channels/config.h` - ChannelConfig, ClocklessChipset, SpiChipsetConfig
- `fl/channels/options.h` - ChannelOptions (correction, temperature, dither, affinity)
- `fl/channels/channel_events.h` - Lifecycle event callbacks
- `fl/channels/engine.h` - Engine interface and state machine

**Examples:**
- `examples/BlinkParallel.ino` - Parallel LED strip example
