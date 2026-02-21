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
| **PARLIO** | 4 (Highest) | ESP32-P4, C6, H2, C5 | Parallel I/O with hardware timing |
| **RMT** | 2 (Recommended) | All ESP32 variants | Recommended default, reliable |
| **I2S** | 1 | ESP32-S3 | LCD_CAM via I80 bus (experimental) |
| **SPI** | 0 | ESP32, S2, S3 | DMA-based, deprioritized due to reliability |
| **UART** | -1 (Lowest) | All ESP32 variants | Wave8 encoding (broken, not recommended) |

### Overriding Engine Selection

For testing or performance tuning, you can control engine selection:

```cpp
void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(16, timing, leds, RGB);
    FastLED.add(config);

    // Method 1: Force a specific engine exclusively (disables all others)
    FastLED.setExclusiveDriver("RMT");

    // Method 2: Enable/disable specific engines
    FastLED.setDriverEnabled("PARLIO", true);   // Enable
    FastLED.setDriverEnabled("SPI", false);     // Disable

    // Method 3: Adjust engine priority (higher = preferred)
    // Engines are sorted by priority - changing priority triggers re-sort
    FastLED.setDriverPriority("RMT", 9000);     // Increase priority
    FastLED.setDriverPriority("PARLIO", 8000);  // Set below RMT

    // Query available drivers (sorted by priority, high to low)
    for (size_t i = 0; i < FastLED.getDriverCount(); i++) {
        auto info = FastLED.getDriverInfos()[i];
        Serial.printf("%s: priority=%d, enabled=%s\n",
                      info.name.c_str(), info.priority,
                      info.enabled ? "yes" : "no");
    }
}
```

**Control methods:**
- `setExclusiveDriver(name)` - Disable all engines except the named one
- `setDriverEnabled(name, enabled)` - Enable/disable specific engine
- `setDriverPriority(name, priority)` - Change priority (triggers automatic re-sort)

**When to override:**
- Testing different engines for performance comparison
- Debugging hardware-specific issues
- Forcing a specific engine for known-good behavior
- Prioritizing a custom third-party engine over built-in engines

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
    FastLED.show();  // Triggers onChannelEnqueued for "my_strip"
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

### Gamma Correction (UCS7604 16-bit)

The UCS7604 chipset supports 16-bit color depth, which benefits from gamma correction to produce perceptually smooth brightness gradients. The Channels API provides per-channel gamma control:

```cpp
#include <FastLED.h>
#include "fl/channels/config.h"
#include "fl/chipsets/led_timing.h"

#define NUM_LEDS 60

CRGB leds[NUM_LEDS];

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();
    fl::ChannelConfig config(fl::ClocklessChipset(2, timing), leds, RGB);

    auto channel = FastLED.add(config);
    channel->setGamma(3.2f);  // Override gamma (default is 2.8)

    FastLED.setBrightness(128);
}

void loop() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(20);
}
```

**How gamma resolution works:**

| Method | Scope | Precedence |
|--------|-------|------------|
| `channel->setGamma(3.2f)` | Per-channel | Highest - overrides built-in default |
| *(no call)* | Built-in default | 2.8 (matches UCS7604 datasheet recommendation) |

**Common gamma values:**
- **1.0** - Linear (no correction, useful for data visualization)
- **2.2** - sRGB standard (common for displays)
- **2.8** - Default for UCS7604 16-bit modes
- **3.2** - Steeper curve, darker midtones, more contrast

**Per-channel example (two strips, different gamma):**

```cpp
CRGB warm_leds[60];
CRGB cool_leds[60];

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();

    auto warm = FastLED.add(fl::ChannelConfig(
        fl::ClocklessChipset(2, timing), warm_leds, RGB));
    warm->setGamma(2.2f);  // Gentle curve for warm ambiance

    auto cool = FastLED.add(fl::ChannelConfig(
        fl::ClocklessChipset(4, timing), cool_leds, RGB));
    cool->setGamma(3.2f);  // Steep curve for high contrast
}
```

**Note:** Gamma correction only affects 16-bit UCS7604 modes (`TIMING_UCS7604_800KHZ` with 16-bit, `TIMING_UCS7604_1600KHZ`). 8-bit mode passes values through unchanged.

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

| State | Description | `poll()` return value meaning |
|-------|-------------|-------------------------------|
| **READY** | Idle, ready to accept new data | Hardware is idle, safe to call `show()` |
| **BUSY** | Actively transmitting or queuing channels | Transmission in progress, engine is working |
| **DRAINING** | All channels enqueued, DMA still transmitting | Transmission finishing, no more data needed |
| **ERROR** | Hardware error occurred | Error state, check error message |

**State flow:** READY → `show()` → BUSY → DRAINING → `poll()` → READY

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

    // Check if engine is ready for new data
    fl::IChannelEngine::EngineState state = manager.poll();

    if (state == fl::IChannelEngine::EngineState::READY) {
        // Hardware is idle - safe to show next frame
        FastLED.show();
    } else if (state == fl::IChannelEngine::EngineState::DRAINING) {
        // DMA transmission finishing - no more poll() needed this frame
        // Do useful work while waiting
        computeNextFrame();
    } else if (state == fl::IChannelEngine::EngineState::ERROR) {
        Serial.println(state.error.c_str());
    }
    // BUSY state: Keep polling until DRAINING or READY

    delay(20);
}
```

**When to use:**
- High frame rate applications requiring CPU/DMA parallelism
- Custom transmission scheduling across multiple engines
- Fine-grained control over transmission timing

**Key insight:** DRAINING state signals that the engine doesn't need more `poll()` calls - all channels are enqueued and DMA is finishing transmission. This is the optimal time to compute the next frame.

---

## Implementing a Custom Channel Engine

Third-party developers can create custom channel engines to support new hardware peripherals or transmission protocols. This section covers the requirements and best practices.

### Overview

A channel engine bridges the gap between high-level `Channel` objects and low-level hardware. Channels pass their encoded data to engines via an **ephemeral enqueue** - engines manage transmission, not channel registration.

**Key responsibilities:**
- Accept ChannelData pointers via `enqueue()` (temporary, per-frame)
- Manage two-queue architecture: **pending** → **in-flight**
- Protect ChannelData with `isInUse` flag during transmission
- Implement 4-state machine: READY → BUSY → DRAINING → READY

### Required Interface: IChannelEngine

Inherit from `fl::IChannelEngine` and implement these methods:

```cpp
#include "fl/channels/engine.h"
#include "fl/channels/data.h"

class MyCustomEngine : public fl::IChannelEngine {
public:
    /// Check if engine can handle this channel type
    bool canHandle(const ChannelDataPtr& data) const override {
        // Example: Only accept clockless chipsets
        return data && data->isClockless();
    }

    /// Enqueue channel data for transmission (ephemeral, per-frame)
    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueuedChannels.push_back(channelData);
        }
    }

    /// Trigger transmission of enqueued channels
    void show() override {
        if (mEnqueuedChannels.empty()) {
            return;
        }

        // CRITICAL: Mark all channels as in-use BEFORE transmission
        for (auto& channel : mEnqueuedChannels) {
            channel->setInUse(true);
        }

        // Move pending queue to in-flight queue
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Start hardware transmission
        beginTransmission(fl::span<const ChannelDataPtr>(
            mTransmittingChannels.data(),
            mTransmittingChannels.size()));
    }

    /// Query engine state and perform maintenance
    EngineState poll() override {
        // Check hardware status
        if (isHardwareBusy()) {
            return EngineState::BUSY;
        }

        if (isTransmitting()) {
            return EngineState::DRAINING;
        }

        // Transmission complete - CRITICAL: Clear isInUse flags
        if (!mTransmittingChannels.empty()) {
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
        }

        return EngineState::READY;
    }

    /// Get engine name for affinity binding
    fl::string getName() const override {
        return fl::string::from_literal("MY_ENGINE");
    }

    /// Declare capabilities (clockless, SPI, or both)
    Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels);
    bool isHardwareBusy() const;
    bool isTransmitting() const;

    // Two-queue architecture (required)
    fl::vector<ChannelDataPtr> mEnqueuedChannels;     // Pending queue
    fl::vector<ChannelDataPtr> mTransmittingChannels; // In-flight queue
};
```

### Critical: isInUse Flag Management

The `isInUse` flag prevents channels from modifying their data while the engine is transmitting. **All engines MUST manage this flag correctly.**

**Rules:**
1. **Set `isInUse(true)` in `show()`** - Before starting transmission
2. **Clear `isInUse(false)` in `poll()`** - When transmission completes (READY state)
3. **Clear `isInUse(false)` on errors** - When returning ERROR state

**Why it matters:**
- Channels reuse their ChannelData buffer across frames
- Without protection, channels could overwrite data mid-transmission
- The safety check in `Channel::showPixels()` prevents this:
  ```cpp
  if (mChannelData->isInUse()) {
      FL_ASSERT(false, "Skipping update - buffer in use by engine");
      return;
  }
  ```

**Example (correct pattern):**
```cpp
void show() override {
    // Mark in-use BEFORE transmission
    for (auto& channel : mEnqueuedChannels) {
        channel->setInUse(true);  // ✅ Prevent modification
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();
    startHardware();
}

EngineState poll() override {
    if (hardwareComplete()) {
        // Clear in-use AFTER transmission
        for (auto& channel : mTransmittingChannels) {
            channel->setInUse(false);  // ✅ Allow modification
        }
        mTransmittingChannels.clear();
        return EngineState::READY;
    }
    return EngineState::DRAINING;
}
```

### Two-Queue Architecture

Engines use a dual-queue system to separate pending data from in-flight data:

**Pending Queue (`mEnqueuedChannels`):**
- Receives ChannelData via `enqueue()` calls
- Accumulates channels until `show()` is called
- Cleared in `show()` after moving to in-flight queue

**In-Flight Queue (`mTransmittingChannels`):**
- Holds channels currently being transmitted
- Populated by `show()`, cleared by `poll()` when READY
- Protected by `isInUse` flag

**Lifecycle flow:**
```
Channel::showPixels()
    ↓
engine->enqueue(data)  →  mEnqueuedChannels.push_back(data)
    ↓
engine->show()         →  Move to mTransmittingChannels, clear mEnqueuedChannels
    ↓
engine->poll()         →  Check hardware status
    ↓
EngineState::READY     →  Clear mTransmittingChannels, ready for next frame
```

### State Machine

Engines implement a 4-state machine for non-blocking transmission:

| State | Description | When `poll()` returns this |
|-------|-------------|------------|
| **READY** | Idle, ready for new data | Hardware idle, no transmissions in progress |
| **BUSY** | Actively transmitting channels | Hardware actively working, still accepting data |
| **DRAINING** | All channels enqueued, DMA finishing | All data submitted, no more `poll()` needed |
| **ERROR** | Hardware error occurred | Error state, check error message |

**State flow:**
```
READY → show() → BUSY → (all queued) → DRAINING → (hardware complete) → READY
                                           ↓
                                       (error) → ERROR
```

**Implementation notes:**
- Most engines skip BUSY (instant transition to DRAINING after `show()`)
- DRAINING signals "all data enqueued, DMA finishing" - optimal time for CPU work
- DRAINING means poll() doesn't need to be called again for current frame
- ERROR requires manual recovery (reset hardware, clear state)

### Registration with ChannelBusManager

Register your engine with the bus manager to make it available:

```cpp
#include "fl/channels/bus_manager.h"

// In your platform initialization code
void setupCustomEngine() {
    auto engine = fl::make_shared<MyCustomEngine>();

    // Register with priority (higher = preferred)
    // Built-in ESP32 engines use priorities 4 (PARLIO), 2 (RMT), 1 (I2S), 0 (SPI), -1 (UART)
    // Custom engines can use any integer priority value
    fl::ChannelBusManager::instance().addEngine(
        10,                // Priority (higher than built-in engines)
        engine,            // Shared pointer to engine
        "MY_ENGINE"        // Unique name for affinity binding
    );
}
```

**Priority guidelines for custom engines:**
- Use priority values higher than built-in engines (>4) to override defaults
- Use negative priorities (<0) for low-priority fallback implementations
- Priority values are just integers - no predefined ranges required

**Engine selection:**
1. Bus manager maintains engines sorted by priority (high to low)
2. Iterates engines in priority order, calls `canHandle()` on each
3. First engine returning `true` wins
4. User can override with `ChannelOptions.mAffinity` or `FastLED.setExclusiveDriver()`

**Priority modification:**
- Engines are sorted by priority on registration (via `addEngine()`)
- Priority can be changed at runtime via `setDriverPriority(name, priority)`
- Changing priority triggers automatic re-sort of engine list
- Higher priority engines are checked first (e.g., priority 10 before priority 2)

### DMA Wait Pattern

**`show()` must wait for READY before starting a new frame.** The correct pattern is a simple spin on `poll()`:

```cpp
void show() override {
    // Wait for previous frame to finish.
    while (poll() != EngineState::READY) {
        // poll() drives the state machine and clears in-use flags.
    }

    // Now safe to start new frame...
}
```

**Do NOT branch on DRAINING or other intermediate states** inside `show()`'s wait loop. The `poll()` method is responsible for driving the state machine to READY — `show()` just needs to wait for it. Branching on intermediate states (e.g., breaking early on DRAINING) splits the "wait for previous frame" logic across multiple places and makes the code harder to reason about.

### Best Practices

**Memory Management:**
- Use `fl::vector` for dynamic arrays (not `std::vector`)
- Store ChannelDataPtr as `fl::shared_ptr<ChannelData>` (not raw pointers)
- Never delete ChannelData - shared_ptr handles lifetime

**Thread Safety:**
- `enqueue()`, `show()`, `poll()` are called from main thread
- ISR callbacks must be marked with `FL_IRAM` attribute
- Use memory barriers when sharing state with ISRs
- See `src/platforms/esp/32/drivers/parlio/parlio_engine.h` for ISR patterns

**Error Handling:**
- Return `EngineState::ERROR` on hardware failures
- Clear `isInUse` flags before returning ERROR
- Log errors with `FL_WARN()` or `FL_DBG()`
- Provide diagnostic information in error messages

**Performance:**
- Minimize work in `show()` and `poll()` (hot paths)
- Use DMA for data transmission (not CPU loops)
- Avoid memory allocation in hot paths
- Pre-allocate buffers during initialization

**Compatibility:**
- Implement `canHandle()` conservatively (reject unsupported chipsets)
- Check timing constraints in `canHandle()` if hardware has limits
- Support both clockless and SPI if hardware permits
- Document hardware requirements in engine header

### Example Engines

Reference implementations in the codebase:

**Simple (good starting point):**
- `src/platforms/esp/32/drivers/uart/channel_engine_uart.cpp.hpp` - UART Wave8 encoding
- `src/platforms/stub/clockless_channel_stub.h` - Stub engine for testing

**Advanced (full-featured):**
- `src/platforms/esp/32/drivers/rmt/rmt_5/channel_engine_rmt.cpp.hpp` - RMT with ISR callbacks
- `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp.hpp` - PARLIO with chipset grouping

**Key differences:**
- UART: Simple blocking transmission
- RMT: ISR-driven async transmission with channel pooling
- PARLIO: Multi-lane parallel output with chipset grouping

### Testing Your Engine

Create unit tests following the existing patterns:

```cpp
#include "test.h"
#include "fl/channels/engine.h"
#include "fl/channels/data.h"

FL_TEST_CASE("MyEngine: Basic enqueue and transmission") {
    auto engine = fl::make_shared<MyCustomEngine>();

    // Create test data
    auto data = fl::ChannelData::create(5, timing, fl::move(encodedData));

    // Enqueue
    engine->enqueue(data);

    // Verify isInUse flag lifecycle
    FL_CHECK_FALSE(data->isInUse());  // Not in use before show()

    engine->show();
    FL_CHECK(data->isInUse());  // In use during transmission

    // Poll until complete
    while (engine->poll() != fl::IChannelEngine::EngineState::READY) {
        fl::delayMicroseconds(100);
    }

    FL_CHECK_FALSE(data->isInUse());  // Not in use after transmission
}
```

See `tests/fl/channels/engine.cpp` for more test examples.

---

## Reference

**Headers:**
- `fl/channels/channel.h` - Channel class and factory methods
- `fl/channels/config.h` - ChannelConfig, ClocklessChipset, SpiChipsetConfig
- `fl/channels/options.h` - ChannelOptions (correction, temperature, dither, affinity, gamma)
- `fl/channels/channel_events.h` - Lifecycle event callbacks
- `fl/channels/engine.h` - Engine interface and state machine

**Examples:**
- `examples/BlinkParallel.ino` - Parallel LED strip example
