# Feature Request: Hardware Quad-SPI for Parallel LED Control

**Status**: 🚧 **In Progress** - Phase 4 Documentation & Examples Complete (Iteration 29/50)

**Based on**: ESP32 Quad-SPI Parallel LED Output Capabilities (QUAD_SPI_ESP32.md)
**Original Software SPI Feature**: FEATURE.md
**Date**: October 8, 2025
**Last Updated**: January 7, 2025 (Iteration 29)

**Latest Progress**: See [QUAD_SPI_PROGRESS.md](QUAD_SPI_PROGRESS.md) for detailed implementation report

**Current Milestone**:
- ✅ Phases 0-3: Core bit-interleaving logic (11 tests passing)
- ✅ Phase 4 (partial): Controller integration, examples, documentation (24 tests total)
- 🚧 Phase 4 (remaining): calculateBytes() helpers, ESP32 hardware integration
- ⏸️ Phase 5: Hardware validation (pending ESP32-P4 access)

---

## 📋 Document Index

This large feature document has been split into focused sub-documents for easier navigation:

| Document | Status | Description |
|----------|--------|-------------|
| **[FEAT_TDD_TESTS.md](FEAT_TDD_TESTS.md)** | ✅ Phases 0-3 Complete | TDD strategy, test infrastructure, mock drivers, unit test specs |
| **[FEAT_IMPLEMENTATION.md](FEAT_IMPLEMENTATION.md)** | 🚧 In Progress (Phase 4) | Architecture, padding strategy, platform support, performance details |
| **[FEAT_DEMO_CODE.md](FEAT_DEMO_CODE.md)** | 📚 Reference | Working demo code, bit-interleaving algorithms, ESP32 examples |

---

## 🎯 At a Glance

**Approach**: Hardware quad-SPI DMA-driven parallel LED control for ESP32
- ✅ Zero CPU overhead (DMA handles transmission)
- ✅ 4-8× faster than software SPI
- ✅ Supports up to 8 LED strips (4 per SPI bus × 2 buses on ESP32/S2/S3)
- ✅ Bit-interleaving multiplexes 4 data streams onto quad-SPI hardware

**Memory Architecture** (Simplified):
```
Per-lane capture:  fl::vector<uint8_t> × 4 lanes
                   Each stores: [preamble + payload + postamble]
                   ↓
Transpose:         Bit-interleave to single DMA buffer
                   ↓
DMA transmission:  fl::vector<uint8_t> (bit-interleaved format)
```

**Three-Phase Workflow**:
1. **Pre-scan** → Calculate exact size per lane, find max, pre-allocate buffers with protocol-safe padding
2. **Capture** → Controllers write protocol data to per-lane buffers (padding region untouched)
3. **Transpose** → Bit-interleave to DMA buffer (all lanes same size via pre-padding)

**Key Insight**: Pre-allocated buffers with protocol-safe padding eliminate runtime padding logic and ensure protocol correctness.

---

## 📊 Progress Status

### Phase 0: Test Infrastructure Setup
**Status**: ✅ **Complete**

**Deliverables**:
- [x] Mock Quad SPI Driver Interface (`tests/mocks/mock_quad_spi.h`)
- [x] Test Helper Functions (`tests/helpers/apa102_test_helpers.h`)

**Details**: See [FEAT_TDD_TESTS.md - Phase 0](FEAT_TDD_TESTS.md#phase-0-test-infrastructure-setup)

---

### Phase 1: Core Transpose Logic (TDD)
**Status**: ✅ **Complete**

**Deliverables**:
- [x] Test 1: Basic 4-Lane APA102 Transpose
- [x] Test 2: Inverse Transpose (Round-Trip Validation)
- [x] Test 3: Bit-Level Pattern Verification
- [x] QuadSPITransposer class (`src/fl/quad_spi_transposer.h`)

**Details**: See [FEAT_TDD_TESTS.md - Phase 1](FEAT_TDD_TESTS.md#phase-1-core-transpose-logic-red--green--refactor)

---

### Phase 2: Mock Hardware Integration
**Status**: ✅ **Complete**

**Deliverables**:
- [x] Test 4: Simulated DMA Transmission
- [x] MockQuadSPIDriver implementation
- [x] Complete integration tests with round-trip validation

**Details**: See [FEAT_TDD_TESTS.md - Phase 2](FEAT_TDD_TESTS.md#phase-2-mock-hardware-integration)

---

### Phase 3: Multi-Chipset Support
**Status**: ✅ **Complete**

**Deliverables**:
- [x] Test 5: LPD8806 with 0x00 padding
- [x] WS2801 chipset tests with 0x00 padding
- [x] P9813 chipset tests with 0x00 padding
- [x] Mixed chipset validation test
- [x] Helper functions for all chipsets (LPD8806, WS2801, P9813)

**Details**: See [FEAT_TDD_TESTS.md - Phase 3](FEAT_TDD_TESTS.md#phase-3-multi-chipset-support)

---

### Phase 4: Implementation & Integration
**Status**: 🚧 **In Progress** (Iteration 29/50)

**Prerequisites**: ✅ All tests in Phases 0-3 passing

**Core Components Complete** ✅:
- [x] QuadSPITransposer class (bit-interleaving logic) - `src/fl/quad_spi_transposer.h`
- [x] MockQuadSPIDriver (hardware simulation) - `tests/mocks/mock_quad_spi_driver.h`
- [x] Multi-chipset padding support - All 4 chipsets validated
- [x] Comprehensive test coverage (24 test cases total)
- [x] Platform detection macros - `src/fl/quad_spi_platform.h`
- [x] QuadSPIController base class - `src/platforms/esp/32/quad_spi_controller.h`
- [x] Controller `getPaddingByte()` methods - APA102, LPD8806, WS2801, P9813 (in `src/chipsets.h`)
- [x] Integration tests - `tests/test_quad_spi_integration.cpp` (6 tests)
- [x] Example sketches - QuadSPI_Basic.ino and QuadSPI_MultiChipset.ino
- [x] Comprehensive documentation - All headers fully documented

**Test Status** ✅:
- `test_quad_spi_transpose.cpp`: 11 tests (Phases 0-3) - Core bit-interleaving
- `test_chipset_padding.cpp`: 7 tests (Phase 4) - ✅ PASSING
- `test_quad_spi_integration.cpp`: 6 tests (Phase 4) - End-to-end validation
- **Total**: 24 test cases, ~2000 lines of code

**Remaining Work** (Requires ESP32 Hardware):
- [ ] Add `calculateBytes()` helper methods to controller classes
- [ ] ESP32 SPI peripheral configuration (VSPI/HSPI in quad mode)
- [ ] DMA channel allocation and buffer management
- [ ] Complete QuadSPIController hardware implementation
- [ ] Performance benchmark tests

**Details**: See [FEAT_IMPLEMENTATION.md](FEAT_IMPLEMENTATION.md)

**Progress**: Documentation and examples complete. Hardware integration pending ESP32-P4 access.

---

### Phase 5: Hardware Validation
**Status**: ⏸️ **Blocked by Implementation**

**Prerequisites**: Phase 4 complete

**Deliverables**:
- [ ] Flash to ESP32 hardware
- [ ] Logic analyzer verification
- [ ] Visual LED inspection
- [ ] Performance benchmarks

---

## 🚀 Summary

Implement **hardware quad-SPI DMA-driven** parallel LED control for ESP32 family chips. This leverages the SPI peripheral's multi-data-line capability (MOSI/D0, MISO/D1, WP/D2, HD/D3) to drive 4 LED strips simultaneously with shared clock and zero CPU overhead.

**Key architectural decisions:**
- **Proxy pattern** from software multi-lane SPI for capture mode
- **Pre-allocated buffers with protocol-safe padding** to handle variable-length lanes
- **Minimal controller changes**: Only add `getPaddingByte()` static method
- **Type-safe padding**: Compile-time resolution via templates

---

## 📖 Core Concept

> "Hardware quad-SPI is 4-8× faster than optimized soft-SPI while using zero CPU cycles during transmission"
> — QUAD_SPI_ESP32.md

Unlike software SPI which requires CPU-spinning bit-banging, hardware quad-SPI uses the ESP32's SPI peripheral with DMA:
- **4 data lines** transmit in parallel per SPI bus
- **DMA transfers** data from memory to SPI peripheral (zero CPU load)
- **Hardware clock generation** ensures precise timing without jitter
- **8 total strips** supported (ESP32/S2/S3: HSPI + VSPI, each with 4 lanes)

---

## 🖥️ Platform Capabilities

### Full Quad-SPI Support (4 channels per bus)

| Platform | SPI Buses | Channels/Bus | Total Parallel Strips | Max Clock Speed |
|----------|-----------|--------------|----------------------|-----------------|
| **ESP32** | HSPI + VSPI | 4 (quad) | **8 strips** | 40-80 MHz |
| **ESP32-S2** | SPI2 + SPI3 | 4 (quad) | **8 strips** | 40-80 MHz |
| **ESP32-S3** | SPI2 + SPI3 | 4 (quad) | **8 strips** | 40-80 MHz |

### Dual-SPI Support (2 channels per bus)

| Platform | SPI Buses | Channels/Bus | Total Parallel Strips | Max Clock Speed |
|----------|-----------|--------------|----------------------|-----------------|
| **ESP32-C3** | SPI2 | 2 (dual) | **2 strips** | 40-80 MHz |
| **ESP32-C2** | SPI2 | 2 (dual) | **2 strips** | 40-60 MHz |
| **ESP32-C6** | SPI2 | 2-4 (TBD) | **2-4 strips** | 40-80 MHz |
| **ESP32-H2** | SPI2 | 2 (dual) | **2 strips** | 40-60 MHz |

**Details**: See [FEAT_IMPLEMENTATION.md - Platform Capabilities](FEAT_IMPLEMENTATION.md#platform-capabilities)

---

## 💡 Supported Chipsets

Hardware quad-SPI works with SPI-based LED chipsets:

- **APA102/SK9822** (Dotstar) - 4-wire SPI, up to 40 MHz
- **LPD8806** - SPI-based LED driver (2 MHz max clock)
- **P9813** (Total Control Lighting) - SPI protocol
- **WS2801** - SPI-compatible LED chipset (up to 25 MHz)
- **HD107** - High-speed APA102 variant (40 MHz)

### Chipset Padding Bytes

| Chipset | Safe Padding Byte | Reason |
|---------|-------------------|--------|
| APA102/SK9822 | **0xFF** | End frame continuation |
| LPD8806 | **0x00** | Latch continuation |
| WS2801 | **0x00** | No protocol state |
| P9813 | **0x00** | Boundary byte |
| HD107 | **0xFF** | End frame continuation |

**Details**: See [FEAT_IMPLEMENTATION.md - Supported Chipsets](FEAT_IMPLEMENTATION.md#supported-chipsets)

---

## ⚙️ Technical Requirements

1. **Hardware Quad-SPI with DMA**
   - ESP32 SPI peripheral configured for quad-line mode (SPI_QUAD_MODE)
   - DMA channels allocated for zero-copy asynchronous transmission
   - Buffers allocated in DMA-capable memory (`MALLOC_CAP_DMA`)

2. **Bit Interleaving for Quad-SPI**
   - Data from 4 independent LED strips bit-interleaved into single buffer
   - Hardware clocks out each interleaved byte, sending bits simultaneously

3. **Multi-Lane Architecture**
   - Support 4 parallel SPI data lanes per bus (quad-SPI)
   - Support 2 parallel SPI data lanes per bus (dual-SPI on C-series)
   - Pre-allocated buffers with protocol-safe padding

**Details**: See [FEAT_IMPLEMENTATION.md - Technical Requirements](FEAT_IMPLEMENTATION.md#technical-requirements)

---

## 🎨 User Experience

```cpp
// ESP32: Automatic quad-SPI (no user action needed)
CRGB leds1[100], leds2[150], leds3[80], leds4[200];

void setup() {
    // All share same CLOCK_PIN → automatic quad-SPI bulk mode
    FastLED.addLeds<APA102, DATA_PIN_1, CLOCK_PIN, RGB>(leds1, 100);
    FastLED.addLeds<APA102, DATA_PIN_2, CLOCK_PIN, RGB>(leds2, 150);
    FastLED.addLeds<APA102, DATA_PIN_3, CLOCK_PIN, RGB>(leds3, 80);
    FastLED.addLeds<APA102, DATA_PIN_4, CLOCK_PIN, RGB>(leds4, 200);
}

void loop() {
    fill_rainbow(leds1, 100, 0, 7);
    fill_rainbow(leds2, 150, 0, 7);
    fill_rainbow(leds3, 80, 0, 7);
    fill_rainbow(leds4, 200, 0, 7);

    FastLED.show();  // DMA handles transmission (zero CPU load!)
}
```

---

## 📈 Performance Characteristics

**Transmission Time Example** (100 LEDs, 40 MHz quad-SPI clock):
- APA102: ~404 bytes
- At 40 MHz: **0.08 milliseconds**
- **4 lanes in parallel**: Same 0.08ms for all 4 strips
- **Zero CPU load**: DMA handles transmission

**Comparison with Software SPI**:

| Approach | Clock Speed | CPU Usage | 4×100 LED Strips |
|----------|-------------|-----------|------------------|
| Software SPI | 6-12 MHz | 100% (blocking) | ~2.16ms |
| **Hardware Quad-SPI** | 40-80 MHz | **0% (DMA)** | **~0.08ms** |

**Speedup**: **~27× faster** with **zero CPU load**!

**Details**: See [FEAT_IMPLEMENTATION.md - Performance](FEAT_IMPLEMENTATION.md#performance-characteristics)

---

## 🔮 Future Goals

### Phase 1: Software SPI Fallback for Non-ESP32 Platforms
**Status**: ⏸️ Documented in FEATURE.md

### Phase 2: Octal-SPI Support (ESP32-P4)
**Status**: 🔮 Future enhancement

### Phase 3: Double-Buffering for Glitch-Free Updates
**Status**: 🔮 Future enhancement

### Phase 4: Hardware SPI on Other Platforms
**Status**: 🔮 Investigation needed

**Details**: See [FEAT_IMPLEMENTATION.md - Future Goals](FEAT_IMPLEMENTATION.md#future-goals)

---

## 🧪 Development Approach

**CRITICAL**: This feature MUST be developed using strict TDD methodology.

### Test Execution Plan

1. **Phase 0**: Create mocks and helpers
   - Run: `uv run test.py test_quad_spi_setup`

2. **Phase 1**: Implement transpose logic (TDD cycle)
   - Run: `uv run test.py test_quad_spi_transpose_apa102`

3. **Phase 2**: Mock driver integration
   - Run: `uv run test.py test_quad_spi_mock_driver`

4. **Phase 3**: Multi-chipset tests
   - Run: `uv run test.py --cpp`

5. **Phase 4**: Hardware validation
   - Flash to ESP32
   - Logic analyzer verification

**Details**: See [FEAT_TDD_TESTS.md](FEAT_TDD_TESTS.md)

---

## 🔗 Related Documents

- **[QUAD_SPI_ESP32.md](QUAD_SPI_ESP32.md)**: ESP32 hardware capabilities research
- **[FEATURE.md](FEATURE.md)**: Software multi-lane SPI (fallback implementation)
- **Implementation Details**: [FEAT_IMPLEMENTATION.md](FEAT_IMPLEMENTATION.md)
- **Test Strategy**: [FEAT_TDD_TESTS.md](FEAT_TDD_TESTS.md)
- **Demo Code**: [FEAT_DEMO_CODE.md](FEAT_DEMO_CODE.md)

---

## ✅ Key Benefits

- ✅ **Protocol-safe**: Each chipset defines its own safe padding byte
- ✅ **Minimal controller changes**: Only add `getPaddingByte()` static method
- ✅ **Type-safe**: Resolved at compile time via templates
- ✅ **Performance**: No conditionals in interleaving loop (cache-friendly)
- ✅ **Zero CPU overhead**: DMA handles all data transfer
- ✅ **4-8× faster**: 40-80 MHz vs 6-12 MHz software SPI
- ✅ **Automatic fallback**: Software SPI on non-ESP32 platforms

---

## 📝 Quick Reference

### Status Symbols
- ✅ Complete
- ⏸️ Planning/Blocked
- ❌ Not Started
- 🔮 Future Enhancement
- 📚 Reference Material

### Command Quick Reference
```bash
# Run all tests
uv run test.py --cpp

# Run specific test
uv run test.py test_quad_spi_transpose_apa102

# Run with QEMU (ESP32 emulation)
uv run test.py --qemu esp32s3

# Code linting
bash lint
```

---

**Last Updated**: 2025-10-07
**Version**: 1.0 (Split into sub-documents)
