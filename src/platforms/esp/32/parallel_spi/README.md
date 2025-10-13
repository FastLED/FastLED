# Multi-Width ISR SPI Implementation with Host Testing

High-priority interrupt-based parallel soft-SPI driver for ESP32 RISC-V platforms (C2/C3/C6/H2) and host testing.

## 📐 Architecture Overview

### Core Design Principle

All width variants (1/2/4/8-way) share the **same ISR core engine** (`fl_parallel_spi_isr_rv.h/cpp`). Only the LUT (Look-Up Table) initialization differs per width, making the implementation highly maintainable and consistent.

```
┌─────────────────────────────────────────────────────────────┐
│                ISR Core Engine (C) - Reusable                │
│  - Generic bit transmission logic (256-entry LUT)            │
│  - Platform-agnostic (ESP32 & host)                          │
│  - Zero volatile reads, write-only                           │
│  - Software bitbanger (ISR-based)                            │
└───────────┬─────────────────────────────────────────────────┘
            │
            ├─────────────────────────────────────────────┐
            │                                             │
   ┌────────▼──────────┐                     ┌───────────▼────────────┐
   │  Width Wrappers   │                     │  Platform Abstraction  │
   │  - 1-way ✅       │                     │    ┌──────────────┐    │
   │  - 2-way ✅       │                     │    │    ESP32     │    │
   │  - 4-way ✅       │                     │    │  - GPIO MMIO │    │
   │  - 8-way ✅       │                     │    │  - HW timer  │    │
   └───────────────────┘                     │    │  - Real ISR  │    │
                                             │    └──────────────┘    │
                                             │    ┌──────────────┐    │
                                             │    │     Host     │    │
                                             │    │  - Ring Buf  │    │
                                             │    │  - SW timer  │    │
                                             │    │  - Mock ISR  │    │
                                             │    └──────────────┘    │
                                             └───────────┬────────────┘
                                                         │
                                                ┌────────▼─────────┐
                                                │   Unit Tests     │
                                                │  - 1-way ✅      │
                                                │  - 2-way ✅      │
                                                │  - 4-way ✅      │
                                                │  - 8-way ✅      │
                                                └──────────────────┘
```

## 📊 Width Variants

| Width | Data Pins | Clock | Use Case | Header File |
|-------|-----------|-------|----------|-------------|
| 1-way | 1 (D0) | 1 | Baseline testing, debugging | `parallel_spi_isr_single_esp32c3.hpp` |
| 2-way | 2 (D0-D1) | 1 | Matches Dual-SPI hardware | `parallel_spi_isr_dual_esp32c3.hpp` |
| 4-way | 4 (D0-D3) | 1 | Matches Quad-SPI hardware | `parallel_spi_isr_quad_esp32c3.hpp` |
| 8-way | 8 (D0-D7) | 1 | Maximum parallelism | `fastled_parallel_spi_esp32c3.hpp` |

### When to Use Each Width

- **1-way (Single-SPI)**: Ideal for simple applications, baseline testing, or when GPIO pins are scarce. Good for understanding ISR behavior.
- **2-way (Dual-SPI)**: Matches hardware Dual-SPI topology on ESP32-C2/C3/C6/H2 platforms. Good for validation testing.
- **4-way (Quad-SPI)**: Matches hardware Quad-SPI topology on ESP32/S2/S3/P4 platforms. Balances parallelism with pin usage.
- **8-way (Octo-SPI)**: Maximum throughput, drives 8 LED strips simultaneously. Future ESP32-P4 may support hardware Octal-SPI.

## 🔑 Key Files

### ISR Core Engine
- **`fl_parallel_spi_isr_rv.h`** - C interface for ISR engine
- **`fl_parallel_spi_isr_rv.cpp`** - RISC-V optimized ISR implementation
- **`fl_parallel_spi_platform.h`** - Platform abstraction layer (ESP32 vs host)

### Width Wrappers
- **`parallel_spi_isr_single_esp32c3.hpp`** - 1-way wrapper class
- **`parallel_spi_isr_dual_esp32c3.hpp`** - 2-way wrapper class
- **`parallel_spi_isr_quad_esp32c3.hpp`** - 4-way wrapper class
- **`fastled_parallel_spi_esp32c3.hpp`** - 8-way wrapper class

### Host Simulation (Testing)
- **`fl_parallel_spi_host_sim.h`** - Host simulation API
- **`fl_parallel_spi_host_sim.cpp`** - Ring buffer capture for GPIO events
- **`fl_parallel_spi_host_timer.cpp`** - Timer simulation for testing

### Platform Support
- **`esp32c3_isr_platform.cpp`** - ESP32-C3/C2 platform-specific code

## 🚀 Usage Example

### Basic 4-Way (Quad-SPI) Usage

```cpp
#include "platforms/esp/32/parallel_spi/parallel_spi_isr_quad_esp32c3.hpp"

using namespace fl;

void setup() {
    // Create Quad-SPI driver instance
    QuadSPI_ISR_ESP32C3 spi;

    // Configure pin mapping (4 data pins + 1 clock)
    spi.setPinMapping(0, 1, 2, 3, 8);  // D0-D3 = GPIO0-3, CLK = GPIO8

    // Prepare data buffer
    uint8_t data[4] = {0x0F, 0x0A, 0x05, 0x00};
    spi.loadBuffer(data, 4);

    // Setup ISR (1.6MHz timer → 800kHz SPI bit rate)
    spi.setupISR(1600000);

    // Wait for memory visibility
    QuadSPI_ISR_ESP32C3::visibilityDelayUs(10);

    // Start transfer
    spi.arm();

    // Wait for completion
    while (spi.isBusy()) {
        delay(1);
    }

    // Acknowledge completion
    spi.ackDone();

    // Stop ISR
    spi.stopISR();
}
```

### API Reference (Common to All Widths)

```cpp
// Pin configuration (varies by width)
void setPinMapping(...);  // 1-way: (data, clk)
                          // 2-way: (d0, d1, clk)
                          // 4-way: (d0, d1, d2, d3, clk)
                          // 8-way: (d0-d7, clk)

// Data transfer
void loadBuffer(const uint8_t* data, uint16_t n);  // Load up to 256 bytes
int setupISR(uint32_t timer_hz);                   // Setup timer ISR
void arm();                                         // Start transfer
bool isBusy() const;                               // Check if transfer active
void ackDone();                                    // Acknowledge completion
void stopISR();                                    // Stop timer ISR

// Memory visibility (required before arm())
static void visibilityDelayUs(uint32_t us);       // Wait for memory sync

// Status flags
uint32_t statusFlags() const;                      // Get status (BUSY | DONE)
static constexpr uint32_t STATUS_BUSY = 1u;
static constexpr uint32_t STATUS_DONE = 2u;
```

## 🧪 Testing Infrastructure

### Host Simulation

The implementation includes a **host simulation layer** that allows testing ISR behavior on a development machine without ESP32 hardware:

- **Ring Buffer Capture**: All GPIO write operations are captured to a ring buffer
- **Manual Tick Control**: Tests drive ISR execution tick-by-tick for deterministic results
- **Event Inspection**: Tests can examine GPIO events to verify correctness
- **Same ISR Code**: Identical ISR code runs on both ESP32 and host

### Running Tests

```bash
# Run individual width tests
uv run test.py test_parallel_spi_isr_single  # 1-way tests (10 test cases)
uv run test.py test_parallel_spi_isr_dual    # 2-way tests (9 test cases)
uv run test.py test_parallel_spi_isr_quad    # 4-way tests (7 test cases)

# All tests include:
# - Basic transmission
# - Clock toggling verification
# - Data pattern verification
# - Multi-byte sequences
# - Edge cases (zero bytes, max bytes, etc.)
```

### Test Files
- **`tests/test_parallel_spi_isr_single.cpp`** - 10 test cases for 1-way
- **`tests/test_parallel_spi_isr_dual.cpp`** - 9 test cases for 2-way
- **`tests/test_parallel_spi_isr_quad.cpp`** - 7 test cases for 4-way

## 📖 Examples

Complete Arduino examples are provided for all width variants:

- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_SingleSPI_ISR/`** - 1-way example
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_DualSPI_ISR/`** - 2-way example
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_QuadSPI_ISR/`** - 4-way example
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_SPI_ISR/`** - 8-way example

Each example includes:
- Pin configuration display
- Test data initialization
- ISR setup and execution
- Transfer completion monitoring
- Optional GPIO event log validation (define `FL_SPI_ISR_VALIDATE`)

## 🔧 Technical Details

### ISR Core Design

The ISR engine uses a **256-entry Look-Up Table (LUT)** that maps each possible byte value to GPIO SET/CLEAR masks:

```cpp
struct PinMaskEntry {
    uint32_t set_mask;    // GPIO pins to set high
    uint32_t clear_mask;  // GPIO pins to clear low
};

PinMaskEntry g_lut[256];  // LUT for all byte values (0x00-0xFF)
```

For each byte transmitted:
1. ISR reads byte value from data buffer
2. Looks up SET/CLEAR masks in LUT
3. Writes masks to GPIO registers (no branching!)
4. Toggles clock pin
5. Advances to next byte

### Platform Abstraction

GPIO writes are abstracted through macros that select ESP32 hardware or host simulation:

```cpp
#ifdef FASTLED_SPI_HOST_SIMULATION
    // Host: Capture to ring buffer
    #define FL_GPIO_WRITE_SET(mask)   fl_gpio_sim_write_set(mask)
    #define FL_GPIO_WRITE_CLEAR(mask) fl_gpio_sim_write_clear(mask)
#else
    // ESP32: Direct MMIO write
    #define FL_GPIO_WRITE_SET(mask) \
        (*(volatile uint32_t*)(uintptr_t)FASTLED_GPIO_W1TS_ADDR = (mask))
    #define FL_GPIO_WRITE_CLEAR(mask) \
        (*(volatile uint32_t*)(uintptr_t)FASTLED_GPIO_W1TC_ADDR = (mask))
#endif
```

### Performance Characteristics

- **Zero volatile reads**: ISR only writes, never reads (eliminates stalls)
- **No branching**: LUT lookup is direct, no conditional jumps
- **Minimal jitter**: High-priority ISR with predictable execution time
- **Configurable timing**: Timer frequency adjusts SPI bit rate

**Typical Configuration**:
- Timer: 1.6MHz
- SPI bit rate: 800kHz (2 ISR calls per bit: set data + clock)
- Byte rate: ~100 kB/s per data pin

## 🎯 Design Goals

1. **Code Reusability**: Single ISR core for all widths
2. **Platform Portability**: Same code runs on ESP32 and host
3. **Testability**: Host simulation enables fast, deterministic testing
4. **Performance**: Zero-read ISR with LUT-based operation
5. **Maintainability**: Width-specific code isolated to LUT initialization

## 🔄 Relationship to Hardware SPI

### SPIBusManager Hierarchy

FastLED's `SPIBusManager` automatically selects the best SPI implementation:

1. **Hardware SPI Preferred** (when available):
   - ESP32-S3: Hardware Quad-SPI for 3-4 devices
   - ESP32-C3: Hardware Dual-SPI for 2 devices
   - ESP32: Hardware Single-SPI for 1 device

2. **Software ISR Fallback** (when needed):
   - Hardware unavailable on platform
   - Hardware initialization fails
   - All hardware SPI buses exhausted
   - Manual override for testing

### Primary Use Cases for Software ISR

1. **Host-side testing**: Mock ESP32 behavior for unit tests
2. **Validation**: Verify ISR logic before hardware deployment
3. **Fallback**: When hardware SPI unavailable
4. **Low-level debugging**: Inspect GPIO events with ring buffer

## 📋 Test Running Environment (QEMU)

### QEMU RISC-V Testing

To test examples in QEMU:

```bash
# Install QEMU for ESP32 (one-time setup)
uv run ci/install-qemu.py

# Run example in QEMU
uv run test.py --qemu esp32c3 Esp32C3_SingleSPI_ISR
uv run test.py --qemu esp32c3 Esp32C3_DualSPI_ISR
uv run test.py --qemu esp32c3 Esp32C3_QuadSPI_ISR
```

Examples produce serial output that can be regex-matched for pass/fail validation.

## 📚 Additional Documentation

- **`LOOP.md`** (project root) - Project iteration loop and task tracking
- **`FEATURE_QUAD_SPI_EXTRA.md`** - Hardware Quad-SPI design documentation
- **`TASK.md`** - QEMU infrastructure and testing

## 🏆 Success Metrics

- ✅ **26 unit tests passing** across all widths
- ✅ **4 complete implementations** (1/2/4/8-way)
- ✅ **Host simulation working** with ring buffer capture
- ✅ **Examples created** for all widths
- ✅ **Same ISR core** reused across all variants

## 🤝 Contributing

When adding new width variants or modifying the ISR engine:

1. Ensure host simulation tests pass: `uv run test.py test_parallel_spi_isr_*`
2. Follow existing code patterns (see Quad-SPI wrapper as reference)
3. Update this README with any architectural changes
4. Add unit tests for new functionality

---

**Last Updated**: 2025-10-12
**Status**: Production Ready ✅
