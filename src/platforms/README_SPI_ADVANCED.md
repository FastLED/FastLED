# FastLED Advanced SPI System

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Data Flow & Lifecycle](#data-flow--lifecycle)
4. [SPI Bus Manager](#spi-bus-manager)
5. [Multi-Lane SPI (Dual & Quad)](#multi-lane-spi-dual--quad)
6. [Bit-Interleaving Algorithm](#bit-interleaving-algorithm)
7. [Synchronized Latching with Padding](#synchronized-latching-with-padding)
8. [Platform Support](#platform-support)
9. [Performance Characteristics](#performance-characteristics)
10. [Usage Guide](#usage-guide)
11. [Implementation Details](#implementation-details)
12. [Testing](#testing)

---

## Overview

FastLED's Advanced SPI system enables **automatic parallel LED strip control** by intelligently routing multiple SPI-based LED strips through hardware acceleration. The system provides:

- **Automatic detection** - Groups strips by shared clock pins
- **Transparent aggregation** - Promotes to Dual-SPI (2 lanes) or Quad-SPI (4 lanes) automatically
- **27× performance boost** - Up to 27× faster than sequential SPI (Quad-SPI)
- **0% CPU usage** - Hardware DMA handles transmission
- **Drop-in compatibility** - Works with existing FastLED API

### Key Innovation: Smart Bus Management

Instead of manually configuring parallel SPI, FastLED **automatically detects** when multiple LED strips share the same clock pin and **promotes them to multi-lane hardware SPI**:

```cpp
// This code automatically uses Quad-SPI (4 parallel lanes):
FastLED.addLeds<APA102, 23, 18>(leds1, 100);  // Same clock pin (18)
FastLED.addLeds<APA102, 19, 18>(leds2, 100);  // Same clock pin (18)
FastLED.addLeds<APA102, 22, 18>(leds3, 100);  // Same clock pin (18)
FastLED.addLeds<APA102, 21, 18>(leds4, 100);  // Same clock pin (18)
// ↑ FastLED detects 4 strips on clock pin 18 → enables Quad-SPI
```

**Result:** All 4 strips transmit **simultaneously** in ~0.16ms instead of sequentially in ~8ms (50× faster).

---

## System Architecture

The Advanced SPI system uses a **4-layer architecture** that separates routing, aggregation, interleaving, and hardware:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        FastLED User API                             │
│         FastLED.addLeds<APA102, PIN, CLK>() × N strips              │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│                      SPIDeviceProxy                                 │
│  • Template-based per-controller proxy                              │
│  • Mirrors ESP32SPIOutput interface                                 │
│  • Buffers writes for multi-lane SPI                                │
│  • Routes to appropriate backend                                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│                      SPI Bus Manager (Singleton)                    │
│  • Groups devices by shared clock pin                               │
│  • Detects conflicts and promotes to S1/S2/S4                       │
│  • Manages bus lifecycle (reference counting)                       │
│  • Routes transmissions to correct lane                             │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                ┌──────────────┼──────────────┐
                │              │              │
┌───────────────▼──────┐  ┌────▼────────┐  ┌─▼──────────────────────┐
│  Single-SPI (S1)     │  │ Dual (S2)   │  │    Quad-SPI (S4)       │
│  ESP32SPIOutput      │  │ SPIDual +   │  │    SPIQuad +           │
│  (direct HW)         │  │ Transposer  │  │    Transposer          │
└──────────────────────┘  └─────────────┘  └────────────────────────┘
         │                       │                    │
         └───────────────────────┴────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│                       Hardware SPI Peripheral                       │
│         ESP32 SPI2/SPI3 with DMA (HSPI/VSPI buses)                  │
└─────────────────────────────────────────────────────────────────────┘
```

### Layer Responsibilities

1. **User API Layer**: FastLED's `addLeds<>()` creates LED controllers
2. **Device Proxy Layer**: `SPIDeviceProxy<>` wraps each controller, routes SPI calls
3. **Bus Manager Layer**: `SPIBusManager` (singleton) groups by clock pin, manages aggregation
4. **Backend Layer**: Single/Dual/Quad-SPI implementations handle actual transmission
5. **Hardware Layer**: ESP32 SPI peripheral with DMA

---

## Data Flow & Lifecycle

### Phase 1: Registration (Device → Bus Manager → Bus Assignment)

When you call `FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN>()`:

```
1. LED Controller created (e.g., APA102Controller)
   ↓
2. Controller creates SPIDeviceProxy<DATA_PIN, CLOCK_PIN, SPEED>
   ↓
3. SPIDeviceProxy::init() called
   ↓
4. Proxy registers with Bus Manager:
   mBusManager = &getSPIBusManager();  // Get singleton
   mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, this);
   ↓
5. Bus Manager groups device by CLOCK_PIN
   ↓
6. Bus Manager assigns:
   - bus_id (which hardware SPI bus)
   - lane_id (which lane on that bus: 0-3)
   ↓
7. Returns SPIBusHandle { bus_id, lane_id, is_valid }
```

**Example:** 4 strips on clock pin 18

```
Device 1: registerDevice(CLK=18, DATA=23) → Handle { bus_id=0, lane_id=0 }
Device 2: registerDevice(CLK=18, DATA=19) → Handle { bus_id=0, lane_id=1 }
Device 3: registerDevice(CLK=18, DATA=22) → Handle { bus_id=0, lane_id=2 }
Device 4: registerDevice(CLK=18, DATA=21) → Handle { bus_id=0, lane_id=3 }

Bus Manager creates:
  Bus 0: Type=QUAD_SPI, Clock=18, Lanes=[23, 19, 22, 21]
```

### Phase 2: Initialization (Bus Manager → Hardware Allocation)

On first `FastLED.show()`, bus manager initializes hardware:

```
1. SPIBusManager::initialize() called
   ↓
2. For each bus with devices:
   ↓
3. Determine bus type based on device count:
   - 1 device  → SINGLE_SPI (no aggregation)
   - 2 devices → DUAL_SPI (if supported)
   - 3-4 devices → QUAD_SPI (if supported)
   ↓
4. Allocate backend hardware:
   - SINGLE_SPI: Each device creates own ESP32SPIOutput
   - DUAL_SPI:   Create SPIDual controller, configure 2 lanes
   - QUAD_SPI:   Create SPIQuad controller, configure 4 lanes
   ↓
5. Store hardware controller in bus info:
   mBuses[bus_id].quad_controller = quad;  // or dual_controller
   ↓
6. Mark bus as initialized
```

### Phase 3: Transmission (Device → Bus Manager → Hardware)

When you call `FastLED.show()`:

```
For each LED controller:
  1. Controller calls showPixels() on its chipset
     ↓
  2. Chipset writes to SPIDeviceProxy:
     proxy.select();                    // Begin transaction
     proxy.writeByte(0xFF);             // Write start frame
     proxy.writeByte(pixel_data);       // Write LED data
     proxy.writeWord(end_frame);        // Write end frame
     proxy.release();                   // End transaction
     proxy.finalizeTransmission();      // Flush buffered data
     ↓
  3. SPIDeviceProxy routes based on backend:

     If SINGLE_SPI (mSingleSPI != nullptr):
       → Direct passthrough to ESP32SPIOutput
       → Immediate hardware transmission

     If DUAL/QUAD_SPI (mSingleSPI == nullptr):
       → Buffer writes in mWriteBuffer (fl::vector<uint8_t>)
       → On finalizeTransmission():
         ↓
  4. SPIDeviceProxy calls Bus Manager:
     mBusManager->transmit(mHandle, mWriteBuffer.data(), mWriteBuffer.size());
     ↓
  5. Bus Manager stores per-lane data:
     mBuses[bus_id].lanes[lane_id].buffer = data;
     mBuses[bus_id].lanes[lane_id].size = length;
     ↓
  6. Bus Manager calls finalizeTransmission(mHandle):
     ↓
  7. Bus Manager checks if all lanes ready:
     - Increments ready_count
     - If ready_count == total_lanes → transmit!
     ↓
  8. Bus Manager calls transposer:
     SPITransposer::transpose4(lanes, max_size, output_buffer)
     → Bit-interleaves all lane data into single buffer
     ↓
  9. Bus Manager calls hardware:
     quad->transmit(output_buffer);
     quad->waitComplete();
     ↓
  10. Hardware SPI peripheral transmits via DMA (0% CPU)
      → All lanes transmit simultaneously
```

### Phase 4: Cleanup (Device Destruction → Hardware Lifecycle)

When a device is destroyed (e.g., program exit):

```
1. SPIDeviceProxy::~SPIDeviceProxy() called
   ↓
2. Proxy unregisters from Bus Manager:
   mBusManager->unregisterDevice(mHandle);
   ↓
3. Bus Manager removes device from bus:
   - Decrements reference count for that bus
   ↓
4. If last device on bus (refcount == 0):
   ↓
5. Bus Manager releases hardware:
   releaseBusHardware(bus_id);
   ↓
6. Delete hardware controller:
   delete mBuses[bus_id].quad_controller;  // or dual_controller
   mBuses[bus_id].quad_controller = nullptr;
   ↓
7. Mark bus as uninitialized
```

**Reference Counting Example:**

```
4 devices on Bus 0 (Quad-SPI):
  Device 1 destroyed → refcount = 3 (keep hardware)
  Device 2 destroyed → refcount = 2 (keep hardware)
  Device 3 destroyed → refcount = 1 (keep hardware)
  Device 4 destroyed → refcount = 0 → releaseBusHardware() called
                                     → SPIQuad controller deleted
```

---

## SPI Bus Manager

**Location:** `src/platforms/shared/spi_bus_manager.h`

The **Bus Manager** is a **singleton** that acts as the central routing and lifecycle controller for all SPI devices.

### Core Responsibilities

1. **Device Registration**: Assigns devices to buses and lanes
2. **Conflict Detection**: Groups devices by shared clock pins
3. **Automatic Promotion**: Upgrades to Dual/Quad-SPI when beneficial
4. **Hardware Allocation**: Creates and manages backend controllers
5. **Lifecycle Management**: Reference counting, cleanup on last device removal
6. **Transmission Routing**: Routes per-lane data to correct hardware

### Key Interface

```cpp
class SPIBusManager {
public:
    // Register a device (called by SPIDeviceProxy::init())
    SPIBusHandle registerDevice(uint8_t clock_pin, uint8_t data_pin, void* controller);

    // Unregister a device (called by SPIDeviceProxy destructor)
    void unregisterDevice(SPIBusHandle handle);

    // Initialize all buses (called on first FastLED.show())
    bool initialize();

    // Transmit data for a specific device/lane
    void transmit(SPIBusHandle handle, const uint8_t* data, size_t length);

    // Finalize transmission (triggers actual hardware transmission when all lanes ready)
    void finalizeTransmission(SPIBusHandle handle);

    // Query bus information
    const SPIBusInfo* getBusInfo(uint8_t bus_id) const;
    bool isDeviceEnabled(SPIBusHandle handle) const;

    // Cleanup (called by destructor)
    void reset();

private:
    struct SPIBusInfo {
        SPIBusType bus_type;           // SINGLE_SPI, DUAL_SPI, QUAD_SPI
        uint8_t clock_pin;
        uint8_t data_pins[4];          // Up to 4 data lanes
        uint8_t num_lanes;
        uint8_t reference_count;       // Number of active devices
        bool initialized;

        // Backend hardware (only one is active)
        SPIQuad* quad_controller;      // For QUAD_SPI
        SPIDual* dual_controller;      // For DUAL_SPI (future)

        // Per-lane buffering
        struct LaneBuffer {
            const uint8_t* buffer;
            size_t size;
        } lanes[4];
        uint8_t ready_count;           // How many lanes have data ready
    };

    fl::vector<SPIBusInfo> mBuses;     // All registered buses
    bool mInitialized;
};

// Global singleton accessor
SPIBusManager& getSPIBusManager();
```

### Singleton Pattern

All production code uses the **global singleton**:

```cpp
// In SPIDeviceProxy::init()
mBusManager = &getSPIBusManager();  // Get global instance
```

This ensures all devices across different LED controllers **share the same bus manager**, enabling:
- Clock pin conflict detection
- Automatic Quad-SPI promotion
- Coordinated hardware lifecycle

**Implementation:**

```cpp
// Singleton accessor (thread-safe in C++11)
inline SPIBusManager& getSPIBusManager() {
    static SPIBusManager instance;  // Created once, lives forever
    return instance;
}
```

**Note:** Unit tests create **local instances** for isolation, avoiding global state pollution during testing.

### Bus Type Determination

```cpp
enum class SPIBusType {
    SOFT_SPI,    // Software bit-bang (fallback)
    SINGLE_SPI,  // 1 device, hardware SPI (no aggregation)
    DUAL_SPI,    // 2 devices, hardware dual-lane SPI
    QUAD_SPI     // 3-4 devices, hardware quad-lane SPI
};
```

**Promotion Logic:**

```
Device count on shared clock pin → Bus type
  1 device  → SINGLE_SPI (ESP32SPIOutput)
  2 devices → DUAL_SPI   (SPIDual, if supported)
  3 devices → QUAD_SPI   (SPIQuad, uses 3 of 4 lanes)
  4 devices → QUAD_SPI   (SPIQuad, all 4 lanes)
  5+ devices → ERROR (disable conflicting devices, warn user)
```

---

## Multi-Lane SPI (Dual & Quad)

### How Multi-Lane SPI Works

**Standard SPI (1 data line):**
```
Clock: ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┐ ┌──
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
MOSI:    ──1───0───1───1───0───0───1───0──  (8 clocks = 1 byte)
```

**Dual-SPI (2 data lines):**
```
Clock: ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌──
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
D0:      ──1───0───1───1───0───0───1───0──  Strip 1 data
D1:      ──0───1───1───0───0───1───1───0──  Strip 2 data
         (8 clocks = 2 bytes, one per strip)
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

All LED strips share a **single clock line** but have **separate data lines**:

**Quad-SPI Wiring:**
```
ESP32
  CLK (GPIO 18) ──┬────────> Strip 1 Clock
                  ├────────> Strip 2 Clock
                  ├────────> Strip 3 Clock
                  └────────> Strip 4 Clock

  D0  (GPIO 23) ──────────> Strip 1 Data  (MOSI/D0)
  D1  (GPIO 19) ──────────> Strip 2 Data  (MISO/D1)
  D2  (GPIO 22) ──────────> Strip 3 Data  (WP/D2)
  D3  (GPIO 21) ──────────> Strip 4 Data  (HD/D3)
```

**Dual-SPI Wiring:**
```
ESP32
  CLK (GPIO 18) ──┬────────> Strip 1 Clock
                  └────────> Strip 2 Clock

  D0  (GPIO 23) ──────────> Strip 1 Data  (MOSI/D0)
  D1  (GPIO 19) ──────────> Strip 2 Data  (MISO/D1)
```

---

## Bit-Interleaving Algorithm

**Location:** `src/platforms/shared/spi_transposer.h` (Unified)

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

The transposer **interleaves bits** from all lanes into output bytes.

#### Quad-SPI Interleaving (4 lanes)

**Input (4 lanes, 1 byte each):**
```
Lane 0: 0xAB = 10101011
Lane 1: 0x12 = 00010010
Lane 2: 0xEF = 11101111
Lane 3: 0x78 = 01111000
```

**Output (4 interleaved bytes):**

Each output byte contains **2 bits from each lane** (MSB first):

```
Byte format: [D3b7 D2b7 D1b7 D0b7 D3b6 D2b6 D1b6 D0b6]

Out[0] = 0b01110010  (bits 7:6 from each lane)
         = L3[7:6]=01, L2[7:6]=11, L1[7:6]=00, L0[7:6]=10

Out[1] = 0b11110110  (bits 5:4 from each lane)
         = L3[5:4]=11, L2[5:4]=11, L1[5:4]=01, L0[5:4]=10

Out[2] = 0b10010011  (bits 3:2 from each lane)
         = L3[3:2]=10, L2[3:2]=01, L1[3:2]=00, L0[3:2]=11

Out[3] = 0b00111011  (bits 1:0 from each lane)
         = L3[1:0]=00, L2[1:0]=11, L1[1:0]=10, L0[1:0]=11
```

#### Dual-SPI Interleaving (2 lanes)

**Input (2 lanes, 1 byte each):**
```
Lane 0: 0xAB = 10101011 (hi=0xA, lo=0xB)
Lane 1: 0x12 = 00010010 (hi=0x1, lo=0x2)
```

**Output (2 interleaved bytes):**

Each output byte contains **1 nibble from each lane**:

```
Out[0] = 0x1A  (hi nibbles: Lane1=0x1, Lane0=0xA)
Out[1] = 0x2B  (lo nibbles: Lane1=0x2, Lane0=0xB)
```

### Optimized Implementation

**Quad-SPI (Direct 2-bit extraction):**

```cpp
void interleave_byte_optimized(uint8_t* dest,
                               uint8_t a, uint8_t b,
                               uint8_t c, uint8_t d) {
    // Extract 2-bit chunks directly (faster than bit-by-bit)
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

**Dual-SPI (Nibble extraction):**

```cpp
void interleave_byte_optimized(uint8_t* dest, uint8_t a, uint8_t b) {
    // Each output byte = 4 bits from each lane
    dest[0] = ((a >> 4) & 0x0F) | (((b >> 4) & 0x0F) << 4);  // Hi nibbles
    dest[1] = (a & 0x0F) | ((b & 0x0F) << 4);                // Lo nibbles
}
```

### Performance

| Operation | Quad-SPI (4 lanes) | Dual-SPI (2 lanes) |
|-----------|--------------------|--------------------|
| Interleave 100 LEDs | ~50-100µs | ~25-50µs |
| Interleave 500 LEDs | ~250µs | ~125µs |

**Note:** Interleaving is CPU-bound but runs **once per frame**. Transmission is DMA-driven (0% CPU).

---

## Synchronized Latching with Padding

### The Challenge: Different Strip Lengths

LED strips often have different lengths:

```
Strip 1: 60 LEDs   → 240 bytes
Strip 2: 100 LEDs  → 400 bytes
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
Strip 1: ░░░░░░░░░░░░████████████ (60 real + padding to 120)
Strip 2: ░░░░░░░░████████████████ (100 real + padding to 120)
Strip 3: ░░░░████████████████████ (80 real + padding to 120)
Strip 4: ████████████████████████ (120 real, no padding)

All strips finish simultaneously → synchronized latch ✓
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

### ESP32 SPI Peripheral Availability

Understanding ESP32 SPI peripheral allocation is critical for LED control implementation:

| Platform | SPI0 | SPI1 | SPI2 | SPI3 | Available for LEDs | FastLED Uses |
|----------|------|------|------|------|-------------------|--------------|
| **ESP32 (classic)** | Flash cache | Flash | ✅ General | ✅ General | **SPI2 + SPI3** (2 hosts) | ✅ **SPI Engine** |
| **ESP32-S2** | Flash cache | Flash | ✅ General | ✅ General | **SPI2 + SPI3** (2 hosts) | ✅ **SPI Engine** |
| **ESP32-S3** | Flash cache | Flash | ✅ General | ✅ General | **SPI2 + SPI3** (2 hosts) | ✅ **SPI Engine** |
| **ESP32-C3** | Flash cache | Flash | ✅ General | ❌ N/A | **SPI2 only** (1 host) | ⚠️ **SPI Engine** (limited) |
| **ESP32-C6** | Flash cache | Flash | ✅ General | ❌ N/A | **SPI2 only** (1 host) | ❌ **RMT5** (SPI not used) |
| **ESP32-H2** | Flash cache | Flash | ✅ General | ❌ N/A | **SPI2 only** (1 host) | ❌ **RMT5** (SPI not used) |
| **ESP32-P4** | Flash cache | Flash | ✅ General | ✅ General | **SPI2 + SPI3** (2 hosts) | ✅ **SPI Engine** + Octal |

**Key Points:**

- **SPI0/SPI1**: Always reserved for flash/PSRAM operations across all ESP32 variants
- **SPI2/SPI3**: General-purpose SPI hosts that can be used for LED control
- **ESP32-C6 Design Decision**: While ESP32-C6 has SPI2 available, FastLED uses RMT5 instead because:
  - Only 1 SPI host available (vs 2-3 on other variants) limits scalability
  - RMT5 provides better performance and nanosecond-precise timing
  - Preserves SPI2 for other user peripherals
- **ESP-IDF Restriction**: `spi_bus_initialize()` documentation explicitly states "SPI0/1 is not supported" for general use

### Current Support

| Platform | Dual-SPI | Quad-SPI | Buses | Notes |
|----------|----------|----------|-------|-------|
| ESP32    | ✅       | ✅       | 2 (HSPI, VSPI) | Full support |
| ESP32-S2 | ✅       | ✅       | 2 | Full support |
| ESP32-S3 | ✅       | ✅       | 2 | Full support |
| ESP32-C3 | ✅       | ❌       | 1 (SPI2) | Dual-SPI only (2 lanes max) |
| ESP32-C2 | ✅       | ❌       | 1 (SPI2) | Dual-SPI only (2 lanes max) |
| ESP32-C6 | ⚠️       | ❌       | 1 (SPI2) | **SPI2 available but not used** - RMT5 preferred (better performance, preserves SPI2 for users) |
| ESP32-H2 | ⚠️       | ❌       | 1 (SPI2) | **SPI2 available but not used** - RMT5 preferred |
| ESP32-P4 | ✅       | ⚠️       | 2 | Supports Octal-SPI (8 lanes, future) |
| **Teensy 4.0/4.1** | ✅ | ⚠️ | 3 (SPI, SPI1, SPI2) | LPSPI supports dual/quad via WIDTH register; Quad mode requires data2/data3 pins (PCS2/PCS3) not exposed on standard boards. See `LP_SPI.md` for implementation details |
| Testing  | ✅       | ✅       | N/A | Mock drivers for unit tests |

### Future Platforms

- **RP2040:** PIO-based Dual/Quad-SPI implementation
- **STM32:** Hardware Quad-SPI peripheral support
- **Teensy 3.x:** Standard SPI only (no multi-lane hardware support)

### Adding New Platforms

1. Implement `SPIQuad` interface (or `SPIDual`) for your platform
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

### Benchmark: 2×100 LED Strips (APA102)

**Sequential Software SPI (baseline):**
```
Strip 1: 2.0 ms
Strip 2: 2.0 ms  Total: 4.0 ms
CPU usage: 100% during transmission
```

**Dual-SPI Hardware DMA:**
```
Transpose: 0.04 ms (CPU, one-time)
Transmit:  0.16 ms (DMA, zero CPU)
Total:     0.20 ms

Speedup: 4.0 / 0.20 = 20× faster
CPU usage: 0% during transmission
```

### Performance Breakdown

| Operation | Quad-SPI (4×100 LEDs) | Dual-SPI (2×100 LEDs) |
|-----------|------------------------|------------------------|
| Bit-interleaving | 50-100µs (CPU) | 25-50µs (CPU) |
| DMA transmission | ~80µs (0% CPU) | ~160µs (0% CPU) |
| **Total** | **~160µs** | **~200µs** |
| **Speedup** | **50×** | **20×** |

### Scalability

#### Quad-SPI (4 lanes)

| LEDs per Strip | Transpose | Transmit @ 40MHz | Total |
|----------------|-----------|------------------|-------|
| 50             | 25µs      | 40µs             | 65µs  |
| 100            | 50µs      | 80µs             | 130µs |
| 200            | 100µs     | 160µs            | 260µs |
| 500            | 250µs     | 400µs            | 650µs |

#### Dual-SPI (2 lanes)

| LEDs per Strip | Transpose | Transmit @ 40MHz | Total |
|----------------|-----------|------------------|-------|
| 50             | 12µs      | 80µs             | 92µs  |
| 100            | 25µs      | 160µs            | 185µs |
| 200            | 50µs      | 320µs            | 370µs |
| 500            | 125µs     | 800µs            | 925µs |

**Note:** Transmission is DMA-driven (0% CPU), so CPU can do other work during transmission.

### Clock Speed Trade-offs

| Clock Speed | Transmission Time (4×100 LEDs) | Notes |
|-------------|-------------------------------|-------|
| 10 MHz      | 320µs                         | Safe for long wires |
| 20 MHz      | 160µs                         | Default, good balance |
| 40 MHz      | 80µs                          | Fast, short wires only |
| 80 MHz      | 40µs                          | Very fast, signal integrity issues |

---

## Usage Guide

### Basic Usage (Automatic Mode - Recommended)

Just add multiple strips with the **same clock pin** - FastLED automatically detects and enables multi-lane SPI:

#### Quad-SPI Example (4 strips)

```cpp
#include <FastLED.h>

#define CLOCK_PIN 18
#define NUM_LEDS 100

CRGB leds_strip1[NUM_LEDS];
CRGB leds_strip2[NUM_LEDS];
CRGB leds_strip3[NUM_LEDS];
CRGB leds_strip4[NUM_LEDS];

void setup() {
    // Add 4 strips sharing clock pin 18
    // FastLED auto-detects and enables Quad-SPI (4 parallel lanes)
    FastLED.addLeds<APA102, 23, CLOCK_PIN>(leds_strip1, NUM_LEDS);
    FastLED.addLeds<APA102, 19, CLOCK_PIN>(leds_strip2, NUM_LEDS);
    FastLED.addLeds<APA102, 22, CLOCK_PIN>(leds_strip3, NUM_LEDS);
    FastLED.addLeds<APA102, 21, CLOCK_PIN>(leds_strip4, NUM_LEDS);
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

#### Dual-SPI Example (2 strips)

```cpp
#include <FastLED.h>

#define CLOCK_PIN 18
#define NUM_LEDS 100

CRGB leds_strip1[NUM_LEDS];
CRGB leds_strip2[NUM_LEDS];

void setup() {
    // Add 2 strips sharing clock pin 18
    // FastLED auto-detects and enables Dual-SPI (2 parallel lanes)
    FastLED.addLeds<APA102, 23, CLOCK_PIN>(leds_strip1, NUM_LEDS);
    FastLED.addLeds<APA102, 19, CLOCK_PIN>(leds_strip2, NUM_LEDS);
}

void loop() {
    // Set colors independently
    fill_rainbow(leds_strip1, NUM_LEDS, 0, 7);
    fill_rainbow(leds_strip2, NUM_LEDS, 128, 7);

    // Both strips transmit in parallel (hardware DMA)
    FastLED.show();
}
```

### Advanced: Direct SpiHw4 Usage

For low-level control:

```cpp
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_transposer.h"

void setup() {
    // Get available Quad-SPI controllers
    const auto& controllers = fl::SpiHw4::getAll();
    if (controllers.empty()) {
        Serial.println("No Quad-SPI hardware available");
        return;
    }

    fl::SpiHw4* quad = controllers[0];

    // Configure hardware
    fl::SpiHw4::Config config;
    config.bus_num = 2;              // HSPI
    config.clock_speed_hz = 40000000; // 40 MHz
    config.clock_pin = 18;
    config.data0_pin = 23;
    config.data1_pin = 19;
    config.data2_pin = 22;
    config.data3_pin = 21;

    if (!quad->begin(config)) {
        Serial.println("Failed to initialize Quad-SPI");
        return;
    }

    // Prepare lane data
    fl::vector<uint8_t> lane0_data = {0xFF, 0x00, 0x00};  // Red LED
    fl::vector<uint8_t> lane1_data = {0xFF, 0xFF, 0x00};  // Green LED

    fl::optional<fl::SPITransposer::LaneData> lanes[4];
    lanes[0] = fl::SPITransposer::LaneData{
        fl::span<const uint8_t>(lane0_data.data(), lane0_data.size()),
        fl::span<const uint8_t>()  // No padding
    };
    lanes[1] = fl::SPITransposer::LaneData{
        fl::span<const uint8_t>(lane1_data.data(), lane1_data.size()),
        fl::span<const uint8_t>()
    };

    // Transpose and transmit
    size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
    fl::vector<uint8_t> output(max_size * 4);

    const char* error = nullptr;
    if (fl::SPITransposer::transpose4(lanes[0], lanes[1], lanes[2], lanes[3],
                                      fl::span<uint8_t>(output), &error)) {
        quad->transmit(fl::span<const uint8_t>(output));
        quad->waitComplete();
    } else {
        Serial.printf("Transpose failed: %s\n", error);
    }
}
```

### Platform Detection

Check if multi-lane SPI is available:

```cpp
#include "platforms/quad_spi_platform.h"

#if FASTLED_HAS_QUAD_SPI
    // Compile-time check (API availability)
    Serial.println("Quad-SPI API is available");
#endif

// Runtime check (actual hardware)
const auto& controllers = fl::SpiHw4::getAll();
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
- Allocate output buffer (`max_size * 4` bytes for Quad, `* 2` for Dual)
- Manage lane data lifetime (must persist until `waitComplete()`)
- Handle errors from `transpose()` function

**Platform Responsibility:**
- Allocate DMA buffers internally (ESP-IDF manages this)
- Free resources on `end()` or destructor

### Thread Safety

**SPITransposer:**
- Pure functional, thread-safe (no shared state)
- Multiple threads can call `transpose2()`, `transpose4()`, etc. simultaneously

**SpiHw2/SpiHw4 Implementations:**
- Not thread-safe (single hardware peripheral)
- Caller must serialize `transmit()` calls
- `waitComplete()` must be called before next transmission

**SPIBusManager:**
- Not thread-safe (singleton, assumes single-threaded FastLED usage)
- All FastLED API calls assumed to be from single thread

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
- `"Output buffer too small"` - Need `max_size * 4` bytes (Quad) or `* 2` (Dual)
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

if (!quad->transmit(buffer)) {
    // DMA queue full or transmission error
    quad->waitComplete();  // Clear pending
    quad->transmit(buffer);  // Retry
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
- For 2 lanes: max ~32K bytes per lane

### Weak Linking Pattern

Platforms override factory via weak linkage:

**Default (no hardware):**
```cpp
// src/platforms/shared/spi_hw_4.cpp
FL_LINK_WEAK
fl::vector<SpiHw4*> SpiHw4::createInstances() {
    return {};  // Empty vector
}
```

**ESP32 override (strong definition):**
```cpp
// src/platforms/esp/32/spi_hw_4_esp32.cpp
fl::vector<SpiHw4*> SpiHw4::createInstances() {
    static SpiHw4ESP32 controller2(2, "HSPI");
    static SpiHw4ESP32 controller3(3, "VSPI");
    return {&controller2, &controller3};
}
```

Linker picks strong definition when ESP32 code is linked.

---

## Testing

### Mock Drivers

**Quad-SPI Mock:** `src/platforms/stub/spi_4_stub.h`
**Dual-SPI Mock:** `src/platforms/stub/spi_2_stub.h`

Test-only implementation that captures transmissions:

```cpp
#ifdef FASTLED_TESTING

auto controllers = fl::SpiHw4::getAll();
fl::SpiHw4Stub* stub = fl::toStub(controllers[0]);

// Perform transmission
stub->transmit(data);

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

**Unit Tests:**
- `tests/test_quad_spi.cpp` - Quad-SPI transposer and hardware interface
- `tests/test_spi_bus_manager.cpp` - Bus manager lifecycle and routing
- `tests/test_dual_spi.cpp` - Dual-SPI (future)

**Test Categories:**

1. **Bit-interleaving correctness:**
   - Single/multi-byte interleaving
   - Edge cases (all 0xFF, all 0x00)
   - Lane padding and synchronization

2. **Bus Manager:**
   - Device registration/unregistration
   - Clock pin conflict detection
   - Automatic Dual/Quad promotion
   - Reference counting and cleanup

3. **Hardware interface:**
   - Initialization (begin/end)
   - Async transmission
   - Busy/complete status

4. **Integration:**
   - Full SPIDeviceProxy → Bus Manager → Hardware flow
   - Multiple devices on same clock pin
   - Different strip lengths (padding)

### Running Tests

```bash
# All tests
uv run test.py

# Specific test
uv run test.py quad_spi
uv run test.py spi_bus_manager

# With QEMU (ESP32 hardware emulation)
uv run test.py --qemu esp32s3
```

### Debugging Tools

**Enable verbose logging:**
```cpp
#define FASTLED_DEBUG 1
#include <FastLED.h>

// Prints diagnostic info about multi-lane SPI initialization
```

**Inspect transmitted data:**
```cpp
// In testing environment
fl::SpiHw4Stub* stub = fl::toStub(quad);
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

FastLED's Advanced SPI system provides **intelligent, automatic parallel LED control** through:

1. **Smart Bus Management:** Central singleton routes devices by clock pin
2. **Automatic Aggregation:** Detects conflicts, promotes to Dual/Quad-SPI
3. **Bit-Interleaving:** Optimized algorithm for 2-way/4-way parallel transmission
4. **Synchronized Latching:** Black LED padding ensures simultaneous strip updates
5. **Platform Abstraction:** Works across ESP32 variants with future expansion
6. **Zero CPU Overhead:** Hardware DMA handles transmission (0% CPU usage)
7. **Transparent Integration:** Works with existing FastLED API - no code changes needed
8. **Lifecycle Management:** Reference counting, automatic hardware cleanup

**Key Advantages:**
- Up to 50× performance improvement (Quad-SPI vs sequential)
- Minimal CPU overhead (~1% for interleaving, 0% for transmission)
- Hardware DMA for true async operation
- Automatic detection and promotion
- Drop-in compatibility with existing sketches
- Comprehensive testing infrastructure

**For Users:** Just add strips with shared clock pins - FastLED handles everything automatically!

**For Developers:** Extensible architecture supports new platforms, chipsets, and higher lane counts (Octal-SPI).

---

## Architecture Diagram (Complete System)

```
┌─────────────────────────────────────────────────────────────────────┐
│                          User Code                                  │
│  FastLED.addLeds<APA102, PIN, CLK>() × N                           │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│                  LED Controller (APA102Controller)                  │
│  • Manages LED buffer (CRGB array)                                  │
│  • Calls chipset showPixels()                                       │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│              SPIDeviceProxy<DATA, CLOCK, SPEED>                     │
│  • Template-based per-controller proxy                              │
│  • init() → registers with Bus Manager                              │
│  • writeByte/writeWord() → buffers or passthrough                   │
│  • finalizeTransmission() → triggers bus manager                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│              SPIBusManager (Singleton)                              │
│  • registerDevice(clock, data) → assigns bus_id + lane_id           │
│  • Groups devices by shared clock pin                               │
│  • Determines bus type: SINGLE / DUAL / QUAD                        │
│  • transmit(handle, data) → stores per-lane buffers                 │
│  • finalizeTransmission() → triggers interleave + hardware          │
│  • Reference counting → cleanup on last unregister                  │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
┌───────▼────────┐  ┌──────────▼─────────┐  ┌────────▼───────────────┐
│  SINGLE_SPI    │  │     DUAL_SPI       │  │      QUAD_SPI          │
│  (1 device)    │  │   (2 devices)      │  │    (3-4 devices)       │
├────────────────┤  ├────────────────────┤  ├────────────────────────┤
│ ESP32SPIOutput │  │ SPITransposer      │  │ SPITransposer          │
│ (direct HW)    │  │ (transpose2)       │  │ (transpose4)           │
│                │  │       ↓            │  │       ↓                │
│                │  │   SpiHw2 driver    │  │   SpiHw4 driver        │
└────────┬───────┘  └──────────┬─────────┘  └────────┬───────────────┘
         │                     │                      │
         └─────────────────────┴──────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│              ESP32 SPI Peripheral (SPI2/SPI3)                       │
│  • Hardware DMA-driven transmission                                 │
│  • 0% CPU usage during transmission                                 │
│  • Supports Single/Dual/Quad/Octal modes                            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## References

### Core Components

- **SPI Bus Manager:** `src/platforms/shared/spi_bus_manager.h`
- **SPI Device Proxy:** `src/platforms/esp/32/spi_device_proxy.h`
- **Quad-SPI Interface:** `src/platforms/shared/spi_hw_4.h`
- **Dual-SPI Interface:** `src/platforms/shared/spi_hw_2.h`
- **Unified Transposer:** `src/platforms/shared/spi_transposer.h`

### Platform Implementations

#### ESP32
- **ESP32 Quad-SPI:** `src/platforms/esp/32/spi_hw_4_esp32.cpp`
- **ESP32 Dual-SPI:** `src/platforms/esp/32/spi_hw_2_esp32.cpp`
- **ESP32 Single-SPI:** `src/platforms/esp/32/fastspi_esp32.h`
- **ESP32 SPI Proxy:** `src/platforms/esp/32/spi_device_proxy.h`

#### Teensy 4.x
- **Teensy Quad-SPI:** `src/platforms/arm/teensy/teensy4_common/spi_hw_4_mxrt1062.cpp`
- **Teensy Dual-SPI:** `src/platforms/arm/teensy/teensy4_common/spi_hw_2_mxrt1062.cpp`
- **Teensy Single-SPI:** `src/platforms/arm/mxrt1062/fastspi_arm_mxrt1062.h`
- **Teensy SPI Proxy:** `src/platforms/arm/mxrt1062/spi_device_proxy.h`
- **LPSPI Implementation Guide:** `LP_SPI.md` (quad-mode pin configuration details)

#### Platform Detection
- **Platform Detection:** `src/platforms/quad_spi_platform.h`

### Testing

- **Bus Manager Tests:** `tests/test_spi_bus_manager.cpp`
- **Quad-SPI Tests:** `tests/test_quad_spi.cpp`
- **Dual-SPI Tests:** `tests/test_dual_spi.cpp`
- **Quad-SPI Mock:** `src/platforms/stub/spi_4_stub.h`
- **Dual-SPI Mock:** `src/platforms/stub/spi_2_stub.h`

### Examples

- **Quad-SPI Basic:** `examples/SpecialDrivers/ESP/QuadSPI/Basic/QuadSPI_Basic.ino`

### Documentation

- **Quad-SPI Only:** `src/platforms/README_SPI_QUAD.md`
- **Dual-SPI Only:** `src/platforms/README_SPI_DUAL.md`
- **This Document:** `src/platforms/README_SPI_ADVANCED.md` (unified guide)
