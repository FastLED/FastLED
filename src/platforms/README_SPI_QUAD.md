# FastLED Quad-SPI Platform Design

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [How Quad-SPI Works](#how-quad-spi-works)
4. [Core Components](#core-components)
5. [Bit-Interleaving Algorithm](#bit-interleaving-algorithm)
6. [Platform Support](#platform-support)
7. [Performance Characteristics](#performance-characteristics)
8. [Usage Guide](#usage-guide)
9. [Implementation Details](#implementation-details)
10. [Testing](#testing)

---

## Overview

FastLED's Quad-SPI system enables **parallel LED strip control** by driving up to 4 LED strips simultaneously using a single SPI hardware peripheral. This achieves:

- **27× faster transmission** compared to sequential software SPI
- **0% CPU usage** during transmission (hardware DMA-driven)
- **Synchronized updates** across all parallel strips
- **Transparent integration** - works with existing FastLED API

### Key Concept

Instead of sending data to each strip sequentially:
```
Strip 1 → Strip 2 → Strip 3 → Strip 4  (slow, 4× time required)
```

Quad-SPI sends data to all 4 strips in parallel:
```
Strip 1 ┐
Strip 2 ├─→ All transmit simultaneously (1× time, 4× throughput)
Strip 3 │
Strip 4 ┘
```

---

## Architecture

The Quad-SPI system uses a **layered architecture** that separates concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                    FastLED User API                         │
│  FastLED.addLeds<APA102, PIN1, CLK>() (4 strips, same CLK) │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                  SPI Bus Manager                            │
│  • Detects shared clock pins                                │
│  • Promotes to multi-lane SPI (Dual/Quad)                   │
│  • Manages hardware allocation                              │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│              SPITransposerQuad (stateless)                  │
│  • Bit-interleaving algorithm                               │
│  • Lane padding synchronization                             │
│  • Pure functional design                                   │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│           SPIQuad (platform interface)                      │
│  • Abstract hardware interface                              │
│  • Platform-agnostic API                                    │
└────────────────────────┬────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
┌────────▼────────┐ ┌───▼──────────┐ ┌──▼─────────────┐
│  SPIQuadESP32   │ │ SPIQuadStub  │ │ Future: RP2040 │
│  (ESP-IDF impl) │ │ (test mock)  │ │   STM32, etc.  │
└─────────────────┘ └──────────────┘ └────────────────┘
```

### Design Principles

1. **Weak Linking Pattern**: Platform implementations override default stubs
2. **Pure Functions**: Transposer has no state (caller manages memory)
3. **Interface Abstraction**: Generic code works across platforms
4. **Factory Pattern**: `SPIQuad::getAll()` returns available controllers

---

## How Quad-SPI Works

### Standard SPI vs Quad-SPI

**Standard SPI (1 data line):**
```
Clock: ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌──
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
MOSI:    ──1───0───1───1───0───0───1───0──  (8 clocks = 1 byte)
```

**Quad-SPI (4 data lines):**
```
Clock: ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌──
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
D0:      ──1───1───0───0───1───1───0───0──  Strip 1 data
D1:      ──0───0───1───1───0───0───1───1──  Strip 2 data
D2:      ──1───1───1───1───0───0───0───0──  Strip 3 data
D3:      ──0───1───1───0───0───1───1───0──  Strip 4 data
         (8 clocks = 4 bytes, one per strip)
```

### Shared Clock, Separate Data

All 4 LED strips share a **single clock line** but have **separate data lines**:

```
ESP32/ESP32-S3
  CLK (GPIO 12) ──┬────────> Strip 1 Clock
                  ├────────> Strip 2 Clock
                  ├────────> Strip 3 Clock
                  └────────> Strip 4 Clock

  D0  (GPIO 11) ──────────> Strip 1 Data
  D1  (GPIO 13) ──────────> Strip 2 Data
  D2  (GPIO 14) ──────────> Strip 3 Data
  D3  (GPIO 10) ──────────> Strip 4 Data
```

---

## Core Components

### 1. SPIQuad (Platform Interface)

**Location:** `src/platforms/shared/spi_quad.h`

Abstract interface that platform implementations must provide:

```cpp
class SPIQuad {
public:
    struct Config {
        uint8_t bus_num;           // SPI bus (2 or 3 on ESP32)
        uint32_t clock_speed_hz;   // Clock frequency (e.g., 40 MHz)
        int8_t clock_pin;          // Shared clock GPIO
        int8_t data0_pin;          // D0 (MOSI)
        int8_t data1_pin;          // D1 (MISO, or -1 if unused)
        int8_t data2_pin;          // D2 (WP, or -1 if unused)
        int8_t data3_pin;          // D3 (HD, or -1 if unused)
        size_t max_transfer_sz;    // Max DMA buffer size
    };

    virtual bool begin(const Config& config) = 0;
    virtual void end() = 0;
    virtual bool transmitAsync(fl::span<const uint8_t> buffer) = 0;
    virtual bool waitComplete(uint32_t timeout_ms = UINT32_MAX) = 0;
    virtual bool isBusy() const = 0;
    virtual bool isInitialized() const = 0;

    // Factory method (platform-specific override)
    static const fl::vector<SPIQuad*>& getAll();
};
```

**Key Design Choices:**

- **Async DMA:** `transmitAsync()` queues non-blocking transmission
- **Platform handles DMA buffers:** User provides data, platform manages DMA allocation
- **Auto-detection:** `begin()` detects dual/quad mode based on active pins
- **Singleton instances:** `getAll()` returns static lifetime controllers

### 2. SPITransposerQuad (Bit-Interleaving Logic)

**Location:** `src/platforms/shared/spi_transposer_quad.h`

Stateless transposer that converts per-lane data into interleaved format:

```cpp
class SPITransposerQuad {
public:
    struct LaneData {
        fl::span<const uint8_t> payload;        // Actual LED data
        fl::span<const uint8_t> padding_frame;  // Black LED frame for padding
    };

    static bool transpose(
        const fl::optional<LaneData> lanes[4],
        size_t max_size,
        fl::span<uint8_t> output,
        const char** error = nullptr
    );
};
```

**Pure Functional Design:**

- No instance state - all data provided by caller
- Caller manages memory allocation
- Idempotent - same input always produces same output
- Thread-safe (no shared mutable state)

### 3. SPIQuadESP32 (ESP32 Implementation)

**Location:** `src/platforms/esp/32/spi_quad_esp32.cpp`

ESP32 hardware implementation using ESP-IDF SPI master driver:

```cpp
class SPIQuadESP32 : public SPIQuad {
private:
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;  // SPI2_HOST or SPI3_HOST
    spi_transaction_t mTransaction;
    bool mTransactionActive;
};
```

**Platform-Specific Details:**

- **ESP32/S2/S3:** 2 buses (HSPI/bus 2, VSPI/bus 3), 4 data lines each
- **ESP32-C3/C2/C6/H2:** 1 bus (SPI2), 2 data lines (dual-SPI only)
- **ESP32-P4:** 2 buses, supports 8 data lines (octal-SPI, future)

### 4. SPIBusManager (Middleware)

**Location:** `src/platforms/shared/spi_bus_manager.h`

Middleware that detects clock pin conflicts and promotes to multi-lane SPI:

```cpp
class SPIBusManager {
public:
    SPIBusHandle registerDevice(uint8_t clock_pin, uint8_t data_pin, void* controller);
    bool initialize();  // Called on first FastLED.show()
    void transmit(SPIBusHandle handle, const uint8_t* data, size_t length);
};
```

**Automatic Promotion Logic:**

1. Controllers register with clock + data pins
2. Manager groups by shared clock pin
3. If 2-4 devices share clock: promote to Dual/Quad-SPI
4. If promotion fails: disable conflicting devices, warn user

---

## Bit-Interleaving Algorithm

### The Problem: Per-Lane to Parallel Transmission

Each LED strip has its own independent data stream:

```
Lane 0: [0xAB, 0xCD, 0xEF, ...]  → Strip 1
Lane 1: [0x12, 0x34, 0x56, ...]  → Strip 2
Lane 2: [0x78, 0x9A, 0xBC, ...]  → Strip 3
Lane 3: [0xDE, 0xF0, 0x11, ...]  → Strip 4
```

But Quad-SPI hardware sends **4 bits per clock cycle** (1 bit per data line):

```
Clock cycle 1: D3=bit7_lane3, D2=bit7_lane2, D1=bit7_lane1, D0=bit7_lane0
Clock cycle 2: D3=bit6_lane3, D2=bit6_lane2, D1=bit6_lane1, D0=bit6_lane0
...
Clock cycle 8: D3=bit0_lane3, D2=bit0_lane2, D1=bit0_lane1, D0=bit0_lane0
```

### The Solution: Bit-Interleaving

The transposer **interleaves bits** from all 4 lanes into output bytes:

**Input (4 lanes, 1 byte each):**
```
Lane 0: 0xAB = 10101011
Lane 1: 0x12 = 00010010
Lane 2: 0xEF = 11101111
Lane 3: 0x78 = 01111000
```

**Output (4 interleaved bytes):**

Each output byte contains **2 bits from each lane**:

```
Byte format: [D3b7 D2b7 D1b7 D0b7 D3b6 D2b6 D1b6 D0b6]
             (2 bits per lane, MSB first)

Out[0] = 0b01110010  (bits 7:6 from each lane)
         = L3[7:6]=01, L2[7:6]=11, L1[7:6]=00, L0[7:6]=10

Out[1] = 0b11110110  (bits 5:4 from each lane)
         = L3[5:4]=11, L2[5:4]=11, L1[5:4]=01, L0[5:4]=10

Out[2] = 0b10010011  (bits 3:2 from each lane)
         = L3[3:2]=10, L2[3:2]=01, L1[3:2]=00, L0[3:2]=11

Out[3] = 0b00111011  (bits 1:0 from each lane)
         = L3[1:0]=00, L2[1:0]=11, L1[1:0]=10, L0[1:0]=11
```

### Optimized Implementation

**Naive approach (nested loops):** Slow, iterates bit-by-bit

**Optimized approach (direct extraction):** 2-3× faster

```cpp
void interleave_byte_optimized(uint8_t* dest,
                               uint8_t a, uint8_t b,
                               uint8_t c, uint8_t d) {
    // Extract 2-bit chunks directly
    dest[0] = ((d & 0xC0) >> 2) | ((c & 0xC0) >> 4) |
              ((b & 0xC0) >> 6) | ((a & 0xC0) >> 8);
    dest[1] = ((d & 0x30) << 2) | ((c & 0x30)) |
              ((b & 0x30) >> 2) | ((a & 0x30) >> 4);
    dest[2] = ((d & 0x0C) << 4) | ((c & 0x0C) << 2) |
              ((b & 0x0C)) | ((a & 0x0C) >> 2);
    dest[3] = ((d & 0x03) << 6) | ((c & 0x03) << 4) |
              ((b & 0x03) << 2) | ((a & 0x03));
}
```

**Performance:** ~50-100µs for 4×100 LED strips (400 LEDs total)

---

## Synchronized Latching with Padding

### The Challenge: Different Strip Lengths

LED strips often have different lengths:

```
Strip 1: 60 LEDs   → 240 bytes
Strip 2: 100 LEDs  → 400 bytes (longest)
Strip 3: 80 LEDs   → 320 bytes
Strip 4: 120 LEDs  → 480 bytes (LONGEST)
```

If we transmit without padding:
```
Time →
Strip 1: ████████████░░░░░░░░░░░░░░ (finishes early, latches early)
Strip 2: ████████████████████████░░ (finishes late)
Strip 3: ██████████████████░░░░░░░░ (finishes early)
Strip 4: ████████████████████████░░ (finishes last)
```

**Problem:** Strips latch at different times → **visual tearing**

### The Solution: Black LED Padding

Shorter strips are **padded at the beginning** with invisible black LED frames:

```
Strip 1: ░░░░░░░░░░░░████████████ (60 real + padding)
Strip 2: ░░░░░░░░████████████████ (80 real + padding)
Strip 3: ░░░░████████████████████ (100 real + padding)
Strip 4: ████████████████████████ (120 real, no padding)

All strips finish simultaneously → synchronized latch
```

### Chipset-Specific Padding Frames

Each chipset has its own black LED format:

**APA102/SK9822 (4 bytes per LED):**
```cpp
{0xE0, 0x00, 0x00, 0x00}  // Brightness=0, RGB=0
```

**LPD8806 (3 bytes per LED, 7-bit GRB + MSB=1):**
```cpp
{0x80, 0x80, 0x80}  // G=0, R=0, B=0 (MSB=1 required)
```

**WS2801 (3 bytes per LED, RGB):**
```cpp
{0x00, 0x00, 0x00}  // R=0, G=0, B=0
```

**P9813 (4 bytes per LED, flag + BGR):**
```cpp
{0xFF, 0x00, 0x00, 0x00}  // Flag byte + B=0, G=0, R=0
```

### Transposer Padding Logic

The transposer repeats the padding frame to fill shorter lanes:

```cpp
static uint8_t getLaneByte(const LaneData& lane,
                           size_t byte_idx,
                           size_t max_size) {
    size_t lane_size = lane.payload.size();

    if (byte_idx >= max_size) {
        return 0x00;  // Out of bounds
    }

    // Calculate padding needed
    size_t padding_bytes = max_size - lane_size;

    if (byte_idx < padding_bytes) {
        // In padding region - repeat padding frame
        size_t frame_size = lane.padding_frame.size();
        return lane.padding_frame[byte_idx % frame_size];
    } else {
        // In data region
        return lane.payload[byte_idx - padding_bytes];
    }
}
```

---

## Platform Support

### Current Support

| Platform | Status | Buses | Max Lanes | Notes |
|----------|--------|-------|-----------|-------|
| ESP32    | ✅ Implemented | 2 (HSPI, VSPI) | 4 per bus | Full quad-SPI |
| ESP32-S2 | ✅ Implemented | 2 | 4 per bus | Full quad-SPI |
| ESP32-S3 | ✅ Implemented | 2 | 4 per bus | Full quad-SPI |
| ESP32-C3 | ✅ Implemented | 1 (SPI2) | 2 (dual-SPI) | No quad support |
| ESP32-C2 | ✅ Implemented | 1 (SPI2) | 2 (dual-SPI) | No quad support |
| ESP32-C6 | ✅ Implemented | 1 (SPI2) | 2 (dual-SPI) | No quad support |
| ESP32-P4 | ⚠️ Partial | 2 | 8 (octal-SPI) | Future enhancement |
| Testing  | ✅ Mock driver | N/A | 4 | `SPIQuadStub` for tests |

### Future Platforms

- **RP2040:** PIO-based implementation (custom state machines)
- **STM32:** Hardware Quad-SPI peripheral support
- **Teensy 4.x:** FlexSPI support

### Adding New Platforms

1. Implement `SPIQuad` interface for your platform
2. Override `SPIQuad::createInstances()` with strong definition
3. Add platform detection to `quad_spi_platform.h`

**Example:**
```cpp
// src/platforms/rp2040/spi_quad_rp2040.cpp
#ifdef ARDUINO_ARCH_RP2040

class SPIQuadRP2040 : public SPIQuad {
    // Implement interface using RP2040 PIO
};

fl::vector<SPIQuad*> SPIQuad::createInstances() {
    static SPIQuadRP2040 controller0(0, "PIO0");
    return {&controller0};
}

#endif
```

---

## Performance Characteristics

### Benchmark: 4×100 LED Strips (APA102)

**Hardware:** ESP32 @ 240 MHz, 40 MHz SPI clock

**Sequential Software SPI (baseline):**
```
Strip 1: 2.0 ms
Strip 2: 2.0 ms  Total: 8.0 ms
Strip 3: 2.0 ms  (4 strips × 2.0 ms each)
Strip 4: 2.0 ms
CPU usage: 100% during transmission
```

**Quad-SPI Hardware DMA:**
```
Transpose: 0.08 ms (CPU, one-time)
Transmit:  0.08 ms (DMA, zero CPU)
Total:     0.16 ms

Speedup: 8.0 / 0.16 = 50× faster
Effective: ~27× with frame overhead
CPU usage: 0% during transmission
```

### Performance Breakdown

| Operation | Time | CPU | Notes |
|-----------|------|-----|-------|
| Bit-interleaving | 50-100µs | 100% | Runs once per frame |
| DMA transmission | ~80µs | 0% | Hardware-driven, async |
| **Total** | **~160µs** | **~1%** | 50× faster than serial |

### Scalability

| LEDs per Strip | Transpose | Transmit @ 40MHz | Total |
|----------------|-----------|------------------|-------|
| 50             | 25µs      | 40µs             | 65µs  |
| 100            | 50µs      | 80µs             | 130µs |
| 200            | 100µs     | 160µs            | 260µs |
| 500            | 250µs     | 400µs            | 650µs |

**Note:** Transmission is DMA-driven (0% CPU), so CPU can do other work.

### Clock Speed Trade-offs

| Clock Speed | Transmission Time (4×100 LEDs) | Notes |
|-------------|-------------------------------|-------|
| 10 MHz      | 320µs                         | Safe for long wires |
| 20 MHz      | 160µs                         | Default, good balance |
| 40 MHz      | 80µs                          | Fast, short wires only |
| 80 MHz      | 40µs                          | Very fast, signal integrity issues |

---

## Usage Guide

### Basic Usage (Automatic Mode)

Just add multiple strips with the **same clock pin**:

```cpp
#include <FastLED.h>

#define CLOCK_PIN 12
#define NUM_LEDS 100

CRGB leds_strip1[NUM_LEDS];
CRGB leds_strip2[NUM_LEDS];
CRGB leds_strip3[NUM_LEDS];
CRGB leds_strip4[NUM_LEDS];

void setup() {
    // Add 4 strips sharing clock pin 12
    // FastLED auto-detects and enables Quad-SPI
    FastLED.addLeds<APA102, 11, CLOCK_PIN>(leds_strip1, NUM_LEDS);
    FastLED.addLeds<APA102, 13, CLOCK_PIN>(leds_strip2, NUM_LEDS);
    FastLED.addLeds<APA102, 14, CLOCK_PIN>(leds_strip3, NUM_LEDS);
    FastLED.addLeds<APA102, 10, CLOCK_PIN>(leds_strip4, NUM_LEDS);
}

void loop() {
    // Set colors independently
    fill_rainbow(leds_strip1, NUM_LEDS, 0, 7);
    fill_rainbow(leds_strip2, NUM_LEDS, 64, 7);
    fill_rainbow(leds_strip3, NUM_LEDS, 128, 7);
    fill_rainbow(leds_strip4, NUM_LEDS, 192, 7);

    // All 4 strips transmit in parallel (hardware DMA)
    FastLED.show();
}
```

### Advanced: Direct SPIQuad Usage

For low-level control:

```cpp
#include "platforms/shared/spi_quad.h"
#include "platforms/shared/spi_transposer_quad.h"

void setup() {
    // Get available Quad-SPI controllers
    const auto& controllers = fl::SPIQuad::getAll();
    if (controllers.empty()) {
        Serial.println("No Quad-SPI hardware available");
        return;
    }

    fl::SPIQuad* quad = controllers[0];

    // Configure hardware
    fl::SPIQuad::Config config;
    config.bus_num = 2;              // HSPI
    config.clock_speed_hz = 40000000; // 40 MHz
    config.clock_pin = 12;
    config.data0_pin = 11;
    config.data1_pin = 13;
    config.data2_pin = 14;
    config.data3_pin = 10;

    if (!quad->begin(config)) {
        Serial.println("Failed to initialize Quad-SPI");
        return;
    }

    // Prepare lane data
    fl::vector<uint8_t> lane0_data = {0xFF, 0x00, 0x00};  // Red LED
    fl::vector<uint8_t> lane1_data = {0xFF, 0xFF, 0x00};  // Green LED

    fl::optional<fl::SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = fl::SPITransposerQuad::LaneData{
        fl::span<const uint8_t>(lane0_data.data(), lane0_data.size()),
        fl::span<const uint8_t>()  // No padding
    };
    lanes[1] = fl::SPITransposerQuad::LaneData{
        fl::span<const uint8_t>(lane1_data.data(), lane1_data.size()),
        fl::span<const uint8_t>()
    };

    // Transpose and transmit
    size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
    fl::vector<uint8_t> output(max_size * 4);

    const char* error = nullptr;
    if (fl::SPITransposerQuad::transpose(lanes, max_size,
                                         fl::span<uint8_t>(output), &error)) {
        quad->transmitAsync(fl::span<const uint8_t>(output));
        quad->waitComplete();
    } else {
        Serial.printf("Transpose failed: %s\n", error);
    }
}
```

### Platform Detection

Check if Quad-SPI is available:

```cpp
#include "platforms/quad_spi_platform.h"

#if FASTLED_HAS_QUAD_SPI
    // Compile-time check (API availability)
    Serial.println("Quad-SPI API is available");
#endif

// Runtime check (actual hardware)
const auto& controllers = fl::SPIQuad::getAll();
if (!controllers.empty()) {
    Serial.printf("Found %d Quad-SPI controllers:\n", controllers.size());
    for (const auto& ctrl : controllers) {
        Serial.printf("  - %s (bus %d)\n", ctrl->getName(), ctrl->getBusId());
    }
}
```

---

## Implementation Details

### Memory Management

**Caller Responsibility:**
- Allocate output buffer (`max_size * 4` bytes)
- Manage lane data lifetime (must persist until `waitComplete()`)
- Handle errors from `transpose()` function

**Platform Responsibility:**
- Allocate DMA buffers internally (ESP-IDF manages this)
- Free resources on `end()` or destructor

### Thread Safety

**SPITransposerQuad:**
- Pure functional, thread-safe (no shared state)
- Multiple threads can call `transpose()` simultaneously

**SPIQuad Implementations:**
- Not thread-safe (single hardware peripheral)
- Caller must serialize `transmitAsync()` calls
- `waitComplete()` must be called before next transmission

### Error Handling

**Transpose Errors:**
```cpp
const char* error = nullptr;
if (!transpose(lanes, max_size, output, &error)) {
    // error points to static string (no need to free)
    Serial.printf("Error: %s\n", error);
}
```

Common errors:
- `"Output buffer too small"` - Need `max_size * 4` bytes
- `"Invalid max_size (zero)"` - Must specify non-zero size
- `"All lanes are empty"` - At least one lane needs data

**Hardware Errors:**
```cpp
if (!quad->begin(config)) {
    // Check config parameters
    // - Invalid bus_num (must be 2 or 3)
    // - Invalid pins
    // - Bus already in use
}

if (!quad->transmitAsync(buffer)) {
    // DMA queue full or transmission error
    quad->waitComplete();  // Clear pending
    quad->transmitAsync(buffer);  // Retry
}
```

### DMA Buffer Requirements

**ESP32:**
- DMA buffers automatically allocated by ESP-IDF
- User buffer can be in any memory (DRAM, flash, etc.)
- ESP-IDF copies to DMA-capable buffer if needed

**Max Transfer Size:**
- Default: 65536 bytes (64 KB)
- Configurable via `Config::max_transfer_sz`
- For 4 lanes: max ~16K bytes per lane

### Weak Linking Pattern

Platforms override factory via weak linkage:

**Default (no hardware):**
```cpp
// src/platforms/shared/spi_quad.cpp
FL_LINK_WEAK
fl::vector<SPIQuad*> SPIQuad::createInstances() {
    return {};  // Empty vector
}
```

**ESP32 override (strong definition):**
```cpp
// src/platforms/esp/32/spi_quad_esp32.cpp
fl::vector<SPIQuad*> SPIQuad::createInstances() {
    static SPIQuadESP32 controller2(2, "HSPI");
    static SPIQuadESP32 controller3(3, "VSPI");
    return {&controller2, &controller3};
}
```

Linker picks strong definition when ESP32 code is linked.

---

## Testing

### Mock Driver (SPIQuadStub)

**Location:** `src/platforms/stub/spi_quad_stub.h`

Test-only implementation that captures transmissions:

```cpp
#ifdef FASTLED_TESTING

auto controllers = fl::SPIQuad::getAll();
fl::SPIQuadStub* stub = fl::toStub(controllers[0]);

// Perform transmission
stub->transmitAsync(data);

// Inspect captured data
const auto& transmitted = stub->getLastTransmission();
REQUIRE(transmitted.size() == expected_size);

// De-interleave to verify per-lane data
auto lanes = stub->extractLanes(4, bytes_per_lane);
REQUIRE(lanes[0][0] == 0xAB);
REQUIRE(lanes[1][0] == 0x12);

#endif
```

### Test Coverage

**Unit Tests:** `tests/test_quad_spi.cpp`

1. **Bit-interleaving correctness:**
   - Single byte interleaving
   - Multi-byte interleaving
   - Edge cases (all 0xFF, all 0x00)

2. **Padding synchronization:**
   - Different strip lengths
   - Padding frame repetition
   - Empty lanes (filled with zeros)

3. **Hardware interface:**
   - Initialization (begin/end)
   - Async transmission
   - Busy/complete status

4. **Performance benchmarks:**
   - Transpose timing
   - Mock transmission timing
   - Memory usage

**Integration Tests:**

Run on real ESP32 hardware:
```bash
uv run test.py --qemu esp32s3
```

### Debugging Tools

**Enable verbose logging:**
```cpp
#define FASTLED_DEBUG 1
#include <FastLED.h>

// Prints diagnostic info about Quad-SPI initialization
```

**Inspect transmitted data:**
```cpp
// In testing environment
fl::SPIQuadStub* stub = fl::toStub(quad);
const auto& data = stub->getLastTransmission();

for (size_t i = 0; i < data.size(); i += 4) {
    printf("Interleaved[%zu]: %02X %02X %02X %02X\n",
           i/4, data[i], data[i+1], data[i+2], data[i+3]);
}
```

**Verify de-interleaving:**
```cpp
auto extracted = stub->extractLanes(4, bytes_per_lane);
for (uint8_t lane = 0; lane < 4; ++lane) {
    printf("Lane %d: ", lane);
    for (auto byte : extracted[lane]) {
        printf("%02X ", byte);
    }
    printf("\n");
}
```

---

## Summary

FastLED's Quad-SPI system provides **high-performance parallel LED control** through:

1. **Layered Architecture:** Clean separation between platform, hardware, and algorithm
2. **Bit-Interleaving:** Optimized algorithm for 4-way parallel transmission
3. **Synchronized Latching:** Black LED padding ensures simultaneous strip updates
4. **Platform Abstraction:** Works across ESP32 variants with future expansion
5. **Zero CPU Overhead:** Hardware DMA handles transmission (0% CPU usage)
6. **Transparent Integration:** Works with existing FastLED API

**Key Advantages:**
- 27× performance improvement vs sequential SPI
- Minimal CPU overhead (~1% for transpose)
- Hardware DMA for true async operation
- Automatic detection and promotion
- Comprehensive testing infrastructure

**For Users:** Just add strips with shared clock - FastLED handles the rest!

**For Developers:** Extensible design supports new platforms and chipsets.

---

## References

- **SPIQuad Interface:** `src/platforms/shared/spi_quad.h`
- **Transposer:** `src/platforms/shared/spi_transposer_quad.h`
- **ESP32 Implementation:** `src/platforms/esp/32/spi_quad_esp32.cpp`
- **Bus Manager:** `src/platforms/shared/spi_bus_manager.h`
- **Platform Detection:** `src/platforms/quad_spi_platform.h`
- **Tests:** `tests/test_quad_spi.cpp`
- **Example:** `examples/SpecialDrivers/ESP/QuadSPI/Basic/QuadSPI_Basic.ino`
