# ESP32 RMT4 Driver (IDF 4.x) - ChannelBusManager Architecture

## Overview

This directory contains the **ChannelBusManager-based RMT4 driver** for ESP32 platforms using ESP-IDF 4.x. It replaces the legacy global-state implementation (`idf4_rmt_impl.cpp`) with a modern, encapsulated architecture that integrates seamlessly with FastLED's unified channel management system.

**Key Features:**
- ✅ **Zero global state** - All state encapsulated in `ChannelEngineRMT4` class
- ✅ **Double-buffer ISR-driven refill** - WiFi interference resistant
- ✅ **Time-multiplexing support** - Handle >8 LED strips via channel sharing
- ✅ **Direct hardware memory access** - High-performance pixel transmission
- ✅ **Polling-based architecture** - Compatible with ChannelBusManager pattern
- ✅ **Identical timing characteristics** - Drop-in replacement for legacy RMT4

---

## Architecture

### Class Structure

```
┌─────────────────────────────────────────────────────────────┐
│                   ChannelBusManager                          │
│  (Singleton - manages all transmission engines)             │
└────────────────────────┬────────────────────────────────────┘
                         │
                         │ registers engine
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                  ChannelEngineRMT4                           │
│  (RMT4 hardware abstraction layer)                          │
│                                                              │
│  • Manages up to 8 RMT channels (platform-dependent)        │
│  • Implements ISR handlers for double-buffer refill         │
│  • Queues pending transmissions (time-multiplexing)         │
│  • Polls for transmission completion                        │
└────────────────────────┬────────────────────────────────────┘
                         │
                         │ uses
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                 ChannelState (per channel)                   │
│  • Hardware configuration (pin, RMT channel)                │
│  • Timing symbols (zero, one)                               │
│  • Double-buffer state (memPtr, whichHalf)                  │
│  • Pixel data reference (owned by ChannelData)              │
│  • Transmission state flags                                 │
└─────────────────────────────────────────────────────────────┘
```

### Component Files

| File | Purpose |
|------|---------|
| `channel_engine_rmt4.h` | Header with class definition, ~260 lines |
| `channel_engine_rmt4.cpp` | Implementation with ISR handlers, ~885 lines |
| `idf4_clockless_rmt_esp32.h` | Strip driver (simplified to ~95 lines) |
| `idf4_rmt_impl.{h,cpp}` | **DEPRECATED** - Legacy global-state implementation |
| `idf4_rmt.{h,cpp}` | **DEPRECATED** - Old RMT controller wrapper |

---

## Migration from Legacy RMT4

### What Changed

| Legacy (idf4_rmt_impl.cpp) | New (ChannelEngineRMT4) |
|----------------------------|-------------------------|
| `gControllers[]` global array | `mChannels` member vector |
| `gOnChannel[]` lookup table | `findChannelByNumber()` method |
| `gTX_sem` semaphore blocking | `pollDerived()` polling |
| `gRMT_intr_handle` global | `mRMT_intr_handle` member |
| `rmt_spinlock` global | `mRmtSpinlock` member |
| `ESP32RMTController` class | Replaced by `ChannelState` struct |
| Manual ISR registration per controller | Single ISR for all channels |
| Semaphore-based wait | Non-blocking enqueue/poll pattern |

### Key Architectural Improvements

1. **Encapsulation**
   - All global state moved into `ChannelEngineRMT4` class
   - No static variables or global mutexes
   - Multiple engine instances possible (though not used)

2. **Lifecycle Management**
   - Constructor registers ISR, destructor cleans up all resources
   - Channels reused across transmissions (fast reacquisition)
   - RAII pattern prevents resource leaks

3. **Separation of Concerns**
   - Strip driver (`idf4_clockless_rmt_esp32.h`) only encodes pixels
   - Engine handles all transmission, ISR logic, and hardware management
   - Clear interface via `ChannelEngine` base class

4. **Time-Multiplexing**
   - Pending queue allows >8 strips with automatic channel reuse
   - Channels released immediately after completion
   - Next pending strip starts as soon as channel available

---

## Double-Buffer Mechanism

The RMT4 driver uses a **double-buffer refill strategy** to achieve glitch-free LED updates even under WiFi interference:

```
RMT Hardware Memory (2 memory blocks = 128 words = 256 bytes)
┌──────────────────┬──────────────────┐
│   Buffer Half 0  │   Buffer Half 1  │
│   (64 words)     │   (64 words)     │
└──────────────────┴──────────────────┘
        ▲                    ▲
        │                    │
   Hardware reads      ISR refills
   (currently sending)  (next data)
```

**Flow:**
1. Initial fill: ISR fills both halves before transmission starts
2. Hardware reads from half 0, sends to LEDs
3. When half 0 depleted, **threshold interrupt** fires
4. ISR refills half 0 with next data (while hardware reads half 1)
5. Process repeats until all pixel data transmitted
6. **TX done interrupt** fires when complete

**WiFi Interference Detection:**
- ISR measures time between refills (`lastFill` timestamp)
- If time > 1.5× expected, WiFi interrupt likely delayed ISR
- Abort transmission to prevent LED glitches (preferable to garbled colors)

---

## ISR Safety

All interrupt handlers are marked `IRAM_ATTR` to ensure flash-safe execution:

| Function | Purpose | IRAM? |
|----------|---------|-------|
| `handleInterrupt()` | Main ISR dispatcher | ✅ |
| `onThresholdInterrupt()` | Half-buffer refill trigger | ✅ |
| `onTxDoneInterrupt()` | Transmission complete handler | ✅ |
| `fillNextBuffer()` | Double-buffer refill logic | ✅ |
| `convertByteToRMT()` | Byte → 8 RMT symbols | ✅ |
| `tx_start()` | Hardware kickoff | ✅ |

**Critical Sections:**
- Spinlock (`mRmtSpinlock`) protects RMT register access
- `portENTER_CRITICAL_ISR` used in ISR context
- `portENTER_CRITICAL` used in main thread context

**Volatile Variables:**
- `transmissionComplete` - ISR sets, main thread reads
- `memPtr` - ISR advances during buffer fill
- `pixelDataPos` - ISR increments as data consumed
- `lastFill` - ISR updates for WiFi interference detection

---

## Platform Support

| Platform | RMT Channels | Memory Blocks | IDF Version | Status |
|----------|--------------|---------------|-------------|--------|
| ESP32 (original) | 8 | 64 words/channel | IDF 4.x, 5.x | ✅ Tested |
| ESP32-S2 | 4 | 64 words/channel | IDF 4.x, 5.x | ✅ Tested |
| ESP32-S3 | 4 | 48 words/channel | IDF 4.x, 5.x | ✅ Tested |
| ESP32-C3 | 2 | 48 words/channel | IDF 4.x, 5.x | ⚠️ Untested |
| ESP32-C6 | 2 | 48 words/channel | IDF 5.x only | ⚠️ Untested |
| ESP32-H2 | 2 | 48 words/channel | IDF 5.x only | ⚠️ Untested |

**Conditional Compilation:**
- RMT4 driver used when `FASTLED_RMT5=0` (IDF 4.x default)
- RMT5 driver used when `FASTLED_RMT5=1` (IDF 5.x with new RMT API)
- Both drivers are mutually exclusive (compile-time selection)

---

## Configuration Macros

### Memory Configuration

```cpp
// Memory blocks per channel (default: 2 = 128 words)
#define FASTLED_RMT_MEM_BLOCKS 2

// Words per channel (auto-detected from platform)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 64  // ESP32, ESP32-S2
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 48  // ESP32-S3, C3, C6, H2
```

### Timing Configuration

```cpp
// Clock divider (2 = 40MHz RMT clock on 80MHz APB)
#define DIVIDER_RMT4 2

// Total buffer capacity
#define MAX_PULSES_RMT4 (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)

// Half-buffer size for double-buffering
#define PULSES_PER_FILL_RMT4 (MAX_PULSES_RMT4 / 2)
```

### Platform-Specific Channels

```cpp
// Max TX channels (auto-detected from platform)
#define FASTLED_RMT_MAX_CHANNELS 8  // ESP32
#define FASTLED_RMT_MAX_CHANNELS 4  // ESP32-S2, S3
#define FASTLED_RMT_MAX_CHANNELS 2  // ESP32-C3, C6, H2
```

---

## Usage Example

```cpp
#include <FastLED.h>

#define NUM_LEDS 100
#define DATA_PIN 5

CRGB leds[NUM_LEDS];

void setup() {
    // Automatically uses ChannelEngineRMT4 on IDF 4.x platforms
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();  // Non-blocking enqueue + polling
    delay(20);
}
```

**Multi-Strip Example:**

```cpp
// More than 8 strips: Time-multiplexing automatically enabled
FastLED.addLeds<WS2812, 5, GRB>(leds1, 100);
FastLED.addLeds<WS2812, 6, GRB>(leds2, 100);
// ... (up to 16 strips queued)
FastLED.addLeds<WS2812, 21, GRB>(leds10, 100);

FastLED.show();  // Queues all 10 strips, sends 8 at once, remaining 2 wait
```

---

## Performance Characteristics

### Timing Accuracy

| Metric | Value | Notes |
|--------|-------|-------|
| RMT Clock | 40 MHz | With `DIVIDER_RMT4=2` on 80MHz APB |
| Timing Resolution | 25 ns | 1 RMT cycle = 25 nanoseconds |
| WS2812 T0H | 400 ns | ±150ns tolerance |
| WS2812 T0L | 850 ns | ±150ns tolerance |
| WS2812 T1H | 800 ns | ±150ns tolerance |
| WS2812 T1L | 450 ns | ±150ns tolerance |

### Throughput

| Configuration | Throughput | Notes |
|---------------|------------|-------|
| Single strip | ~800 kHz | Hardware-limited, no software overhead |
| 8 concurrent strips | ~6.4 MHz aggregate | Full hardware utilization |
| >8 strips (time-mux) | ~800 kHz per strip | Sequential transmission |

### ISR Latency

| Interrupt Type | Max Latency | Notes |
|----------------|-------------|-------|
| Threshold (refill) | <10 µs | Critical path, must complete before next half |
| TX Done | <5 µs | Non-critical, just sets flag |
| WiFi Interference | Detected if >1.5× expected | Aborts to prevent glitches |

---

## Debugging

### Enable Debug Output

```cpp
// Before including FastLED.h
#define FASTLED_DEBUG_ENABLED 1
#include <FastLED.h>
```

### Debug Messages

The driver emits `FL_LOG_RMT()` messages for key events:
- Channel acquisition: `"acquireChannel: Reusing channel N for pin P"`
- Transmission start: `"startTransmission: Started channel N on pin P, X bytes"`
- Completion: `"pollDerived: Channel N completed"`
- Time-multiplexing: `"processPendingChannels: All N channels busy, deferring X pending strips"`

### Common Issues

**Problem:** LEDs flicker or show wrong colors
- **Cause:** WiFi interference delaying ISR refills
- **Solution:** Increase `FASTLED_RMT_MEM_BLOCKS` to 3 or 4 (more buffering)

**Problem:** Only first 8 strips work
- **Cause:** Time-multiplexing not enabled or polling not called
- **Solution:** Ensure `FastLED.show()` returns quickly and is called in loop

**Problem:** Compilation errors about `ChannelDataPtr`
- **Cause:** Old FastLED version without ChannelBusManager support
- **Solution:** Update to FastLED 3.7+ or rebuild from latest source

---

## Performance Comparison

### Before (Legacy RMT4)

```cpp
// Old implementation (idf4_rmt_impl.cpp)
- 800+ lines of implementation
- 10 global variables
- Semaphore blocking on FastLED.show()
- Manual channel management per strip
- Complex ISR state tracking
```

### After (ChannelEngineRMT4)

```cpp
// New implementation (channel_engine_rmt4.cpp)
- 885 lines (includes extensive documentation)
- Zero global variables
- Non-blocking enqueue/poll pattern
- Unified channel management
- Clean ISR dispatch via class methods
```

**Strip Driver Simplification:**

| Metric | Legacy | New | Change |
|--------|--------|-----|--------|
| Lines of code | ~120 | ~95 | -21% |
| Responsibilities | Encode + transmit + manage | Encode only | Simplified |
| RMT-specific code | ~60 lines | 0 lines | -100% |
| Global dependencies | 3 | 0 | -100% |

---

## Future Work

### Planned Improvements

1. **DMA Support** - Offload buffer refill to DMA controller (ESP32-S3+)
2. **Adaptive Timing** - Auto-tune timing based on measured LED response
3. **Multi-Channel Sync** - Synchronized start for multiple strips
4. **Power Management** - Integrate with ESP32 power-saving modes

### Migration to RMT5

For users on ESP-IDF 5.x, consider migrating to `ChannelEngineRMT` (RMT5 driver):
- Simpler API (no manual memory management)
- Better interrupt handling
- Official ESP-IDF support

Set `FASTLED_RMT5=1` in platformio.ini or build flags.

---

## References

- [ESP32 RMT Peripheral Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html)
- [FastLED ChannelBusManager Architecture](../../../../fl/channels/README.md)
- [WS2812 Timing Specification](https://cdn-shop.adafruit.com/datasheets/WS2812.pdf)

---

## Credits

**Original RMT4 Implementation:** FastLED contributors (idf4_rmt_impl.cpp)
**ChannelBusManager Migration:** FastLED core team (2024)
**Architecture Reference:** RMT5 implementation (channel_engine_rmt.cpp)

---

**Last Updated:** 2024-11-21
**Tested With:** ESP-IDF 4.4.x, 5.3.x, 5.4.x
**Status:** Production-ready, backward-compatible with legacy RMT4
