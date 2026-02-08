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
2. **RMT** (High) - All ESP32 variants: Remote control peripheral (recommended default)
3. **I2S** (Medium) - ESP32-S3: LCD_CAM via I80 bus (experimental)
4. **SPI** (Low) - ESP32, S2, S3: DMA-based SPI transmission (deprioritized due to reliability issues)
5. **UART** (Lowest) - All ESP32 variants: Wave8 encoding (broken, not recommended)

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

## Advanced: Runtime Channel Creation with Channel API

For advanced use cases requiring runtime configuration or engine affinity control, you can use the Channel API:

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_config.h"

#define NUM_LEDS 60
#define PIN 16

CRGB leds[NUM_LEDS];

void setup() {
    // Create channel configuration
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(PIN, timing, fl::span<CRGB>(leds, NUM_LEDS), RGB);

    // Option 1: Create channel (automatic engine selection)
    auto channel = fl::Channel::create(config);
    FastLED.add(channel);

    // Option 2: Create and register in one call (preferred)
    auto channel2 = FastLED.add(config);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
    FastLED.show();
    delay(20);
}
```

**Engine Selection:**
- Engine selection happens automatically based on platform capabilities
- Use affinity to bind to a specific engine:
  ```cpp
  fl::ChannelOptions options;
  options.mAffinity = "PARLIO";  // or "RMT", "SPI", etc.
  fl::ChannelConfig config(pin, timing, leds, RGB, options);
  ```
- Available engines: PARLIO (ESP32-P4/C6/H2), RMT (all ESP32 variants, recommended), I2S (ESP32-S3, experimental), SPI (ESP32/S2/S3, low priority), UART (all variants, broken)
- See `setExclusiveDriver()` for forcing specific engines globally

**Note:** This advanced API is primarily for runtime configuration or testing. Most users should use `FastLED.addLeds<>()` which automatically selects the best engine and uses static storage.

## Runtime Reconfiguration with `applyConfig()`

Once a channel is created and registered, you can update its reconfigurable settings at runtime without destroying and recreating it. This is useful for LED controller software where users change settings through a UI.

### What `applyConfig()` updates:
- **RGB order** (`GRB`, `BGR`, `RGB`, etc.)
- **LED buffer** (pointer and size — can switch to a different array)
- **Color correction**
- **Color temperature**
- **Dither mode**
- **RGBW settings**

### What it does NOT change (structural/identity fields):
- Pin assignment
- Chipset timing
- Engine binding
- Channel ID

### Example: Reconfigure a channel at runtime

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

#define PIN 16

CRGB leds1[256];
fl::ChannelPtr channel;

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(PIN, timing, fl::span<CRGB>(leds1, 256), GRB);
    channel = fl::Channel::create(config);
    FastLED.add(channel);
}

// Called when user changes settings (e.g., from a web UI)
void reconfigure(CRGB* newLeds, int newCount, EOrder newOrder) {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions opts;
    opts.mCorrection = TypicalSMD5050;
    opts.mTemperature = CRGB(200, 180, 160);
    opts.mDitherMode = DISABLE_DITHER;

    fl::ChannelConfig newConfig(PIN, timing,
        fl::span<CRGB>(newLeds, newCount), newOrder, opts);
    channel->applyConfig(newConfig);
    // Channel keeps its ID, pin, engine — only the settings above change.
}

void loop() {
    fill_solid(leds1, 256, CRGB::Red);
    FastLED.show();
    delay(20);
}
```

### Managing multiple channels

Store `ChannelPtr` objects so you can reconfigure individual channels later:

```cpp
fl::vector<fl::ChannelPtr> channels;

void setupChannels(const int* pins, int nrOfPins,
                   CRGB* leds, const int* ledsPerPin) {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    int startLed = 0;
    for (int i = 0; i < nrOfPins; i++) {
        fl::ChannelConfig config(pins[i], timing,
            fl::span<CRGB>(&leds[startLed], ledsPerPin[i]), GRB);
        auto ch = fl::Channel::create(config);
        FastLED.add(ch);
        channels.push_back(ch);
        startLed += ledsPerPin[i];
    }
}

void reconfigureChannel(int index, CRGB* newLeds, int newCount, EOrder order) {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig updated(pins[index], timing,
        fl::span<CRGB>(newLeds, newCount), order);
    channels[index]->applyConfig(updated);
}
```

**Bug reports welcome:** If calling `applyConfig()` does not correctly update any of the reconfigurable fields listed above, please report it as a bug.

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
- **ERROR** - Engine encountered an error; error message included in `EngineState.error`

**Typical state flow**: **READY** → `beginTransmission()` → **BUSY** → (all queued) → **DRAINING** → (transmission done) → **READY**

**Note**: Some implementations may skip the **BUSY** state entirely if they use internal mechanisms (like ISRs) to asynchronously queue pending channels as the hardware becomes ready.

If an error occurs at any point, the engine transitions to **ERROR** state.

### Non-Blocking Show API

The channel engine provides a non-blocking `poll()` API for advanced use cases:

```cpp
// Start transmission (may block if previous frame still transmitting)
engine->beginTransmission(channels);

// Poll until ready or draining (non-blocking)
while (true) {
    IChannelEngine::EngineState result = engine->poll();

    if (result == IChannelEngine::EngineState::READY ||
        result == IChannelEngine::EngineState::DRAINING) {
        break;  // Transmission queued/started (DRAINING) or complete (READY)
    }

    if (result == IChannelEngine::EngineState::ERROR) {
        // Handle error - error message is in result.error
        Serial.print("Engine error: ");
        Serial.println(result.error.c_str());
        break;
    }

    // State is BUSY - transmission still being queued
    // Do other work while waiting...
    // Examples: process input, compute next frame, handle network, etc.
}
```

**How it works:**
- `beginTransmission()` - Queues channels and starts DMA transmission (may block if poll() returns BUSY or DRAINING)
- `poll()` - Non-blocking state check; returns `EngineState` with state and optional error message
- User code can interleave computation while DMA hardware handles LED updates asynchronously
- Error messages are returned in `EngineState.error` field when state == ERROR
- Backward compatible: `poll() == EngineState::READY` still works via implicit conversion

**Blocking Behavior:**
- `FastLED.show()` may or may not block, depending on engine state
- When `FastLED.show()` returns, all engines are guaranteed to be in **DRAINING**, **READY**, or **ERROR** state
- This means transmission has been queued/started and may still be transmitting (DRAINING), already complete (READY), or encountered an error (ERROR)
- Most users don't need to interact with the engine directly - the poll() API is for advanced scenarios requiring fine-grained control over CPU/DMA parallelism

### Mixing Chipset Timings

Channel engines can handle different chipset timings in two ways:

**Sequential (Single Engine)**: A single engine can drive strips with different timings by transmitting them one after another in the same frame. This works automatically with no special configuration:

```cpp
// Single engine handles both WS2812 (800kHz) and WS2816 (different timing) sequentially
CRGB ws2812_leds[60];
CRGB ws2816_leds[60];

FastLED.addLeds<WS2812, 16>(ws2812_leds, 60);  // First strip
FastLED.addLeds<WS2816, 17>(ws2816_leds, 60);  // Second strip (different timing)

// Both strips update sequentially through the same engine
FastLED.show();
```

**Parallel (Multiple Engines)**: To transmit different chipset timings **simultaneously**, bind each timing group to a separate engine using affinity:

```cpp
// Parallel transmission: WS2812 strips on one engine, WS2816 on another
CRGB ws2812_strip1[100];
CRGB ws2812_strip2[100];
CRGB ws2816_strip1[100];
CRGB ws2816_strip2[100];

// WS2812 strips bound to RMT engine
fl::ChannelOptions ws2812_opts;
ws2812_opts.mAffinity = "RMT";
auto timing_ws2812 = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();

FastLED.add(fl::ChannelConfig(16, timing_ws2812, ws2812_strip1, RGB, ws2812_opts));
FastLED.add(fl::ChannelConfig(17, timing_ws2812, ws2812_strip2, RGB, ws2812_opts));

// WS2816 strips bound to SPI engine (parallel with RMT)
fl::ChannelOptions ws2816_opts;
ws2816_opts.mAffinity = "SPI";
auto timing_ws2816 = fl::makeTimingConfig<fl::TIMING_WS2816>();

FastLED.add(fl::ChannelConfig(18, timing_ws2816, ws2816_strip1, RGB, ws2816_opts));
FastLED.add(fl::ChannelConfig(19, timing_ws2816, ws2816_strip2, RGB, ws2816_opts));

// All strips update: RMT engine handles WS2812 strips in parallel with SPI engine handling WS2816
FastLED.show();
```

**Key Points:**
- Sequential: Automatic, no configuration needed, strips update one after another
- Parallel: Requires affinity binding to separate engines, all strips update simultaneously
- Use parallel for maximum performance when mixing chipset timings

## API Status

**Current Status**: Experimental / Under Development

The Channels API is actively being developed. Interface stability is not guaranteed. For production code, consider using the traditional `FastLED.addLeds()` API until this interface stabilizes.

## See Also

- `channel.h` - Channel class definition with factory methods
- `channel_config.h` - Configuration structures
- `channel_engine.h` - Platform engine interface with non-blocking poll() API
- `chipset_timing_config.h` - Chipset timing definitions
- `examples/BlinkParallel.ino` - Traditional parallel LED example
