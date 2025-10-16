# Multi-Width Software SPI: ISR and Blocking Implementations

Software-based parallel SPI drivers for ESP32 platforms with two implementation strategies:
- **ISR-based** (async, non-blocking via interrupts)
- **Main thread blocking** (inline bit-banging, lower overhead)

Both implementations use identical bit-banging logic - only the execution context differs.

## 📐 Architecture Overview

### Two Implementation Strategies

FastLED provides two software SPI implementations using the same bit-banging logic:

1. **ISR-based SPI** (`*_isr_*.hpp`): Async execution via timer interrupts
   - Non-blocking operation (main thread remains responsive)
   - Uses timer ISR to drive bit-banging at ~1.6MHz
   - Good for complex applications with multiple concurrent tasks

2. **Blocking SPI** (`*_blocking_*.hpp`): Inline execution on main thread
   - Simpler API (no ISR setup/teardown)
   - Lower overhead (no interrupt context switching)
   - Better timing precision (no interrupt jitter)
   - Good for straightforward LED updates

### Core Design Principle

Both implementations share the **same bit-banging logic** and **256-entry LUT design**. Only the execution context differs (ISR vs main thread), making the implementation highly maintainable and consistent.

```
┌─────────────────────────────────────────────────────────────────┐
│          Shared Bit-Banging Logic (256-entry LUT)               │
│  - GPIO SET/CLEAR mask lookup                                   │
│  - Platform-agnostic (ESP32 & host)                             │
│  - Zero volatile reads, write-only                              │
└────────────────┬───────────────────────────┬────────────────────┘
                 │                           │
     ┌───────────▼────────────┐    ┌────────▼─────────────┐
     │   ISR Implementation   │    │ Blocking Implementation│
     │  - Timer-driven ISR    │    │  - Main thread inline  │
     │  - Async/non-blocking  │    │  - Synchronous/blocking│
     │  - ~1.6MHz ISR tick    │    │  - No ISR overhead     │
     └───────────┬────────────┘    └────────┬───────────────┘
                 │                           │
     ┌───────────▼────────────────────────┐ │
     │   ISR Width Wrappers               │ │
     │  - SpiIsr1  (1-way)                │ │
     │  - SpiIsr2  (2-way)                │ │
     │  - SpiIsr4  (4-way)                │ │
     │  - SpiIsr8  (8-way) ✨             │ │
     └────────────────────────────────────┘ │
                                            │
              ┌─────────────────────────────▼────────────┐
              │   Blocking Width Wrappers                │
              │  - SpiBlock1  (1-way) ✅                 │
              │  - SpiBlock2  (2-way) ✅                 │
              │  - SpiBlock4  (4-way) ✅                 │
              │  - SpiBlock8  (8-way) ✨ NEW!           │
              └──────────────────────────────────────────┘
```

## 📊 Width Variants & Implementation Matrix

### ISR-Based Implementations (Async, Non-Blocking)

| Width | Data Pins | Clock | Class Name | Header File |
|-------|-----------|-------|------------|-------------|
| 1-way | 1 (D0) | 1 | `SpiIsr1` | `spi_isr_1.h` |
| 2-way | 2 (D0-D1) | 1 | `SpiIsr2` | `spi_isr_2.h` |
| 4-way | 4 (D0-D3) | 1 | `SpiIsr4` | `spi_isr_4.h` |
| 8-way | 8 (D0-D7) | 1 | `SpiIsr8` | `spi_isr_8.h` |

### Blocking Implementations (Inline, Main Thread)

| Width | Data Pins | Clock | Class Name | Header File |
|-------|-----------|-------|------------|-------------|
| 1-way | 1 (D0) | 1 | `SpiBlock1` | `spi_block_1.h` |
| 2-way | 2 (D0-D1) | 1 | `SpiBlock2` | `spi_block_2.h` |
| 4-way | 4 (D0-D3) | 1 | `SpiBlock4` | `spi_block_4.h` |
| 8-way | 8 (D0-D7) | 1 | `SpiBlock8` ✨ | `spi_block_8.h` ✨ |

### When to Use Each Width

- **1-way (Single-SPI)**: Ideal for simple applications, baseline testing, or when GPIO pins are scarce.
- **2-way (Dual-SPI)**: Matches hardware Dual-SPI topology on ESP32-C2/C3/C6/H2 platforms.
- **4-way (Quad-SPI)**: Matches hardware Quad-SPI topology on ESP32/S2/S3/P4 platforms. Balances parallelism with pin usage.
- **8-way (Octo-SPI)**: Maximum throughput, drives 8 LED strips simultaneously.

### ISR vs Blocking: When to Use Each

**Choose ISR-based SPI when:**
- ✅ You need non-blocking LED updates
- ✅ Main thread must remain responsive during transmission
- ✅ Complex application with multiple concurrent tasks
- ✅ Async operation is preferred

**Choose Blocking SPI when:**
- ✅ Simple LED update patterns
- ✅ Lower overhead needed (no ISR context switching)
- ✅ Blocking during LED update is acceptable
- ✅ Better timing precision required (no interrupt jitter)
- ✅ Simpler code/API preferred

## 🔑 Key Files

### ISR Implementation
- **`spi_isr_engine.h`** - C interface for ISR engine (formerly `fl_parallel_spi_isr_rv.h`)
- **`spi_isr_engine.cpp`** - RISC-V optimized ISR implementation
- **`spi_isr_1.h`** - 1-way ISR wrapper (formerly `parallel_spi_isr_single_esp32c3.hpp`)
- **`spi_isr_2.h`** - 2-way ISR wrapper (formerly `parallel_spi_isr_dual_esp32c3.hpp`)
- **`spi_isr_4.h`** - 4-way ISR wrapper (formerly `parallel_spi_isr_quad_esp32c3.hpp`)
- **`spi_isr_8.h`** - 8-way ISR wrapper ✨ (formerly `fastled_parallel_spi_esp32c3.hpp`)

### Blocking Implementation
- **`spi_block_1.h`** - 1-way blocking inline bit-banger (formerly `parallel_spi_blocking_single.hpp`)
- **`spi_block_2.h`** - 2-way blocking inline bit-banger (formerly `parallel_spi_blocking_dual.hpp`)
- **`spi_block_4.h`** - 4-way blocking inline bit-banger (formerly `parallel_spi_blocking_quad.hpp`)
- **`spi_block_8.h`** - 8-way blocking inline bit-banger ✨ (NEW!)

### Platform Support
- **`spi_platform.h`** - Platform abstraction layer (ESP32 vs host)
- **`../esp/32/spi_platform_esp32.cpp`** - ESP32 platform-specific implementation

### Host Simulation (Testing)
- **`host_sim.h`** - Host simulation API (formerly `fl_parallel_spi_host_sim.h`)
- **`host_sim.cpp`** - Ring buffer capture for GPIO events
- **`host_timer.cpp`** - Timer simulation for testing

## 🚀 Usage Examples

### Blocking SPI (Simpler API, Inline Execution)

```cpp
#include "platforms/shared/spi_bitbang/spi_block_4.h"

using namespace fl;

void setup() {
    // Create 4-way SPI blocking driver instance
    SpiBlock4 spi;

    // Configure pin mapping (4 data pins + 1 clock)
    spi.setPinMapping(0, 1, 2, 3, 8);  // D0-D3 = GPIO0-3, CLK = GPIO8

    // Prepare data buffer
    uint8_t data[4] = {0x0F, 0x0A, 0x05, 0x00};
    spi.loadBuffer(data, 4);

    // Transmit (blocks until complete - inline bit-banging)
    spi.transmit();

    // Done! No ISR setup/teardown needed.
}
```

### ISR-Based SPI (Async, Non-Blocking)

```cpp
#include "platforms/shared/spi_bitbang/spi_isr_4.h"

using namespace fl;

void setup() {
    // Create 4-way SPI ISR driver instance
    SpiIsr4 spi;

    // Configure pin mapping (4 data pins + 1 clock)
    spi.setPinMapping(0, 1, 2, 3, 8);  // D0-D3 = GPIO0-3, CLK = GPIO8

    // Prepare data buffer
    uint8_t data[4] = {0x0F, 0x0A, 0x05, 0x00};
    spi.loadBuffer(data, 4);

    // Setup ISR (1.6MHz timer → 800kHz SPI bit rate)
    spi.setupISR(1600000);

    // Wait for memory visibility
    SpiIsr4::visibilityDelayUs(10);

    // Start transfer (non-blocking - ISR handles transmission)
    spi.arm();

    // Wait for completion (main thread can do other work here)
    while (spi.isBusy()) {
        delay(1);
    }

    // Acknowledge completion
    spi.ackDone();

    // Stop ISR
    spi.stopISR();
}
```

## 📚 API Reference

### Blocking SPI API (All Widths)

```cpp
// Pin configuration (varies by width)
void setPinMapping(...);  // 1-way: (data, clk)
                          // 2-way: (d0, d1, clk)
                          // 4-way: (d0, d1, d2, d3, clk)

// Data transfer (simple blocking API)
void loadBuffer(const uint8_t* data, uint16_t n);  // Load up to 256 bytes
void transmit();                                    // Block and transmit inline
```

**Key Features:**
- Simpler API (only 3 methods needed)
- No ISR setup/teardown
- Blocks until transmission complete
- Lower overhead than ISR
- Better timing precision

### ISR-Based SPI API (All Widths)

```cpp
// Pin configuration (varies by width)
void setPinMapping(...);  // 1-way: (data, clk)
                          // 2-way: (d0, d1, clk)
                          // 4-way: (d0, d1, d2, d3, clk)
                          // 8-way: (d0-d7, clk)

// Data transfer (async API)
void loadBuffer(const uint8_t* data, uint16_t n);  // Load up to 256 bytes
int setupISR(uint32_t timer_hz);                   // Setup timer ISR
void arm();                                         // Start async transfer
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

**Key Features:**
- Non-blocking operation
- Main thread remains responsive
- Requires ISR setup/management
- Good for complex applications

## 🧪 Testing Infrastructure

### Host Simulation

Both ISR and blocking implementations include **host simulation** for testing on development machines without ESP32 hardware:

- **Ring Buffer Capture**: All GPIO write operations are captured
- **Manual Tick Control**: ISR tests drive execution tick-by-tick for deterministic results
- **Event Inspection**: Tests examine GPIO events to verify correctness
- **Same Code Paths**: Identical bit-banging logic runs on both ESP32 and host

### Running Tests

```bash
# Blocking SPI tests (18 test cases total)
uv run test.py test_spi_blocking  # All blocking variants (single/dual/quad)

# ISR-based SPI tests (26 test cases total)
uv run test.py test_parallel_spi_isr_single  # 1-way ISR tests (10 test cases)
uv run test.py test_parallel_spi_isr_dual    # 2-way ISR tests (9 test cases)
uv run test.py test_parallel_spi_isr_quad    # 4-way ISR tests (7 test cases)

# Test coverage includes:
# - Basic transmission
# - Clock toggling verification
# - Data pattern verification (all fundamental patterns)
# - Multi-byte sequences
# - Edge cases (zero bytes, max bytes, etc.)
# - LUT initialization correctness
# - Multiple pin configurations
```

### Test Files

**Blocking SPI Tests:**
- **`tests/test_spi_blocking.cpp`** - 18 test cases covering all blocking variants
  - 5 single-lane tests
  - 6 dual-lane tests
  - 7 quad-lane tests

**ISR-Based SPI Tests:**
- **`tests/test_parallel_spi_isr_single.cpp`** - 10 test cases for 1-way ISR
- **`tests/test_parallel_spi_isr_dual.cpp`** - 9 test cases for 2-way ISR
- **`tests/test_parallel_spi_isr_quad.cpp`** - 7 test cases for 4-way ISR

## 📖 Examples

Complete Arduino examples are provided for both ISR and blocking implementations:

### Blocking SPI Examples (Simple, Inline Execution)
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_SingleSPI_Blocking/`** - 1-way blocking
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_DualSPI_Blocking/`** - 2-way blocking
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_QuadSPI_Blocking/`** - 4-way blocking

Each blocking example demonstrates:
- Pin configuration display
- Test pattern data (all fundamental bit patterns)
- Simple `transmit()` call
- Transmission timing measurement
- Effective bit rate calculation

### ISR-Based SPI Examples (Async, Non-Blocking)
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_SingleSPI_ISR/`** - 1-way ISR
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_DualSPI_ISR/`** - 2-way ISR
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_QuadSPI_ISR/`** - 4-way ISR
- **`examples/SpecialDrivers/ESP/ParallelSPI/Esp32C3_SPI_ISR/`** - 8-way ISR

Each ISR example includes:
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
   - ESP32-P4: Hardware Octal-SPI for up to 8 devices (IDF 5.0+)
   - ESP32-S3: Hardware Quad-SPI for 3-4 devices
   - ESP32-C3: Hardware Dual-SPI for 2 devices
   - ESP32: Hardware Single-SPI for 1 device

2. **Software ISR Fallback** (when needed):
   - Hardware unavailable on platform
   - Hardware initialization fails
   - All hardware SPI buses exhausted
   - Manual override for testing

### Hardware Octal-SPI (ESP32-P4 + IDF 5.0+)

The ESP32-P4 supports **hardware octal-SPI** for driving up to 8 parallel LED strips simultaneously via DMA. This is ~40× faster than software implementations with zero CPU usage during transmission.

**Key Differences from Software Implementations:**
- **Performance**: DMA-accelerated, completes 8×100 LED strips in <500µs
- **CPU Usage**: 0% during transmission (vs. 100% for blocking, or ISR overhead)
- **Platform**: ESP32-P4 only with IDF 5.0+ (older platforms use software fallback)
- **Implementation**: Located in `src/platforms/esp/32/spi_hw_8_esp32.cpp` and `src/platforms/shared/spi_transposer.*`

For detailed hardware octal-SPI information, see:
- `src/platforms/esp/32/README.md` - Hardware octal-SPI overview
- `src/platforms/shared/README.md` - Transposer infrastructure

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

### ISR-Based Implementation
- ✅ **26 unit tests passing** across all ISR widths (1/2/4/8-way)
- ✅ **4 complete implementations** (single/dual/quad/octo)
- ✅ **Host simulation working** with ring buffer capture
- ✅ **Examples created** for all ISR widths

### Blocking Implementation
- ✅ **18 unit tests passing** across all blocking widths (1/2/4-way)
- ✅ **3 complete implementations** (single/dual/quad)
- ✅ **Host simulation compatibility** with same test infrastructure
- ✅ **Examples created** for all blocking widths

### Combined
- ✅ **44 total unit tests** validating both implementations
- ✅ **7 complete driver variants** (4 ISR + 3 blocking)
- ✅ **Shared bit-banging logic** between ISR and blocking
- ✅ **Both implementations coexist** peacefully

## 🤝 Contributing

When adding new width variants or modifying implementations:

1. Ensure all tests pass:
   - `uv run test.py test_spi_blocking` (blocking variants)
   - `uv run test.py test_parallel_spi_isr_*` (ISR variants)
2. Follow existing code patterns:
   - ISR: See `parallel_spi_isr_quad_esp32c3.hpp` as reference
   - Blocking: See `parallel_spi_blocking_quad.hpp` as reference
3. Update this README with any architectural changes
4. Add unit tests for new functionality

## 📊 Performance Comparison: ISR vs Blocking

### Performance Characteristics

| Characteristic | ISR-Based | Blocking (Inline) |
|----------------|-----------|-------------------|
| **Execution Context** | Timer ISR (~1.6MHz) | Main thread inline |
| **Overhead** | ISR context switch | None (inline) |
| **Latency** | ISR entry delay | Zero (immediate) |
| **Jitter** | Interrupt scheduling | None (deterministic) |
| **Main Thread** | Non-blocking | Blocks during TX |
| **API Complexity** | Higher (7+ methods) | Lower (3 methods) |
| **Use Case** | Complex apps | Simple apps |
| **Bit Rate** | ~800kHz effective | Higher potential |

### Expected Performance Gains (Blocking over ISR)

- **Lower Latency**: No ISR entry overhead (~20-30 CPU cycles saved)
- **Higher Throughput**: No interrupt scheduling delays
- **Better Precision**: No interrupt jitter or priority conflicts
- **Simpler Code**: Fewer API calls, no ISR management

### Trade-offs

**ISR Advantages:**
- Main thread remains responsive
- Good for multi-tasking applications
- Well-tested in production

**Blocking Advantages:**
- Simpler to understand and use
- Lower CPU overhead
- More predictable timing
- Easier to debug

---

**Last Updated**: 2025-10-13
**Status**: Production Ready ✅ (Both ISR and Blocking Implementations)
