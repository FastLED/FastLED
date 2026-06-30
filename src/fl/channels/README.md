# FastLED Channels API

## Overview

The Channels API provides a modern, hardware-accelerated interface for driving multiple LED strips in parallel with DMA-based timing. It abstracts platform-specific hardware (PARLIO, RMT, SPI, I2S) into a unified interface that works across ESP32 variants and other microcontrollers.

**Key benefits:**
- **Parallel output** - Drive multiple LED strips simultaneously with hardware timing
- **Zero CPU overhead** - DMA-based transmission frees the CPU for other tasks
- **Automatic driver selection** - Platform-optimal hardware automatically chosen
- **Runtime reconfiguration** - Change LED settings without recreating channels

## Architecture

The system consists of two layers:

1. **Channel** - High-level LED strip controller with explicit configuration API
2. **ChannelEngine** - Low-level hardware driver (RMT, PARLIO, SPI, I2S, UART, FLEX_IO, OBJECT_FLED, BIT_BANG, ...)

Users create `Channel` objects using the Channel API (`FastLED.add(cfg)` / `Channel::create(cfg)`) or the template-based `FastLED.addLeds<>()` API. Both route through the same `ChannelManager` and driver layer; pick by call-site shape, not by maturity. The driver layer is managed automatically based on platform capabilities and priorities.

Two complementary dispatch modes are available (introduced by issue #2428, refined by #2459 / #2460):

- **Compile-time `fl::Bus` binding** — two equivalent entry points pin the driver at compile time:
  - `fl::TypedChannel<fl::Bus::RMT, fl::ClocklessChipset>::create(cfg)` — strongly-typed factory with `static_assert` for bus/chipset compatibility.
  - `FastLED.addLeds<WS2812, 4, GRB, fl::Bus::RMT>(leds, NUM)` — every `addLeds<>` variant takes an optional trailing `fl::Bus B = fl::Bus::AUTO` template parameter (#2460).

  In either case, naming `Bus::X` at the call site is what links the driver's translation unit, so `--gc-sections` drops every driver the sketch doesn't reference. Bus/chipset mismatches become `static_assert` errors rather than runtime warnings.
- **Runtime selection** — `FastLED.add(cfg)` is **non-template**. Pick the driver by setting `cfg.options.mBus = fl::Bus::RMT` (typed `enum class`). The non-template path auto-enrolls every driver on the platform via `fl::enableAllDrivers()` and emits a one-time `FL_WARN_ONCE` explaining the binary-size trade-off (suppress with `-DFASTLED_SUPPRESS_RUNTIME_DRIVER_WARNING`). For minimum binary size, use the compile-time path instead. Custom/mock drivers (whose names aren't in the `fl::Bus` enum) bind via priority dispatch — register the mock with `manager.addDriver()` and either let it win by priority, or use `manager.setExclusiveDriver(name)` to force-select.

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

### Template `addLeds<>` API

The familiar template-based `FastLED.addLeds<>()` form is a one-line convenience over the Channel API. It's the right pick for short sketches that don't need to reconfigure at runtime.

```cpp
#include "FastLED.h"

CRGB leds1[60];
CRGB leds2[60];

void setup() {
    // Each addLeds<> internally constructs a ChannelConfig.
    FastLED.addLeds<WS2812, 16>(leds1, 60);
    FastLED.addLeds<WS2812, 17>(leds2, 60);
}

void loop() {
    fill_solid(leds1, 60, CRGB::Red);
    FastLED.show();
}
```

Every `addLeds<>` variant also accepts an optional trailing `fl::Bus B = fl::Bus::AUTO` template parameter — see "Compile-Time Bus Selection" below for how to pin the driver from the call site.

### Compile-Time Bus Selection (`fl::Bus`)

The `fl::Bus` enum (in `fl/channels/bus.h`) is the portable identifier that flows through both the templated APIs and the runtime registry overrides. Some portable values map to different concrete vendor drivers on different chips; `deviceInfo<Bus, Which>()` reports the concrete `vendor_name` and `device_name`.

| `fl::Bus::X` | Portable string (`busName(X)`) | Typical concrete driver |
|---|---|---|
| `RMT` | `"RMT"` | ESP32 RMT4/RMT5 |
| `FLEX_IO` | `"FLEX_IO"` | PARLIO, LCD_CAM, I2S, Teensy FlexIO/ObjectFLED |
| `SPI` | `"SPI"` | Platform SPI path |
| `DUAL_SPI` | `"DUAL_SPI"` | Platform dual-SPI path |
| `QUAD_SPI` | `"QUAD_SPI"` | Platform quad-SPI path |
| `OCTAL_SPI` | `"OCTAL_SPI"` | Platform octal-SPI path |
| `UART` | `"UART"` | ESP32 UART / Teensy LPUART |
| `BIT_BANG` | `"BIT_BANG"` | Portable fallback (all platforms) |
| `AUTO` | sentinel - resolves to `DefaultBus<Chipset>::value` for the platform | — |

> Concrete parallel-I/O drivers are still chip-specific. For example, PARLIO is only available on chips that ship Espressif's PARLIO peripheral (`SOC_PARLIO_SUPPORTED=1`). Sketches select that portable slot with `fl::enableDriver<fl::Bus::FLEX_IO, 0>()`; `deviceInfo<fl::Bus::FLEX_IO, 0>()` reports whether the concrete driver is available.

`busName(Bus)` returns the portable string literal. Concrete runtime driver names can differ (`"PARLIO"`, `"LCD_SPI"`, `"OBJECT_FLED"`, etc.); use `busDriverName(Bus, Which, isSpi)` or `deviceInfo<Bus, Which>()` when the concrete name matters.

```cpp
#include "FastLED.h"
#include "fl/channels/bus.h"
#include "fl/channels/config.h"
// Including the per-driver bus_traits.h is the explicit opt-in that links
// the driver translation unit. Without it the templated call fails with
// "implicit instantiation of undefined template BusTraits<Bus::RMT>".
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"

CRGB leds[60];

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> cfg{
        fl::ClocklessChipset(16, timing),
        fl::span<CRGB>(leds, 60), RGB};

    // Compile-time bus pinning via TypedChannel — the single template entry
    // point. Typos like fl::Bus::RTM are compile errors, and the only driver
    // TU linked is RMT.
    auto channel = fl::TypedChannel<fl::Bus::RMT, fl::ClocklessChipset>::create(cfg);
    FastLED.add(channel);
}
```

**Strongly-typed `ChannelConfigOf<Chipset>` (Phase 3b, #2428):** the `TypedChannel<Bus, Chipset>::create(cfg)` template accepts a `ChannelConfigOf<ClocklessChipset>` or `ChannelConfigOf<SpiChipsetConfig>` and `static_asserts` via `BusSupports<B, Chipset>::value` that the chosen bus actually handles the chipset family:

```cpp
fl::ChannelConfigOf<fl::ClocklessChipset> cfg{
    fl::ClocklessChipset(4, fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>()),
    fl::span<CRGB>(leds, 60), GRB};

// AUTO resolves to DefaultBus<ClocklessChipset> per platform.
fl::TypedChannel<fl::Bus::AUTO, fl::ClocklessChipset>::create(cfg);
// Explicit bus selection.
fl::TypedChannel<fl::Bus::RMT, fl::ClocklessChipset>::create(cfg);      // OK
fl::TypedChannel<fl::Bus::DUAL_SPI, fl::ClocklessChipset>::create(cfg);  // compile error
```

`TypedChannel<Bus, Chipset>` lives in `fl/channels/channel_typed.h`. It returns a `ChannelPtr` to the regular non-template runtime `Channel` so callbacks, the draw list, and `ChannelManager` see one channel type.

**`addLeds<>` Bus pinning (#2460):** every `FastLED.addLeds<>` variant accepts an optional trailing `fl::Bus B = fl::Bus::AUTO` template parameter. `B = AUTO` (the default) leaves call sites byte-for-byte unchanged; `B != AUTO` ODR-uses `fl::BusTraits<B>::instance` via `fl::busKeepAlive<B>()` so `--gc-sections` retains the named driver TU.

```cpp
// Clockless: pin to RMT at compile time.
FastLED.addLeds<WS2812, 4, GRB, fl::Bus::RMT>(leds, 60);

// SPI: pin to SPI at compile time.
FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(12), fl::Bus::SPI>(leds, 60);
```

The Bus parameter triggers linker keep-alive in every variant. For the SPI variants on the `FASTLED_SPI_USES_CHANNEL_API` branch, the parameter also populates `cfg.options.mBus = B` so the channel routes through the named driver at runtime. Non-Channel-API controllers (older `ClocklessController` subclasses that pre-date the Channel API) keep their platform-default routing and rely on the linker keep-alive alone — for full runtime routing through a specific Bus, prefer `FastLED.add(cfg)` with `cfg.options.mBus = B`.

### Runtime Bus Selection (non-template `FastLED.add(cfg)`)

`FastLED.add(cfg)` is non-template (#2459). Pick the driver via `cfg.options.mBus`:

- **`cfg.options.mBus = fl::Bus::RMT`** — pin to a specific driver. The dispatch looks up `busName(mBus)` in `ChannelManager`.
- **`cfg.options.mBus = fl::Bus::AUTO`** (the default) — `ChannelManager` picks the highest-priority driver that `canHandle` the chipset.

For **custom / third-party / mock drivers** whose names aren't in the `fl::Bus` enum, register the driver with `manager.addDriver(priority, driver)` and either (a) clear competing drivers first so priority dispatch picks it, or (b) call `manager.setExclusiveDriverByName(name)` for process-wide binding (the by-name escape hatch — `manager.setExclusiveDriver(fl::Bus)` is the typed form for built-in drivers).

The non-template path **auto-enrolls every driver** on the platform (`fl::enableAllDrivers()` runs inside) so any `mBus` value can be dispatched at runtime. A one-time `FL_WARN_ONCE` explains the binary-size trade-off; suppress it with `-DFASTLED_SUPPRESS_RUNTIME_DRIVER_WARNING`. For minimum binary size, use the compile-time `TypedChannel<...>::create()` path above.

```cpp
#include "FastLED.h"

CRGB leds[60];

fl::Bus userPreferredBus();            // declared elsewhere -- reads config / UI

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig cfg(fl::ClocklessChipset(16, timing),
        fl::span<CRGB>(leds, 60), RGB);

    cfg.options.mBus = userPreferredBus();   // data-driven choice
    FastLED.add(cfg);                         // auto-enables all drivers, dispatches by mBus
}
```

If `cfg.options.mBus` names a driver that — for whatever reason — isn't in the manager's registry, `Channel::showPixels` emits a one-shot `FL_ERROR` listing the resolution options (`fl::enableDrivers<fl::Bus::X>()`, `FastLED.enableAllDrivers()`, or the `FastLED.addLeds<..., fl::Bus::X>(...)` shape) and falls back to AUTO/priority dispatch (#2455, #2460).

Passing `fl::Bus::AUTO` (the default) skips the pinning step and lets `ChannelManager` pick by priority — identical to constructing the config without touching `cfg.options.mBus`.

### Opt-In Driver Registration (`enableDrivers<>` / `enableAllDrivers` / `setExclusiveDriver<>`)

**Default behaviour: no driver auto-registration.** Only the platform-default driver TU (named by the legacy clockless controller's Phase 5b pre-bind via `BusTraits<DefaultBus<Chipset>>::instancePtr()`) is linked into the binary; every other driver is `--gc-sections`-eligible until something names its `BusTraits<Bus::X>::instancePtr()`. This is the binary-size fix for #2420 / #2421 — the old `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY` macro has been removed; the default IS the opt-in path.

To register additional drivers at runtime, sketches pick one of three opt-in calls:

```cpp
// 1. Selective opt-in: only RMT and the FLEX_IO slot end up linked AND registered.
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"
#include "platforms/esp/32/drivers/parlio/bus_traits.h"

void setup() {
    fl::enableDrivers<fl::Bus::RMT>();
    fl::enableDriver<fl::Bus::FLEX_IO, 0>();
}
```

```cpp
// 2. Universal opt-in: 3.10.3-style "every driver available at runtime".
#include "FastLED.h"

void setup() {
    FastLED.enableAllDrivers();        // forwards to fl::enableAllDrivers()
    // any cfg.options.mBus value now resolves at runtime via the manager
}
```

> `FastLED.enableAllDrivers()` is defined in `libfastled` (no extra include
> needed). `-Wl,--gc-sections` drops the call graph — every driver TU it
> references — when no sketch calls it, so the opt-in remains zero-cost
> for sketches that don't need it.

```cpp
// 3. Single-driver override: link the named driver AND set it at priority
//    above the platform default so it wins ChannelManager dispatch. Must be
//    called BEFORE addLeds<> / FastLED.add() so the override is visible
//    when channels resolve their drivers.
#include "FastLED.h"
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"

void setup() {
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
    FastLED.addLeds<APA102, 23, 18, RGB>(leds, 60);
}
```

Including the matching per-driver `bus_traits.h` is the explicit opt-in that makes the `BusTraits<Bus::X>` specialization visible at the call site — without that include the call fails to link and `--gc-sections` stays free to drop the driver TU.

---

## Hardware Engine Selection

The system automatically selects the best hardware driver based on platform capabilities:

### Engine Priority

Engines are tried in priority order (highest first) until one accepts the channel. Default priorities live in `fl/channels/bus_priorities.h`; `setDriverPriority(name, n)` overrides them at runtime.

**ESP32 family:**

| Engine | Priority | Platforms | Notes |
|--------|----------|-----------|-------|
| **I2S_SPI** | 10 | ESP32-dev (original) | Native I2S parallel SPI for true SPI chipsets |
| **LCD_SPI** | 10 | ESP32-S3 | LCD_CAM SPI driver for true SPI chipsets |
| **PARLIO** | 4 | ESP32-P4, C6, H2, C5 | Parallel I/O with hardware timing |
| **LCD_RGB** | 3 | ESP32-P4 | LCD RGB peripheral (parallel clockless) |
| **RMT** | 2 (Recommended default) | All ESP32 variants | Reliable, broad chipset support |
| **LCD_CLOCKLESS** | 2 | ESP32-S3 | LCD_CAM clockless (replaces the misnamed I2S) |
| **I2S** | 1 | ESP32-S3 | LCD_CAM via legacy I80 bus (experimental) |
| **SPI** | 0 | ESP32, S2, S3 | DMA-based, deprioritized due to reliability |
| **UART** | -1 | All ESP32 variants | Wave8 encoding (experimental, not recommended) |

**Teensy 4.x:**

| Engine | Priority | Notes |
|--------|----------|-------|
| **FLEX_IO** | 1 | FlexIO2 driver |
| **OBJECT_FLED** | 1 | ObjectFLED driver |

**Portable fallbacks:**

| Engine | Priority | Notes |
|--------|----------|-------|
| **BIT_BANG** | 0 | Cycle-counted GPIO toggling fallback |
| **STUB** | 0 | Native/host/test stub driver |

### Overriding Engine Selection

For testing or performance tuning, you can control driver selection:

```cpp
#include "FastLED.h"
#include "fl/channels/manager.h"   // for fl::ChannelManager::instance().setDriverPriority

CRGB leds[60];

void setup() {
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config(16, timing, fl::span<CRGB>(leds, 60), RGB);
    FastLED.add(config);

    // The three methods below are independent alternatives -- pick one
    // strategy per program. They are shown together for reference only;
    // calling them all in sequence (as written here) makes the later
    // calls override the earlier ones.

    // Method 1a: Link + register a specific driver at priority above the
    //            platform default (compile-time template form — production
    //            opt-in path). Must be called BEFORE the addLeds<>/FastLED.add()
    //            calls that should pick it up.
    //   #include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"  (at file top)
    FastLED.setExclusiveDriver<fl::Bus::RMT>();

    // Method 1b: Same as 1a, but runtime-typed (no TU-link side effect).
    //            Use when the driver is already registered (mocks, custom,
    //            or after FastLED.enableAllDrivers()). Typed, typo-safe —
    //            fl::Bus::RTM is a compile error.
    FastLED.setExclusiveDriver(fl::Bus::RMT);

    // Method 1c: For custom/mock drivers (names not in the fl::Bus enum),
    //            use the by-name escape hatch on the manager directly:
    //   fl::ChannelManager::instance().setExclusiveDriverByName("MOCK_NAME");

    // Method 2: Enable/disable specific already-registered drivers (string
    //           form -- works for drivers that have already been registered
    //           via enableDrivers<> / enableAllDrivers() / a mock test).
    FastLED.setDriverEnabled("PARLIO", true);
    FastLED.setDriverEnabled("SPI", false);

    // Method 3: Adjust driver priority (higher = preferred)
    // Engines are sorted by priority - changing priority triggers re-sort.
    // Note: priority editing lives on the ChannelManager directly -- FastLED
    // does NOT expose a setDriverPriority() forwarder.
    fl::ChannelManager::instance().setDriverPriority("RMT", 9000);     // Increase priority
    fl::ChannelManager::instance().setDriverPriority("PARLIO", 8000);  // Set below RMT

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
- `FastLED.setExclusiveDriver<fl::Bus::X>()` - Link the named driver TU and register it at priority above the platform default (production opt-in path; compile-time TU-link). Must be called before `addLeds<>` / `FastLED.add()`.
- `FastLED.setExclusiveDriver(fl::Bus)` - Runtime-typed: disable all drivers except the named one. Typed, typo-safe (`fl::Bus::RTM` is a compile error). Does NOT link a driver TU — use the template form above for that.
- `fl::ChannelManager::instance().setExclusiveDriverByName(name)` - By-name escape hatch for mocks / custom drivers not in `fl::Bus`. Does NOT link a driver TU.
- `FastLED.setDriverEnabled(name, enabled)` - Enable/disable a specific already-registered driver.
- `fl::ChannelManager::instance().setDriverPriority(name, priority)` - Change priority (triggers automatic re-sort). No `FastLED.*` forwarder is provided for this.

**When to override:**
- Testing different drivers for performance comparison
- Debugging hardware-specific issues
- Forcing a specific driver for known-good behavior
- Prioritizing a custom third-party driver over built-in drivers

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
    events.onChannelCreated.add([](const fl::IChannel& ch) {
        Serial.printf("Channel created: %s\n", ch.name().c_str());
    });

    // Called when channel data is enqueued to driver
    events.onChannelEnqueued.add([](const fl::IChannel& ch, const fl::string& driver) {
        Serial.printf("%s -> %s\n", ch.name().c_str(), driver.c_str());
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
- `onChannelEnqueued` - After data enqueued to driver
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
    // makeClockless<>() carries both bit-period timing AND the UCS7604 encoder
    // selector through to the channel. Use this one-liner for any non-WS2812
    // clockless chipset — the 2-arg ClocklessChipset(pin, timing) form would
    // default the encoder to WS2812.
    fl::ChannelConfig config(fl::makeClockless<fl::TIMING_UCS7604_800KHZ>(2), leds, RGB);

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
    // makeClockless<>() carries the UCS7604 encoder selector with the timing.
    auto warm = FastLED.add(fl::ChannelConfig(
        fl::makeClockless<fl::TIMING_UCS7604_800KHZ>(2), warm_leds, RGB));
    warm->setGamma(2.2f);  // Gentle curve for warm ambiance

    auto cool = FastLED.add(fl::ChannelConfig(
        fl::makeClockless<fl::TIMING_UCS7604_800KHZ>(4), cool_leds, RGB));
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
- Pin assignment, chipset timing, driver binding, channel ID

**Use cases:**
- Web-based LED controllers with live configuration
- User-adjustable color correction and RGB order
- Dynamic LED count/buffer switching

### Per-Channel Bus Pinning (Mixed-Timing Parallel Output)

Bind specific channels to specific drivers via the typed `mBus` field — useful for transmitting different chipset timings in parallel across distinct hardware peripherals:

```cpp
#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

#define NUM_LEDS 100

// Two strips with different chipset timings
CRGB ws2812_strip[NUM_LEDS];
CRGB ws2816_strip[NUM_LEDS];

void setup() {
    // WS2812 strips bound to RMT driver
    fl::ChannelOptions ws2812_opts;
    ws2812_opts.mBus = fl::Bus::RMT;       // typed, preferred (#2459)
    auto timing_ws2812 = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    FastLED.add(fl::ChannelConfig(16, timing_ws2812,
        fl::span<CRGB>(ws2812_strip, NUM_LEDS), RGB, ws2812_opts));

    // WS2816 strips bound to SPI driver (transmits in parallel with RMT)
    fl::ChannelOptions ws2816_opts;
    ws2816_opts.mBus = fl::Bus::SPI;
    auto timing_ws2816 = fl::makeTimingConfig<fl::TIMING_WS2816>();
    FastLED.add(fl::ChannelConfig(18, timing_ws2816,
        fl::span<CRGB>(ws2816_strip, NUM_LEDS), RGB, ws2816_opts));
}

void loop() {
    // Different effects on each strip
    fill_rainbow(ws2812_strip, NUM_LEDS, 0, 255 / NUM_LEDS);
    fill_solid(ws2816_strip, NUM_LEDS, CRGB::Blue);

    FastLED.show();  // Both drivers transmit simultaneously
    delay(20);
}
```

**Use cases:**
- Parallel transmission of different chipset timings (see "Mixing Chipset Timings" below)
- Testing specific driver implementations
- Debugging hardware-specific behavior

**`mBus` is typed.** `cfg.options.mBus` is an `enum class` (#2459), so typos like `fl::Bus::RTM` are compile errors. The canonical driver name is derived via `busName(B)` — string literals never appear at the call site.

**Bus-miss diagnostic (one-shot, from #2456 / #2459 / #2460):** when `cfg.options.mBus != fl::Bus::AUTO` resolves to a driver that isn't registered with `ChannelManager`, the first `Channel::showPixels()` call emits a single `FL_ERROR` and falls back to AUTO/priority dispatch. Subsequent shows on the same channel suppress the warning via `mBusWarned`. The diagnostic uses `ChannelManager::findDriverByName()` (silent lookup) to distinguish two cases:

- Driver isn't registered at all — message lists three remediations: `fl::enableDrivers<fl::Bus::X>()`, `FastLED.enableAllDrivers()`, or `FastLED.addLeds<..., fl::Bus::X>(...)` (pins Bus + triggers linker keep-alive).
- Driver is registered but `canHandle()` rejected the chipset (bus/chipset mismatch) — message suggests picking a different `Bus`.

Use `ChannelManager::findDriverByName(name)` directly when you want to probe the registry without triggering the log; `getDriverByName(name)` is the noisy variant.

---

## Which API to Use

Both APIs are first-class and route through the same `ChannelManager` / driver layer. Pick by call-site shape, not by maturity.

**Channel API (`FastLED.add(cfg)` / `Channel::create(cfg)`):**
- Explicit `ChannelConfig` — chipset, span, RGB order, and `ChannelOptions` are all visible at the call site.
- Mutable at runtime via `Channel::applyConfig()` — good fit for web UIs, MQTT, or any sketch that reconfigures LEDs after `setup()`.
- Returns a `ChannelPtr` you can hold and re-apply.

**Template `addLeds<>` API (`FastLED.addLeds<Chipset, PIN, ...>(...)`):**
- One-line convenience for short sketches.
- Internally constructs a `ChannelConfig`; no behavioral difference from the Channel API.
- Optional trailing `fl::Bus B` template parameter pins the driver and triggers linker keep-alive.

**Prefer the Channel API when:**
- You need runtime reconfiguration (`applyConfig`).
- You want named channels for logging / diagnostics.
- The config is data-driven (read from JSON, UI, network).
- You're mixing chipset timings across multiple drivers in parallel (per-channel `mBus`).

---

## Low-Level Engine API

⚠️ **Advanced users only** - Most users don't need direct driver access. `FastLED.show()` handles everything automatically.

### Mixing Chipset Timings

Engines handle different chipset timings in two modes:

**Sequential (Default)** - Single driver transmits different timings one after another:

```cpp
// Automatic - no configuration needed
FastLED.addLeds<WS2812, 16>(leds1, 60);  // 800kHz timing
FastLED.addLeds<WS2816, 17>(leds2, 60);  // Different timing

FastLED.show();  // Sequential transmission through same driver
```

**Parallel (Explicit)** - Multiple drivers transmit different timings simultaneously (see "Per-Channel Bus Pinning" above).

### Engine States

Hardware drivers use a 4-state machine for non-blocking DMA transmission:

| State | Description | `poll()` return value meaning |
|-------|-------------|-------------------------------|
| **READY** | Idle, ready to accept new data | Hardware is idle, safe to call `show()` |
| **BUSY** | Actively transmitting or queuing channels | Transmission in progress, driver is working |
| **DRAINING** | All channels enqueued, DMA still transmitting | Transmission finishing, no more data needed |
| **ERROR** | Hardware error occurred | Error state, check error message |

**State flow:** READY → `show()` → BUSY → DRAINING → `poll()` → READY

### Non-Blocking API

For advanced CPU/DMA parallelism (e.g., computing next frame while DMA transmits):

```cpp
#include "FastLED.h"
#include "fl/channels/manager.h"

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
    // Get driver from ChannelManager
    auto& manager = fl::ChannelManager::instance();

    // Check if driver is ready for new data
    fl::IChannelDriver::DriverState state = manager.poll();

    if (state == fl::IChannelDriver::DriverState::READY) {
        // Hardware is idle - safe to show next frame
        FastLED.show();
    } else if (state == fl::IChannelDriver::DriverState::DRAINING) {
        // DMA transmission finishing - no more poll() needed this frame
        // Do useful work while waiting
        computeNextFrame();
    } else if (state == fl::IChannelDriver::DriverState::ERROR) {
        Serial.println(state.error.c_str());
    }
    // BUSY state: Keep polling until DRAINING or READY

    delay(20);
}
```

**When to use:**
- High frame rate applications requiring CPU/DMA parallelism
- Custom transmission scheduling across multiple drivers
- Fine-grained control over transmission timing

**Key insight:** DRAINING state signals that the driver doesn't need more `poll()` calls - all channels are enqueued and DMA is finishing transmission. This is the optimal time to compute the next frame.

---

## Implementing a Custom Channel Engine

Third-party developers can create custom channel drivers to support new hardware peripherals or transmission protocols. This section covers the requirements and best practices.

### **Rule: Parallel-IO peripherals — one engine for both clockless and SPI modes**

> **Any parallel-IO peripheral driver (FlexIO, ObjectFLED, ESP32 PARLIO, LCD_CAM, I2S, etc.) must put its SPI mode and its clockless mode in the SAME `ChannelEngine`, unless the hardware truly cannot share the dispatch path.**

Forking into two separate engines (e.g. `ChannelEngineFlexIO` and `ChannelEngineFlexIOSPI`) creates Bus enum proliferation, priority-table juggling, two registrations, two `BusTraits<>` specializations, and two engines competing for the same silicon block. The peripheral itself can only run in one mode at a time, so internal mode-switch logic in `show()` is the architecturally correct shape.

**How to apply:**

```cpp
class ChannelEngineMyPeripheral : public fl::IChannelDriver {
    // Both modes -> one engine returns BOTH caps.
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(/*clockless=*/true, /*spi=*/true);
    }

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override {
        if (!data) return false;
        if (data->isClockless()) return pin_routes_for_clockless(data->getPin());
        if (data->isSpi()) {
            const auto* spi = data->getChipset().ptr<SpiChipsetConfig>();
            return spi && pins_route_for_spi(spi->dataPin, spi->clockPin);
        }
        return false;
    }

    void show() FL_NO_EXCEPT override {
        for (auto& ch : mTransmittingChannels) {
            if (ch->isClockless()) { run_clockless_mode(ch); }
            else if (ch->isSpi())  { run_spi_mode(ch); }
            // Mode switch between channels => reconfigure peripheral
            // (shifter/timer config, DMA TCD, etc.) before transmitting.
        }
    }
};
```

`BusTraits<Bus::X>` registers ONCE. `BusSupports<Bus::X, ClocklessChipset>` AND `BusSupports<Bus::X, SpiChipsetConfig>` both specialize to `fl::true_type`. Sketches selecting `Bus::X` with either chipset type get the right mode automatically.

**Exception (rule does NOT apply):** genuinely separate peripherals — e.g. ESP32 LPSPI vs I2S_SPI are different silicon blocks with completely different register layouts and DMA paths. Two drivers is correct there. The "parallel-IO" qualifier in the rule excludes this case — the rule covers shared-peripheral-different-mode, not different-peripheral-same-protocol.

**Why:** Established 2026-06-27 during the #3428 FlexIO-SPI / ObjectFLED-SPI implementation. The initial design forked into separate FLEX_IO_SPI + OBJECT_FLED_SPI bus slots; the user reverted to the unified pattern because forking made the Bus enum + priority table + registration scaffolding 4× the maintenance burden for zero behavioral benefit. The CodeRabbit ruleset (`.coderabbit.yaml`) flags Bus enum additions that look like a parallel-IO peripheral mode fork.

### Overview

A channel driver bridges the gap between high-level `Channel` objects and low-level hardware. Channels pass their encoded data to drivers via an **ephemeral enqueue** - drivers manage transmission, not channel registration.

**Key responsibilities:**
- Accept ChannelData pointers via `enqueue()` (temporary, per-frame)
- Manage two-queue architecture: **pending** → **in-flight**
- Protect ChannelData with `isInUse` flag during transmission
- Implement 4-state machine: READY → BUSY → DRAINING → READY

### Required Interface: IChannelDriver

Inherit from `fl::IChannelDriver` and implement these methods:

```cpp
#include "fl/channels/driver.h"
#include "fl/channels/data.h"

class MyCustomEngine : public fl::IChannelDriver {
public:
    /// Check if driver can handle this channel type
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

    /// Query driver state and perform maintenance
    DriverState poll() override {
        // Check hardware status
        if (isHardwareBusy()) {
            return DriverState::BUSY;
        }

        if (isTransmitting()) {
            return DriverState::DRAINING;
        }

        // Transmission complete - CRITICAL: Clear isInUse flags
        if (!mTransmittingChannels.empty()) {
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
        }

        return DriverState::READY;
    }

    /// Driver name — matched against `busName(cfg.options.mBus)` for compile-time
    /// or runtime bus pinning. Use UPPER_SNAKE_CASE.
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

The `isInUse` flag prevents channels from modifying their data while the driver is transmitting. **All drivers MUST manage this flag correctly.**

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
      FL_ASSERT(false, "Skipping update - buffer in use by driver");
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

DriverState poll() override {
    if (hardwareComplete()) {
        // Clear in-use AFTER transmission
        for (auto& channel : mTransmittingChannels) {
            channel->setInUse(false);  // ✅ Allow modification
        }
        mTransmittingChannels.clear();
        return DriverState::READY;
    }
    return DriverState::DRAINING;
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
driver->enqueue(data)  →  mEnqueuedChannels.push_back(data)
    ↓
driver->show()         →  Move to mTransmittingChannels, clear mEnqueuedChannels
    ↓
driver->poll()         →  Check hardware status
    ↓
DriverState::READY     →  Clear mTransmittingChannels, ready for next frame
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
- Most drivers skip BUSY (instant transition to DRAINING after `show()`)
- DRAINING signals "all data enqueued, DMA finishing" - optimal time for CPU work
- DRAINING means poll() doesn't need to be called again for current frame
- ERROR requires manual recovery (reset hardware, clear state)

### Registration with ChannelManager

Register your driver with the bus manager to make it available:

```cpp
#include "fl/channels/manager.h"

// In your platform initialization code
void setupCustomEngine() {
    auto driver = fl::make_shared<MyCustomEngine>();

    // Register with priority (higher = preferred). Built-in priorities are
    // defined in `fl/channels/bus_priorities.h` — see the Engine Priority
    // section above. Custom drivers can use any integer priority value.
    //
    // The driver name is obtained via driver->getName() — addDriver() is a
    // 2-arg call. If getName() returns an empty string, addDriver() emits an
    // FL_WARN and rejects the driver.
    fl::ChannelManager::instance().addDriver(10, driver);
}
```

**Priority guidelines for custom drivers:**
- Use a priority above the built-in tier you want to outrank (see `bus_priorities.h`).
- Use negative priorities for low-priority fallbacks.
- Priority values are just integers — no predefined ranges required.

**Driver selection (`ChannelManager::selectDriverForChannel`):**
1. `Channel::showPixels()` derives a bus key from `cfg.options.mBus` via `busName(mBus)` and passes it to `selectDriverForChannel`. When `mBus != Bus::AUTO`, the manager does a silent `findDriverByName(busKey)` lookup first.
   - On hit: that driver is returned (no priority iteration).
   - On miss: `Channel::showPixels()` emits a one-shot `FL_ERROR` (see "Bus-miss diagnostic" above) and falls through to priority dispatch.
2. Otherwise (`mBus == Bus::AUTO` or bus-miss fallback): the manager iterates drivers by priority (high to low) and returns the first that `canHandle()`s the channel data.
3. Callers can override via `cfg.options.mBus` (per-channel, runtime), `fl::TypedChannel<Bus, Chipset>::create()` or `FastLED.addLeds<..., fl::Bus::X>` (compile-time, links only the named driver), or `FastLED.setExclusiveDriver<fl::Bus::X>()` (process-wide, compile-time TU-link; must be called before `addLeds<>`).

**Priority modification:**
- Engines are sorted by priority on registration (via `addDriver()`)
- Priority can be changed at runtime via `setDriverPriority(name, priority)`
- Changing priority triggers automatic re-sort of driver list
- Higher priority drivers are checked first (e.g., priority 10 before priority 2)

### DMA Wait Pattern

**`show()` must wait for READY before starting a new frame.** The correct pattern is a simple spin on `poll()`:

```cpp
void show() override {
    // Wait for previous frame to finish.
    while (poll() != DriverState::READY) {
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
- Return `DriverState::ERROR` on hardware failures
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
- Document hardware requirements in driver header

### Example Engines

Reference implementations in the codebase:

**Simple (good starting point):**
- `src/platforms/esp/32/drivers/uart/channel_engine_uart.cpp.hpp` - UART Wave8 encoding
- `src/platforms/stub/clockless_channel_stub.h` - Stub driver for testing

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
#include "fl/channels/driver.h"
#include "fl/channels/data.h"

FL_TEST_CASE("MyEngine: Basic enqueue and transmission") {
    auto driver = fl::make_shared<MyCustomEngine>();

    // Create test data
    auto data = fl::ChannelData::create(5, timing, fl::move(encodedData));

    // Enqueue
    driver->enqueue(data);

    // Verify isInUse flag lifecycle
    FL_CHECK_FALSE(data->isInUse());  // Not in use before show()

    driver->show();
    FL_CHECK(data->isInUse());  // In use during transmission

    // Poll until complete
    while (driver->poll() != fl::IChannelDriver::DriverState::READY) {
        fl::delayMicroseconds(100);
    }

    FL_CHECK_FALSE(data->isInUse());  // Not in use after transmission
}
```

See `tests/fl/channels/driver.cpp` for more test examples.

---

## Reference

**Headers:**
- `fl/channels/channel.h` — `Channel` class and the non-template `Channel::create(cfg)` factory.
- `fl/channels/channel_typed.h` — `TypedChannel<Bus, Chipset>` (compile-time bus/chipset enforcement, returns a `ChannelPtr` to the same `Channel`).
- `fl/channels/ichannel.h` — `IChannel` ABC (callback-facing identification base).
- `fl/channels/config.h` — `ChannelConfig`, `ChannelConfigOf<Chipset>`, `ClocklessChipset`, `SpiChipsetConfig`.
- `fl/channels/options.h` — `ChannelOptions` (correction, temperature, dither, rgbw, `mBus` driver selection, gamma).
- `fl/channels/bus.h` — `fl::Bus` enum, `busName()`, `DefaultBus<Chipset>`.
- `fl/channels/bus_traits.h` — `BusTraits<B>`, `BusSupports<B, Chipset>`, `enableDrivers<Bus...>()`, `busKeepAlive<B>()`.
- `fl/channels/bus_priorities.h` — `default_bus_priority(Bus)` table consumed by `enableDrivers<>()`.
- `fl/channels/all_drivers.h` — declaration header for `fl::enableAllDrivers()` / `FastLED.enableAllDrivers()`. The body lives in `platforms/channel_drivers.impl.cpp.hpp`, linked into libfastled; `--gc-sections` handles the tree-shaking.
- `fl/channels/manager.h` — `ChannelManager` (`addDriver`, `getDriverByName`, `findDriverByName`, `selectDriverForChannel`, `setDriverPriority`, `setDriverEnabled`, `setExclusiveDriver`, `setExclusiveDriverByName`, `clearAllDrivers`, ...).
- `fl/channels/channel_events.h` — Lifecycle event callbacks.
- `fl/channels/driver.h` — `IChannelDriver` interface and `DriverState` machine.

**Examples:**
- `examples/BlinkParallel.ino` - Parallel LED strip example

## Reducing Binary Size

The compiler emits DWARF `.eh_frame` / FDE unwind tables by default on most
toolchains. On ESP32-S3 release builds this can add ~180 KB even when
`-fno-exceptions` is set. To strip it, add the codegen flags to your
`platformio.ini`:

```ini
build_flags =
    -fno-asynchronous-unwind-tables
    -fno-unwind-tables
```

> **Note:** Earlier releases shipped `FL_NO_UNWIND` / `FL_NO_UNWIND_BEGIN` /
> `FL_NO_UNWIND_END` / `FASTLED_FORCE_NO_UNWIND_TABLES` /
> `FASTLED_FORCE_UNWIND_TABLES` macros that attempted to do this via
> `#pragma GCC optimize("no-unwind-tables")`. A byte-level audit on
> GCC 14.2.0 / xtensa-esp-elf (issue #2473) proved the pragma is a no-op:
> wrapped TUs still shipped the full `.eh_frame`. The macros have been
> removed (issue #2474). Use the `build_flags` form above instead — it
> actually shrinks the binary and also covers `libstdc++.a` and user TUs,
> which the macros could not reach. fbuild#243 will eventually apply
> these flags automatically per-architecture.
