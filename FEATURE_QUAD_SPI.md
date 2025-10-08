# Feature Request: Hardware Quad-SPI for Parallel LED Control

## At a Glance

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

**Based on**: ESP32 Quad-SPI Parallel LED Output Capabilities (QUAD_SPI_ESP32.md)
**Original Software SPI Feature**: FEATURE.md
**Date**: October 8, 2025
**Status**: Proposal Phase

## Summary

Implement **hardware quad-SPI DMA-driven** parallel LED control for ESP32 family chips. This leverages the SPI peripheral's multi-data-line capability (MOSI/D0, MISO/D1, WP/D2, HD/D3) to drive 4 LED strips simultaneously with shared clock and zero CPU overhead.

**Key architectural decisions:**
- **Proxy pattern** from software multi-lane SPI for capture mode
- **Pre-allocated buffers with protocol-safe padding** to handle variable-length lanes
- **Minimal controller changes**: Only add `getPaddingByte()` static method
- **Type-safe padding**: Compile-time resolution via templates

## Test-Driven Development Strategy

**CRITICAL**: This feature MUST be developed using strict TDD methodology to ensure correctness before hardware integration.

### Development Phases (TDD Approach)

#### Phase 0: Test Infrastructure Setup
**Goal**: Create test harness and mock infrastructure before any implementation code

**Deliverables**:
1. **Mock Quad SPI Driver Interface** (`tests/mocks/mock_quad_spi.h`)
   - Fake SPI peripheral that captures transmitted data
   - Simulated DMA buffer inspection
   - Clock cycle tracking for timing validation
   - No actual hardware dependencies

2. **Test Helper Functions** (`tests/helpers/apa102_test_helpers.h`)
   - `generateAPA102ProtocolData(num_leds, pattern_byte)` - Creates known test patterns
   - `validateBitInterleaving(input_lanes, output_buffer)` - Verifies correct bit positions
   - `deinterleaveQuadSPI(buffer, lane_sizes)` - Inverse operation for round-trip testing

#### Phase 1: Core Transpose Logic (RED → GREEN → REFACTOR)

**Test 1: Basic 4-Lane APA102 Transpose** ✅ FIRST TEST
```cpp
// File: tests/test_quad_spi_transpose_apa102.cpp
TEST_CASE("APA102 4-lane transpose with variable LED counts") {
    // RED: Write this test FIRST (it will fail - no implementation exists yet)

    // Setup: Create APA102 data for 4 lanes with 1, 2, 4, 7 LEDs
    auto lane0 = generateAPA102ProtocolData(1, 0xAA);  // 12 bytes
    auto lane1 = generateAPA102ProtocolData(2, 0xBB);  // 16 bytes
    auto lane2 = generateAPA102ProtocolData(4, 0xCC);  // 24 bytes
    auto lane3 = generateAPA102ProtocolData(7, 0xDD);  // 36 bytes (max)

    // Action: Transpose with padding to 36 bytes
    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);  // APA102 padding byte
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();

    // Assert: Verify interleaved buffer structure
    REQUIRE(interleaved.size() == 36);  // Max lane size

    // Verify START frames (all 0x00) are correctly interleaved
    validateStartFrameInterleaving(interleaved, 0, 4);

    // Verify first LED brightness bytes (all 0xFF) are correctly interleaved
    validateBrightnessInterleaving(interleaved, 4, 8);

    // Verify padding region (lanes 0-2 should contribute 0xFF padding)
    validatePaddingRegion(interleaved, lane0.size(), lane3.size());
}
```

**GREEN**: Implement minimal `QuadSPITransposer` class to make test pass
**REFACTOR**: Optimize bit-interleaving algorithm

---

**Test 2: Inverse Transpose (Round-Trip Validation)**
```cpp
TEST_CASE("Quad SPI inverse transpose recovers original data") {
    // RED: Write test for de-interleaving
    auto [lane0_orig, lane1_orig, lane2_orig, lane3_orig] = createTestLanes();

    auto interleaved = transpose(lane0_orig, lane1_orig, lane2_orig, lane3_orig);
    auto [lane0_out, lane1_out, lane2_out, lane3_out] = deinterleave(interleaved);

    // Assert: Exact match (validates transpose is lossless)
    REQUIRE(lane0_out == lane0_orig);
    REQUIRE(lane1_out == lane1_orig);
    REQUIRE(lane2_out == lane2_orig);
    REQUIRE(lane3_out == lane3_orig);
}
```

**GREEN**: Implement `deinterleave()` function
**REFACTOR**: Ensure transpose/detranspose are symmetric

---

**Test 3: Bit-Level Pattern Verification**
```cpp
TEST_CASE("Bit interleaving produces correct nibble patterns") {
    // RED: Test with known bit patterns
    fl::vector<uint8_t> lane0 = {0b10101010};  // 0xAA
    fl::vector<uint8_t> lane1 = {0b11001100};  // 0xCC
    fl::vector<uint8_t> lane2 = {0b11110000};  // 0xF0
    fl::vector<uint8_t> lane3 = {0b11111111};  // 0xFF

    auto interleaved = transposeSimple(lane0, lane1, lane2, lane3);

    // For quad-SPI: Each output byte contains 2 bits from each lane
    // Byte 0: lane3[7:6], lane2[7:6], lane1[7:6], lane0[7:6]
    //       = 11        , 11        , 11        , 10         = 0b11111110 = 0xFE
    REQUIRE(interleaved[0] == 0xFE);

    // Continue for all 4 output bytes...
}
```

**GREEN**: Fix bit-shifting logic to match expected patterns
**REFACTOR**: Extract bit manipulation into reusable functions

#### Phase 2: Mock Hardware Integration

**Test 4: Simulated DMA Transmission**
```cpp
TEST_CASE("Mock Quad SPI driver receives correct DMA buffer") {
    // RED: Test with mock hardware driver
    MockQuadSPIDriver mock_driver;

    // Create test pattern using APA102 controller
    CRGB leds[4] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    APA102Controller<1, 2> controller;  // Mock pins

    // Capture protocol data
    controller.showPixels(leds, 4);

    // Verify mock received bit-interleaved data
    auto dma_buffer = mock_driver.getLastTransmission();
    validateAPA102DMABuffer(dma_buffer, leds, 4);
}
```

**GREEN**: Implement `MockQuadSPIDriver` with data capture
**REFACTOR**: Make mock driver reusable across chipsets

#### Phase 3: Multi-Chipset Support

**Test 5: LPD8806 with Different Padding**
```cpp
TEST_CASE("LPD8806 uses 0x00 padding byte") {
    // Test that different chipsets use protocol-safe padding
    auto lane0 = generateLPD8806ProtocolData(10);  // 30 bytes
    auto lane1 = generateLPD8806ProtocolData(20);  // 60 bytes

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0x00);  // LPD8806 padding
    transposer.addLane(1, lane1, 0x00);

    auto interleaved = transposer.transpose();

    // Verify padding region uses 0x00 (not 0xFF)
    validatePaddingByte(interleaved, lane0.size(), lane1.size(), 0x00);
}
```

### Mock Strategy: Quad SPI Driver Simulation

**Key Principle**: Test all logic WITHOUT real ESP32 hardware by faking the SPI peripheral

#### Mock Architecture

```cpp
// File: tests/mocks/mock_quad_spi_driver.h

class MockQuadSPIDriver {
private:
    fl::vector<uint8_t> mLastDMABuffer;
    uint32_t mClockSpeed;
    uint32_t mTransmissionCount;

public:
    // Fake DMA transmission (just captures data)
    void transmitDMA(const uint8_t* buffer, size_t length) {
        mLastDMABuffer.assign(buffer, buffer + length);
        mTransmissionCount++;
    }

    // Test inspection methods
    const fl::vector<uint8_t>& getLastTransmission() const {
        return mLastDMABuffer;
    }

    uint32_t getTransmissionCount() const {
        return mTransmissionCount;
    }

    // Simulate de-interleaving to verify each lane's data
    fl::vector<fl::vector<uint8_t>> extractLanes(uint8_t num_lanes) {
        fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

        // Reverse the bit-interleaving to extract per-lane data
        for (size_t byte_idx = 0; byte_idx < mLastDMABuffer.size(); ++byte_idx) {
            uint8_t interleaved_byte = mLastDMABuffer[byte_idx];

            // Extract bits for each lane (implementation depends on interleaving format)
            for (uint8_t lane = 0; lane < num_lanes; ++lane) {
                // Nibble extraction logic here
            }
        }

        return lanes;
    }

    // Timing simulation
    uint64_t estimateTransmissionTimeMicros(size_t buffer_size) {
        // Calculate based on mock clock speed
        return (buffer_size * 8 * 1000000ULL) / mClockSpeed;
    }
};
```

#### Mock Usage in Tests

```cpp
TEST_CASE("APA102 controller writes correct protocol via mock driver") {
    // Setup: Create mock driver and controller
    MockQuadSPIDriver mock;
    APA102ControllerWithMock<1, 2> controller(&mock);  // Inject mock

    // Test data
    CRGB leds[8] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White,
                     CRGB::Yellow, CRGB::Cyan, CRGB::Magenta, CRGB::Black};

    // Action: Trigger show() which should call mock's transmitDMA()
    controller.showPixels(leds, 8);

    // Assert: Verify mock received data
    REQUIRE(mock.getTransmissionCount() == 1);

    auto dma_buffer = mock.getLastTransmission();

    // Calculate expected size: START(4) + LED_DATA(8*4) + END(4) = 40 bytes
    REQUIRE(dma_buffer.size() == 40);

    // Verify START frame
    REQUIRE(dma_buffer[0] == 0x00);
    REQUIRE(dma_buffer[1] == 0x00);
    REQUIRE(dma_buffer[2] == 0x00);
    REQUIRE(dma_buffer[3] == 0x00);

    // Verify first LED (Red = 0xFF, 0x00, 0x00 with brightness 0xFF)
    REQUIRE(dma_buffer[4] == 0xFF);  // Brightness byte
    REQUIRE(dma_buffer[5] == 0xFF);  // Red
    REQUIRE(dma_buffer[6] == 0x00);  // Green
    REQUIRE(dma_buffer[7] == 0x00);  // Blue
}
```

### Benefits of This TDD Approach

✅ **Catch bugs early**: Bit-interleaving errors found in unit tests, not on hardware
✅ **Fast iteration**: No ESP32 flashing required during development
✅ **Regression prevention**: Test suite validates every code change
✅ **Documentation**: Tests serve as executable specification
✅ **Confidence**: 100% test coverage before hardware integration

### Test Execution Plan

**Phase 0**: Create mocks and helpers
- `tests/mocks/mock_quad_spi_driver.h`
- `tests/helpers/apa102_test_helpers.h`
- Run: `uv run test.py test_quad_spi_setup` (should compile but have no tests yet)

**Phase 1**: Implement transpose logic (TDD cycle)
- Write `test_quad_spi_transpose_apa102.cpp` (RED - fails)
- Implement `QuadSPITransposer` (GREEN - passes)
- Optimize (REFACTOR)
- Run: `uv run test.py test_quad_spi_transpose_apa102`

**Phase 2**: Mock driver integration
- Write `test_quad_spi_mock_driver.cpp` (RED)
- Implement mock driver (GREEN)
- Run: `uv run test.py test_quad_spi_mock_driver`

**Phase 3**: Multi-chipset tests
- Add LPD8806, WS2801, P9813 test cases
- Run: `uv run test.py --cpp` (all SPI tests)

**Phase 4**: Hardware validation (AFTER all tests pass)
- Flash to ESP32
- Logic analyzer verification
- Visual LED inspection

---

## Original Testing Requirements Summary

**Unit tests required before implementation**:

1. **Interleaving Test**: Generate APA102 protocol data for 4 strips with 1, 2, 4, 7 LEDs ✅ Covered in Phase 1, Test 1
2. **Expected Pattern Assertion**: Verify exact bit-interleaved output matches expected pattern ✅ Covered in Phase 1, Test 3
3. **Inverse Transpose Test**: De-interleave back to original per-lane format and verify match ✅ Covered in Phase 1, Test 2
4. **Reference**: See "Unit Test Requirements" section below for detailed specification ✅ Expanded into full TDD strategy above

## Core Concept

> "Hardware quad-SPI is 4-8× faster than optimized soft-SPI while using zero CPU cycles during transmission"
> — QUAD_SPI_ESP32.md

Unlike software SPI which requires CPU-spinning bit-banging, hardware quad-SPI uses the ESP32's SPI peripheral with DMA:
- **4 data lines** transmit in parallel per SPI bus
- **DMA transfers** data from memory to SPI peripheral (zero CPU load)
- **Hardware clock generation** ensures precise timing without jitter
- **8 total strips** supported (ESP32/S2/S3: HSPI + VSPI, each with 4 lanes)

## Platform Capabilities

### Full Quad-SPI Support (4 channels per bus)

| Platform | SPI Buses | Channels/Bus | Total Parallel Strips | Max Clock Speed |
|----------|-----------|--------------|----------------------|-----------------|
| **ESP32** | HSPI + VSPI | 4 (quad) | **8 strips** | 40-80 MHz |
| **ESP32-S2** | SPI2 + SPI3 | 4 (quad) | **8 strips** | 40-80 MHz |
| **ESP32-S3** | SPI2 + SPI3 | 4 (quad) | **8 strips** | 40-80 MHz |

**Pin Configuration per Bus:**
- 1× Clock (SCLK) - shared across all 4 strips
- 4× Data (MOSI/D0, MISO/D1, WP/D2, HD/D3)

### Dual-SPI Support (2 channels per bus)

| Platform | SPI Buses | Channels/Bus | Total Parallel Strips | Max Clock Speed |
|----------|-----------|--------------|----------------------|-----------------|
| **ESP32-C3** | SPI2 | 2 (dual) | **2 strips** | 40-80 MHz |
| **ESP32-C2** | SPI2 | 2 (dual) | **2 strips** | 40-60 MHz |
| **ESP32-C6** | SPI2 | 2-4 (TBD) | **2-4 strips** | 40-80 MHz |
| **ESP32-H2** | SPI2 | 2 (dual) | **2 strips** | 40-60 MHz |

**Pin Configuration per Bus (Dual-SPI):**
- 1× Clock (SCLK) - shared across 2 strips
- 2× Data (MOSI/D0, MISO/D1)

## Technical Requirements

### 1. Hardware Quad-SPI with DMA
- ESP32 SPI peripheral configured for quad-line mode (SPI_QUAD_MODE)
- DMA channels allocated for zero-copy asynchronous transmission
- Buffers allocated in DMA-capable memory (`MALLOC_CAP_DMA`)
- Bit-interleaved data format for parallel transmission

### 2. Bit Interleaving for Quad-SPI

To use quad-SPI for parallel LED control, data from 4 independent LED strips must be **bit-interleaved** into a single buffer:

```
Strip A: a7 a6 a5 a4 a3 a2 a1 a0
Strip B: b7 b6 b5 b4 b3 b2 b1 b0
Strip C: c7 c6 c5 c4 c3 c2 c1 c0
Strip D: d7 d6 d5 d4 d3 d2 d1 d0

Interleaved: d7 c7 b7 a7 | d6 c6 b6 a6 | ... | d0 c0 b0 a0
```

The SPI hardware then clocks out each interleaved byte, sending bit `aN` on D0, `bN` on D1, `cN` on D2, `dN` on D3 simultaneously.

### 3. Multi-Lane Architecture
- Support 4 parallel SPI data lanes per bus (quad-SPI)
- Support 2 parallel SPI data lanes per bus (dual-SPI on C-series)
- Each lane drives independent LED strips
- Shared clock signal (SCK) across all lanes in a bus
- **Lane Length Handling**: LED strips can be different sizes:
  - Each lane has its own CRGB array (normal FastLED usage)
  - Controllers generate protocol data into per-lane buffers
  - Buffers pre-allocated to max lane size with protocol-safe padding
  - Interleaving transposes from equal-size buffers → single DMA buffer
  - No runtime padding needed (handled during buffer allocation)

## Supported Chipsets

Hardware quad-SPI works with SPI-based LED chipsets:

- **APA102/SK9822** (Dotstar) - 4-wire SPI, up to 20 MHz (reference implementation uses 40 MHz)
- **LPD8806** - SPI-based LED driver (2 MHz max clock)
- **P9813** (Total Control Lighting) - SPI protocol
- **WS2801** - SPI-compatible LED chipset (up to 25 MHz)
- **HD107** - High-speed APA102 variant (40 MHz)

### Chipset Timing Characteristics and Protocol-Safe Padding

| Chipset | Max Clock Speed | Start Frame | End Frame | Per-LED Data | Safe Padding Byte |
|---------|----------------|-------------|-----------|--------------|-------------------|
| APA102/SK9822 | 1-40 MHz | 32 bits (0x00000000) | 32+ bits based on count | 32 bits: brightness + RGB | **0xFF** (end frame continuation) |
| LPD8806 | 2 MHz | None | Latch clocks (zeros) | 24 bits with MSB=1 | **0x00** (latch continuation) |
| WS2801 | 25 MHz | None | 500μs quiet time | 24 bits RGB | **0x00** (no protocol state) |
| P9813 | ~15 MHz | 32 bits (0x00000000) | 32 bits (0x00000000) | 32 bits: checksum + RGB | **0x00** (boundary byte) |
| HD107 | 40 MHz | 32 bits (0x00000000) | Based on count | 32 bits: brightness + RGB | **0xFF** (end frame continuation) |

**Critical**: Padding bytes must be protocol-safe. Using `0x00` for APA102 would create false start frames. Each chipset requires its own safe padding value (see "Padding Strategy" section below).

## Padding Strategy for Variable-Length Lanes

### The Problem

When driving LED strips of different lengths in parallel (e.g., 60 LEDs on Lane 0, 150 LEDs on Lane 1), shorter lanes must be "padded" to match the longest lane's transmission time. **Incorrect padding corrupts LED protocol state.**

**Example of WRONG padding (using 0x00 for APA102)**:
```
Lane 0 (60 LEDs):
[START: 0x00000000][LED data: 240 bytes][END: 0xFFFFFFFF][PADDING: 0x00000000...]
                                                           ↑
                                              Interpreted as FALSE START FRAME!
                                              Corrupts protocol state for next frame
```

### The Solution: Pre-Allocated Buffers with Protocol-Safe Padding

**Hybrid Strategy** combining pre-allocation efficiency with compile-time type safety:

1. **Controllers provide static padding byte** via `getPaddingByte()` method
2. **Bulk driver pre-allocates buffers** to max lane size during registration
3. **Padding region pre-filled** with protocol-safe value (never modified)
4. **Controllers write only actual data** during capture phase
5. **Interleaving reads equal-size buffers** with no conditionals

### Controller Interface (Minimal Change)

Each SPI controller adds **one static method** returning the protocol-safe padding byte:

```cpp
// APA102 - pad with end frame byte
template </* existing template params */>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
public:
    // NEW: Static method for padding byte (compile-time constant)
    static constexpr uint8_t getPaddingByte() {
        // Extract first byte from END_FRAME template parameter
        return (END_FRAME >> 24) & 0xFF;  // Default: 0xFF
    }

    // Existing methods unchanged
    static constexpr int32_t calculateBytes(int32_t num_leds) { /* ... */ }
    virtual void showPixels(PixelController<RGB_ORDER> & pixels) override { /* ... */ }
};

// LPD8806 - pad with latch byte
template </* template params */>
class LPD8806Controller : public CPixelLEDController<RGB_ORDER> {
public:
    static constexpr uint8_t getPaddingByte() {
        return 0x00;  // Latch byte continuation
    }
};

// WS2801 - any value safe (no protocol state)
template </* template params */>
class WS2801Controller : public CPixelLEDController<RGB_ORDER> {
public:
    static constexpr uint8_t getPaddingByte() {
        return 0x00;  // Don't care
    }
};

// P9813 - pad with boundary byte
template </* template params */>
class P9813Controller : public CPixelLEDController<RGB_ORDER> {
public:
    static constexpr uint8_t getPaddingByte() {
        return 0x00;  // Boundary byte
    }
};
```

### Bulk Driver Implementation

```cpp
template<uint8_t SPI_BUS_NUM, uint32_t SPI_CLOCK_HZ>
class QuadSPIController {
private:
    struct LaneInfo {
        uint8_t lane_id;
        fl::vector<uint8_t> capture_buffer;  // Pre-allocated to mMaxLaneBytes
        uint32_t actual_bytes;               // Actual data size (without padding)
    };
    fl::vector<LaneInfo> mLanes;
    uint32_t mMaxLaneBytes = 0;

public:
    // Called during setup for each controller
    template<typename CONTROLLER_TYPE>
    void registerLane(uint8_t lane_id, int num_leds) {
        // Phase 1: Calculate actual data size
        int32_t actual_bytes = CONTROLLER_TYPE::calculateBytes(num_leds);

        // Update max lane size
        mMaxLaneBytes = fl::max(mMaxLaneBytes, static_cast<uint32_t>(actual_bytes));

        LaneInfo lane;
        lane.lane_id = lane_id;
        lane.actual_bytes = actual_bytes;

        // Pre-allocate buffer to max size (will be resized again after all lanes registered)
        lane.capture_buffer.reserve(actual_bytes);

        mLanes.push_back(lane);
    }

    // Called after all lanes registered
    void finalize() {
        // Resize all buffers to max size and pre-fill padding
        for (auto& lane : mLanes) {
            // Resize to max size
            lane.capture_buffer.resize(mMaxLaneBytes);

            // Get padding byte from controller's static method
            // This requires storing controller type info...
            // See "Type-Erased Version" below for practical approach
        }
    }
};
```

### Type-Erased Practical Implementation

Since we can't easily store controller type info after template instantiation, use a **padding byte parameter**:

```cpp
template<typename CONTROLLER_TYPE>
void registerLane(uint8_t lane_id, int num_leds) {
    int32_t actual_bytes = CONTROLLER_TYPE::calculateBytes(num_leds);
    uint8_t padding_byte = CONTROLLER_TYPE::getPaddingByte();

    mMaxLaneBytes = fl::max(mMaxLaneBytes, static_cast<uint32_t>(actual_bytes));

    LaneInfo lane;
    lane.lane_id = lane_id;
    lane.actual_bytes = actual_bytes;
    lane.padding_byte = padding_byte;  // Store for later
    lane.capture_buffer.reserve(actual_bytes);

    mLanes.push_back(lane);
}

void finalize() {
    for (auto& lane : mLanes) {
        size_t old_size = lane.capture_buffer.size();
        lane.capture_buffer.resize(mMaxLaneBytes);

        // Pre-fill padding region with protocol-safe byte
        size_t padding_start = lane.actual_bytes;
        size_t padding_length = mMaxLaneBytes - lane.actual_bytes;
        memset(&lane.capture_buffer[padding_start], lane.padding_byte, padding_length);
    }
}
```

### Capture Phase with Write Limit

During capture, controllers must **not overwrite** the pre-filled padding:

```cpp
void beginShowLeds() {
    for (auto& lane : mLanes) {
        // Clear only the actual data region (preserve padding)
        memset(lane.capture_buffer.data(), 0, lane.actual_bytes);

        // Start capture with size limit
        auto* proxy = static_cast<SPIOutputProxy*>(lane.spiImpl.get());
        proxy->beginCaptureToBuffer(&lane.capture_buffer, lane.actual_bytes);
    }
}

// Modified SPIOutputProxy
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIOutputProxy : public ISPIOutput {
private:
    fl::vector<uint8_t>* mCaptureBuffer = nullptr;
    uint32_t mCaptureLimit = UINT32_MAX;  // Write limit
    uint32_t mCapturePosition = 0;

public:
    void beginCaptureToBuffer(fl::vector<uint8_t>* buffer, uint32_t size_limit) {
        mMode = CAPTURE_TO_BUFFER;
        mCaptureBuffer = buffer;
        mCaptureLimit = size_limit;
        mCapturePosition = 0;
    }

    void writeByte(uint8_t b) override {
        if (mMode == DIRECT_HARDWARE) {
            mHardwareSPI.writeByte(b);
        } else {
            // Write only if within limit
            if (mCapturePosition < mCaptureLimit) {
                (*mCaptureBuffer)[mCapturePosition++] = b;
            }
            // Beyond limit: silently ignore (padding region)
        }
    }
};
```

### Interleaving (Simplified - No Conditionals)

All buffers are now the same size, so interleaving is straightforward:

```cpp
void interleaveLanes() {
    uint32_t* output = reinterpret_cast<uint32_t*>(mInterleavedDMABuffer.data());

    // All lanes have same size (mMaxLaneBytes), no bounds checking needed
    for (uint32_t byte_idx = 0; byte_idx < mMaxLaneBytes; byte_idx += 4) {
        uint32_t words[4];

        // Read 4 bytes from each lane (always valid, no conditionals)
        for (uint8_t lane = 0; lane < 4; lane++) {
            words[lane] = *reinterpret_cast<uint32_t*>(
                &mLanes[lane].capture_buffer[byte_idx]
            );
        }

        // Bit-interleave (no padding logic needed here!)
        interleave_quad_words(&output[byte_idx / 4], words[0], words[1], words[2], words[3]);
    }
}
```

### Benefits of This Approach

✅ **Protocol-safe**: Each chipset defines its own safe padding byte
✅ **Minimal controller changes**: Only add `getPaddingByte()` static method
✅ **Type-safe**: Resolved at compile time via templates
✅ **Performance**: No conditionals in interleaving loop (cache-friendly)
✅ **Set-and-forget**: Padding written once during `finalize()`
✅ **Memory cost acceptable**: Modern platforms have KB-MB of RAM
✅ **Extensible**: New chipsets just add `getPaddingByte()` method

### Memory Overhead Example

**Scenario**: 4 lanes with 60, 150, 80, 100 LEDs (APA102)

```
Lane 0: 252 bytes actual, 608 bytes allocated (356 bytes padding = 58% overhead)
Lane 1: 608 bytes actual, 608 bytes allocated (0 bytes padding)
Lane 2: 328 bytes actual, 608 bytes allocated (280 bytes padding = 46% overhead)
Lane 3: 408 bytes actual, 608 bytes allocated (200 bytes padding = 33% overhead)

Total: 1596 bytes actual data, 2432 bytes allocated = 836 bytes overhead (34%)
```

**Acceptable**: On ESP32 with 520KB SRAM, 836 bytes is 0.16% of total RAM.

### Why NOT Use 0x00 for Padding (Detailed Example)

**APA102 protocol corruption with 0x00 padding**:

```
Correct transmission (Lane 0, 60 LEDs):
[START: 0x00 0x00 0x00 0x00]
[LED_0: 0xE0 0xBB 0xGG 0xRR]
...
[LED_59: 0xE0 0xBB 0xGG 0xRR]
[END: 0xFF 0xFF 0xFF 0xFF]
                              ← Transmission ends here

WRONG transmission with 0x00 padding (to match 150 LED lane):
[START: 0x00 0x00 0x00 0x00]
[LED_0: 0xE0 0xBB 0xGG 0xRR]
...
[LED_59: 0xE0 0xBB 0xGG 0xRR]
[END: 0xFF 0xFF 0xFF 0xFF]
[PADDING: 0x00 0x00 0x00 0x00]  ← APA102 interprets as NEW START FRAME!
[PADDING: 0x00 0x00 0x00 0x00]  ← Interpreted as LED data (black, brightness 0)
...                               ← Corrupts protocol state machine

CORRECT transmission with 0xFF padding:
[START: 0x00 0x00 0x00 0x00]
[LED_0: 0xE0 0xBB 0xGG 0xRR]
...
[LED_59: 0xE0 0xBB 0xGG 0xRR]
[END: 0xFF 0xFF 0xFF 0xFF]
[PADDING: 0xFF 0xFF 0xFF 0xFF]  ← Interpreted as END FRAME continuation
[PADDING: 0xFF 0xFF 0xFF 0xFF]  ← Harmless, chip already latched data
...                               ← Protocol state remains valid
```

## Proposed Implementation Details (Hardware Quad-SPI)

### Architecture Components

#### 1. Hardware Quad-SPI DMA Controller

**Memory Architecture**: Simple array of per-lane buffers transposed into single interleaved DMA buffer.

**Key Insight**: No need for RectangularByteBuffer - each controller already has its own LED array. We just need per-lane capture buffers and one interleaved DMA buffer.

```cpp
template<uint8_t SPI_BUS_NUM, uint32_t SPI_CLOCK_HZ>
class QuadSPIController {
private:
    // Lane configuration - points to per-lane capture buffers
    struct LaneInfo {
        uint8_t lane_id;          // 0-3 for quad-SPI
        uint8_t padding_byte;     // Protocol-safe padding byte (0xFF for APA102, etc.)
        uint32_t actual_bytes;    // Actual data size (without padding)
        fl::vector<uint8_t> capture_buffer;  // Pre-allocated to mMaxLaneBytes
    };
    fl::vector<LaneInfo> mLanes;

    // Single interleaved DMA buffer (fl::vector supports PSRamAllocator natively)
    fl::vector<uint8_t> mInterleavedDMABuffer;

    // DMA transaction handle
    spi_transaction_t mSpiTransaction;
    spi_device_handle_t mSpiDevice;

    uint32_t mMaxLaneBytes;
    uint8_t mNumLanes;  // 2 or 4 depending on platform

public:
    void begin(uint8_t sclk_pin, uint8_t d0_pin, uint8_t d1_pin,
               uint8_t d2_pin, uint8_t d3_pin) {
        // Configure ESP32 SPI peripheral for quad mode
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = d0_pin,
            .miso_io_num = d1_pin,
            .sclk_io_num = sclk_pin,
            .quadwp_io_num = d2_pin,  // WP/D2
            .quadhd_io_num = d3_pin,  // HD/D3
            .max_transfer_sz = 4096,  // DMA buffer size
            .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD
        };

        spi_device_interface_config_t dev_cfg = {
            .clock_speed_hz = SPI_CLOCK_HZ,
            .mode = 0,  // SPI mode 0 (CPOL=0, CPHA=0)
            .spics_io_num = -1,  // No CS
            .queue_size = 1,
            .flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX
        };

        spi_bus_initialize(SPI_BUS_NUM, &bus_cfg, SPI_DMA_CH_AUTO);
        spi_bus_add_device(SPI_BUS_NUM, &dev_cfg, &mSpiDevice);
    }

    // Template method called during setup for each controller
    template<typename CONTROLLER_TYPE>
    void addLane(uint8_t lane_id, int num_leds) {
        // Phase 1: Calculate actual data size and get padding byte
        int32_t actual_bytes = CONTROLLER_TYPE::calculateBytes(num_leds);
        uint8_t padding_byte = CONTROLLER_TYPE::getPaddingByte();

        LaneInfo lane;
        lane.lane_id = lane_id;
        lane.actual_bytes = actual_bytes;
        lane.padding_byte = padding_byte;

        // Pre-allocate buffer (will be resized to max in finalize())
        lane.capture_buffer.reserve(actual_bytes);

        mLanes.push_back(lane);
        mMaxLaneBytes = fl::max(mMaxLaneBytes, static_cast<uint32_t>(actual_bytes));
    }

    // Called after all lanes registered - pre-fill padding regions
    void finalize() {
        for (auto& lane : mLanes) {
            // Resize to max size
            lane.capture_buffer.resize(mMaxLaneBytes);

            // Pre-fill padding region with protocol-safe byte
            size_t padding_start = lane.actual_bytes;
            size_t padding_length = mMaxLaneBytes - lane.actual_bytes;
            memset(&lane.capture_buffer[padding_start], lane.padding_byte, padding_length);
        }

        // Allocate interleaved DMA buffer
        mInterleavedDMABuffer.resize(mMaxLaneBytes);
    }

    // DMA-driven transmission - returns immediately, completes asynchronously
    void transmitAsync() {
        // Bit-interleave all lanes into DMA buffer
        interleaveLanes();

        // Configure DMA transaction
        mSpiTransaction = {
            .flags = SPI_TRANS_MODE_QUAD,  // Quad data mode (not QIO)
            .length = mInterleavedDMABuffer.size() * 8,  // Total bits
            .tx_buffer = mInterleavedDMABuffer.data()
        };

        // Start DMA transfer (non-blocking)
        spi_device_queue_trans(mSpiDevice, &mSpiTransaction, portMAX_DELAY);
    }

    // Wait for DMA completion
    void waitComplete() {
        spi_transaction_t* trans_result;
        spi_device_get_trans_result(mSpiDevice, &trans_result, portMAX_DELAY);
    }

private:
    void interleaveLanes() {
        // Transpose from per-lane buffers → interleaved DMA buffer
        // All buffers are mMaxLaneBytes size (pre-padded), no conditionals needed

        uint32_t* output = reinterpret_cast<uint32_t*>(mInterleavedDMABuffer.data());

        for (uint32_t byte_idx = 0; byte_idx < mMaxLaneBytes; byte_idx += 4) {
            // Gather 4 bytes from each lane (all buffers same size, always valid)
            uint32_t words[4];

            for (uint8_t lane = 0; lane < mNumLanes && lane < 4; lane++) {
                words[lane] = *reinterpret_cast<uint32_t*>(
                    &mLanes[lane].capture_buffer[byte_idx]
                );
            }

            // Bit-interleave 4 words into output (using optimized function)
            // Padding regions contain protocol-safe bytes (0xFF for APA102, etc.)
            interleave_quad_words(&output[byte_idx / 4],
                                  words[0], words[1], words[2], words[3]);
        }
    }
};
```

#### 2. Three-Phase Process: Pre-scan, Capture, Transpose

The implementation uses a **three-phase approach** to handle variable-size lanes efficiently.

**Phase 1: Pre-Scan (Calculate Sizes and Pre-Allocate with Padding)**

Before any data capture, calculate the exact byte count for each lane and pre-allocate buffers with protocol-safe padding:

```cpp
// For each lane, use controller's static calculateBytes() and getPaddingByte()
Lane 0 (APA102, 100 LEDs):
    actual_bytes = 4 + (100*4) + (100/32)*4 = 408 bytes
    padding_byte = APA102Controller::getPaddingByte() = 0xFF

Lane 1 (APA102, 150 LEDs):
    actual_bytes = 4 + (150*4) + (150/32)*4 = 608 bytes  ← longest
    padding_byte = 0xFF

Lane 2 (APA102,  60 LEDs):
    actual_bytes = 4 + ( 60*4) + ( 60/32)*4 = 248 bytes
    padding_byte = 0xFF

Lane 3 (APA102, 125 LEDs):
    actual_bytes = 4 + (125*4) + (125/32)*4 = 508 bytes
    padding_byte = 0xFF

max_lane_bytes = 608  // All buffers allocated to this size

// Pre-allocate and pre-pad
for each lane:
    buffer.resize(max_lane_bytes)
    memset(&buffer[actual_bytes], padding_byte, max_lane_bytes - actual_bytes)
```

**Phase 2: Capture (Controllers Write Protocol Data)**

Each controller writes to its lane's pre-allocated buffer (padding region remains untouched):

```
Lane 0: [START(4)][LED data(400)][END(4)][PADDING: 0xFF × 200]  = 608 bytes total
        ↑─────── 408 bytes written ────↑ ↑─── pre-filled ────↑

Lane 1: [START(4)][LED data(600)][END(4)]                       = 608 bytes total
        ↑─────────────── 608 bytes written ──────────────────↑  (no padding)

Lane 2: [START(4)][LED data(240)][END(4)][PADDING: 0xFF × 360]  = 608 bytes total
        ↑─────── 248 bytes written ────↑ ↑─── pre-filled ────↑

Lane 3: [START(4)][LED data(500)][END(4)][PADDING: 0xFF × 100]  = 608 bytes total
        ↑─────── 508 bytes written ────↑ ↑─── pre-filled ────↑

Controllers use proxy->writeByte() with write limit (actual_bytes)
Writes beyond limit are ignored (padding region preserved)
```

**Phase 3: Transpose (Bit-Interleave Equal-Size Buffers)**

Transpose from per-lane buffers to single interleaved DMA buffer:

```
Source (per-lane buffers):           Destination (DMA buffer):
Lane0[0..3] = [A0 A1 A2 A3]
Lane1[0..3] = [B0 B1 B2 B3]          [I0 I1 I2 I3...]
Lane2[0..3] = [C0 C1 C2 C3]    →     bit-interleave
Lane3[0..3] = [D0 D1 D2 D3]          (608 bytes total)

All lanes are now 608 bytes (padding pre-filled):
  Lane 0: bytes 408-607 → 0xFF (pre-filled END_FRAME continuation)
  Lane 2: bytes 248-607 → 0xFF (pre-filled END_FRAME continuation)
  Lane 3: bytes 508-607 → 0xFF (pre-filled END_FRAME continuation)

Interleaving function: interleave_quad_words(output, A, B, C, D)
Processes 4 words at a time, outputs 4 interleaved words
NO CONDITIONALS needed - all buffers same size
```

**Why Bit-Interleaving:**
- Hardware quad-SPI clocks out 4 bits simultaneously (one per data line)
- D0 transmits bits from Lane 0, D1 from Lane 1, etc.
- Interleaved format allows single SPI transaction to drive all 4 lanes

**Buffer Sizes:**
- Per-lane buffers: Variable sizes (408, 608, 248, 508 bytes in example)
- Interleaved DMA buffer: `max_lane_bytes = 608` (sized to longest lane)
- Zero-padding: Shorter lanes contribute zeros during interleaving
- DMA allocation: `fl::vector<uint8_t>` uses PSRamAllocator automatically on ESP32S3

#### 3. Avoiding Combinatorial Controller Explosion - Same Proxy Pattern

**Reuse Proxy Pattern from Software SPI** (from FEATURE.md):

The same factory + proxy approach works for hardware quad-SPI:

```cpp
// Base interface for SPI operations (unchanged from software version)
class ISPIOutput {
public:
    virtual ~ISPIOutput() = default;
    virtual void writeByte(uint8_t b) = 0;
    virtual void writeWord(uint16_t w) = 0;
    virtual void select() = 0;
    virtual void release() = 0;
    virtual void waitFully() = 0;
    virtual void init() = 0;
};

// Proxy SPI device - can write to hardware OR capture to buffer
// SAME as software SPI proxy, but HardwareSPI uses ESP32 quad-SPI peripheral
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class QuadSPIOutputProxy : public ISPIOutput {
private:
    // Hardware implementation uses ESP32 quad-SPI instead of bit-banging
    using HardwareSPI = QuadSPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>;
    HardwareSPI mHardwareSPI;

    // Mode switching (same as software SPI)
    enum Mode { DIRECT_HARDWARE, CAPTURE_TO_BUFFER };
    Mode mMode = DIRECT_HARDWARE;

    // Capture buffer for interleaving
    fl::vector<uint8_t>* mCaptureBuffer = nullptr;

public:
    // Same interface as software SPI proxy
    void writeByte(uint8_t b) override {
        if (mMode == DIRECT_HARDWARE) {
            mHardwareSPI.writeByte(b);
        } else {
            mCaptureBuffer->push_back(b);
        }
    }

    // Mode control - called by bulk driver
    void beginCapture(fl::vector<uint8_t>* buffer) {
        mMode = CAPTURE_TO_BUFFER;
        mCaptureBuffer = buffer;
    }

    void endCapture() {
        mMode = DIRECT_HARDWARE;
        mCaptureBuffer = nullptr;
    }
};

// Factory creates shared SPI instances (same as software SPI)
// Reference: fl::shared_ptr in src/fl/shared_ptr.h
class QuadSPIFactory {
private:
    struct SPIKey {
        uint8_t data_pin;
        uint8_t clock_pin;
        uint32_t spi_speed;

        bool operator==(const SPIKey& other) const {
            return data_pin == other.data_pin &&
                   clock_pin == other.clock_pin &&
                   spi_speed == other.spi_speed;
        }
    };

    fl::map<SPIKey, fl::shared_ptr<ISPIOutput>> mSPIInstances;

public:
    template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
    fl::shared_ptr<ISPIOutput> getSPI() {
        SPIKey key{DATA_PIN, CLOCK_PIN, SPI_SPEED};

        auto it = mSPIInstances.find(key);
        if (it != mSPIInstances.end()) {
            return it->second;
        }

        // Create new quad-SPI proxy instance
        auto spi = fl::make_shared<QuadSPIOutputProxy<DATA_PIN, CLOCK_PIN, SPI_SPEED>>();
        mSPIInstances[key] = spi;
        return spi;
    }

    // Check if multiple controllers share same clock (enables quad-SPI bulk mode)
    bool canEnableQuadSPIMode(uint8_t clock_pin, uint32_t spi_speed) {
        int count = 0;
        for (const auto& pair : mSPIInstances) {
            if (pair.first.clock_pin == clock_pin &&
                pair.first.spi_speed == spi_speed) {
                count++;
            }
        }
        return count >= 2 && count <= 4;  // Quad-SPI supports 2-4 lanes
    }
};
```

**Modified Controller** (minimal change - IDENTICAL to software SPI version):
```cpp
template </* ... */>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
    // Same change as software SPI version
    fl::shared_ptr<ISPIOutput> mSPI;

public:
    APA102Controller() {
        // Get shared SPI from quad-SPI factory
        mSPI = QuadSPIFactory::getInstance().getSPI<DATA_PIN, CLOCK_PIN, SPI_SPEED>();
    }

    // All existing methods unchanged (use mSPI-> instead of mSPI.)
};
```

**Key Insight**: Same proxy pattern works for hardware quad-SPI because:
- ✅ Controllers still capture data to buffers in proxy mode
- ✅ Bulk driver orchestrates capture → interleaving → DMA transmission
- ✅ No controller code changes needed (same as software SPI)
- ✅ Factory detects quad-SPI capable platform and creates appropriate proxy

#### 4. Platform-Specific Implementation Strategy

**Automatic Quad-SPI Detection**:
- **ESP32/S2/S3**: Automatically enable quad-SPI when 2-4 controllers share same CLOCK_PIN
- **ESP32-C3/C2/C6/H2**: Automatically enable dual-SPI when 2 controllers share same CLOCK_PIN
- **Other platforms**: Fall back to software SPI (from FEATURE.md)

**Leverage Platform Detection**: Uses `FASTLED_ESP32` and related macros

```cpp
// Auto-enable quad-SPI on supported platforms
#if defined(FASTLED_ESP32) || defined(FASTLED_ESP32S2) || defined(FASTLED_ESP32S3)
    #define FASTLED_QUAD_SPI_AVAILABLE 1
    #define FASTLED_MAX_SPI_LANES 4
#elif defined(FASTLED_ESP32C3) || defined(FASTLED_ESP32C2) || defined(FASTLED_ESP32C6) || defined(FASTLED_ESP32H2)
    #define FASTLED_QUAD_SPI_AVAILABLE 1
    #define FASTLED_MAX_SPI_LANES 2  // Dual-SPI only
#else
    #define FASTLED_QUAD_SPI_AVAILABLE 0
    #define FASTLED_MAX_SPI_LANES 1  // No hardware multi-lane support
#endif

// Wrapper class provides VALUE semantics while using quad-SPI internally
#if FASTLED_QUAD_SPI_AVAILABLE

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIOutputWrapper {
private:
    fl::shared_ptr<ISPIOutput> mImpl;

public:
    SPIOutputWrapper() {
        mImpl = QuadSPIFactory::getInstance().getSPI<DATA_PIN, CLOCK_PIN, SPI_SPEED>();
    }

    // Value semantics - same interface as regular SPIOutput
    void writeByte(uint8_t b) { mImpl->writeByte(b); }
    void writeWord(uint16_t w) { mImpl->writeWord(w); }
    void select() { mImpl->select(); }
    void release() { mImpl->release(); }
    void waitFully() { mImpl->waitFully(); }
    void init() { mImpl->init(); }

    // Expose shared_ptr for bulk driver access
    fl::shared_ptr<ISPIOutput> getImpl() { return mImpl; }
};

// Override SPIOutput to use quad-SPI wrapper on ESP32
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
using SPIOutput = SPIOutputWrapper<DATA_PIN, CLOCK_PIN, SPI_SPEED>;

#else
    // Other platforms: SPIOutput is software SPI (from FEATURE.md)
#endif

// Universal controller implementation - NO CHANGES NEEDED
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(6)>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
    typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
    SPI mSPI;  // SAME AS BEFORE - value semantics on ALL platforms

public:
    void showPixels(PixelController<RGB_ORDER> & pixels) override {
        // SAME SYNTAX ON ALL PLATFORMS
        mSPI.writeByte(0x00);
        mSPI.writeWord(0xFF);
    }
};
```

**Platform Capabilities**:

| Platform | Hardware Support | SPI Type | Lanes | DMA | Activation |
|----------|-----------------|----------|-------|-----|------------|
| ESP32 | ✅ Quad-SPI | `QuadSPIOutputWrapper` | 4 | ✅ Yes | Shared CLOCK_PIN |
| ESP32-S2 | ✅ Quad-SPI | `QuadSPIOutputWrapper` | 4 | ✅ Yes | Shared CLOCK_PIN |
| ESP32-S3 | ✅ Quad-SPI | `QuadSPIOutputWrapper` | 4 | ✅ Yes | Shared CLOCK_PIN |
| ESP32-C3 | ✅ Dual-SPI | `DualSPIOutputWrapper` | 2 | ✅ Yes | Shared CLOCK_PIN |
| ESP32-C6 | ✅ Dual-SPI | `DualSPIOutputWrapper` | 2-4 | ✅ Yes | Shared CLOCK_PIN |
| Teensy 4.x | ❌ No | Software fallback | 1 | ❌ No | - |
| RP2040 | ❌ No | Software fallback | 1 | ❌ No | - |
| AVR | ❌ No | Software fallback | 1 | ❌ No | - |

#### 5. Supporting Multiple Quad-SPI Buses (Different Clock Pins)

**ESP32 has 2 SPI buses**: HSPI (SPI2) + VSPI (SPI3)

**Scenario**: 8 total strips - 4 on HSPI, 4 on VSPI

```cpp
// HSPI bus (SPI2) - 4 lanes
FastLED.addLeds<APA102, DATA_PIN_1, CLOCK_PIN_HSPI, RGB>(leds1, 100);
FastLED.addLeds<APA102, DATA_PIN_2, CLOCK_PIN_HSPI, RGB>(leds2, 150);
FastLED.addLeds<APA102, DATA_PIN_3, CLOCK_PIN_HSPI, RGB>(leds3, 80);
FastLED.addLeds<APA102, DATA_PIN_4, CLOCK_PIN_HSPI, RGB>(leds4, 200);

// VSPI bus (SPI3) - 4 lanes
FastLED.addLeds<APA102, DATA_PIN_5, CLOCK_PIN_VSPI, RGB>(leds5, 120);
FastLED.addLeds<APA102, DATA_PIN_6, CLOCK_PIN_VSPI, RGB>(leds6, 90);
FastLED.addLeds<APA102, DATA_PIN_7, CLOCK_PIN_VSPI, RGB>(leds7, 110);
FastLED.addLeds<APA102, DATA_PIN_8, CLOCK_PIN_VSPI, RGB>(leds8, 140);
```

**Implementation - Parallel DMA Transmission**:

```cpp
class QuadSPIBulkDriverRegistry {
private:
    fl::vector<QuadSPIBulkDriver*> mBulkDrivers;

public:
    // Factory creates one driver per unique CLOCK_PIN (SPI bus)
    QuadSPIBulkDriver* getOrCreateDriver(uint8_t clock_pin, uint8_t spi_bus_num) {
        for (auto* driver : mBulkDrivers) {
            if (driver->getClockPin() == clock_pin) {
                return driver;
            }
        }

        auto* driver = new QuadSPIBulkDriver(clock_pin, spi_bus_num);
        mBulkDrivers.push_back(driver);

        FL_WARN("Quad-SPI: Created bulk driver for CLOCK_PIN %d (SPI%d)",
                clock_pin, spi_bus_num);

        return driver;
    }

    // Called by FastLED.show() - DMA allows parallel transmission
    void showAll() {
        FL_WARN("Quad-SPI: Starting DMA transfer for %d buses", mBulkDrivers.size());

        // Start all DMA transfers (non-blocking)
        for (auto* driver : mBulkDrivers) {
            driver->beginShowLeds();
            driver->transmitAsync();  // Start DMA transfer
        }

        // Wait for all DMA transfers to complete
        for (auto* driver : mBulkDrivers) {
            driver->waitComplete();
            driver->endShowLeds();
        }

        FL_WARN("Quad-SPI: Completed %d buses", mBulkDrivers.size());
    }
};

class QuadSPIBulkDriver {
private:
    uint8_t mClockPin;
    uint8_t mSpiBusNum;
    fl::vector<LaneInfo> mLanes;
    QuadSPIController<> mQuadSPI;

public:
    void beginShowLeds() {
        // Set capture mode for all lanes in THIS bus only
        for (auto& lane : mLanes) {
            auto* proxy = static_cast<QuadSPIOutputProxy*>(lane.spiImpl.get());

            // Give proxy the lane's capture buffer for writing
            proxy->beginCaptureToBuffer(&lane.capture_buffer);
        }
    }

    void transmitAsync() {
        // Interleaving happens inside QuadSPIController
        // (reads from mLanes[].capture_buffer, writes to internal DMA buffer)

        // Start DMA transfer (non-blocking)
        mQuadSPI.transmitAsync();
    }

    void waitComplete() {
        // Wait for DMA completion
        mQuadSPI.waitComplete();
    }

    void endShowLeds() {
        // End capture mode
        for (auto& lane : mLanes) {
            auto* proxy = static_cast<QuadSPIOutputProxy*>(lane.spiImpl.get());
            proxy->endCapture();
        }

        FL_WARN("Quad-SPI: Transmitted %d lanes on SPI%d (CLOCK_PIN %d)",
                mLanes.size(), mSpiBusNum, mClockPin);
    }
};
```

**Rendering Flow with Multiple Quad-SPI Buses**:

```
FastLED.show() called
  ↓
QuadSPIBulkDriverRegistry::showAll()
  ↓
  For each bulk driver (SPI bus):
    ├─ beginShowLeds() → Enable capture for this bus's lanes
    ├─ Controllers write data (captured to buffers)
    ├─ transmitAsync() → Start DMA transfer (non-blocking)
  ↓
  Wait for all DMA transfers to complete (parallel transmission)
  ↓
All buses rendered in parallel via DMA
```

**Performance - Parallel DMA**:

| Scenario | DMA Strategy | Total Time |
|----------|-------------|------------|
| Single bus (4 lanes @ 40MHz) | Single DMA transfer | ~0.08ms (100 LEDs) |
| Two buses (8 lanes total) | **Parallel DMA transfers** | ~0.08ms (same time!) |
| Three buses (ESP32-P4 future) | Parallel DMA | ~0.08ms |

**Key Benefit**: DMA allows true parallel transmission of independent SPI buses!

#### 6. Static Byte Calculation - Pre-Scan Phase

**Critical for Phase 1**: Before capture begins, we must know the exact size of each lane's protocol data.

Each controller provides a static `calculateBytes()` method that returns:
```
total_bytes = preamble_bytes + (num_leds × bytes_per_led) + postamble_bytes(num_leds)
```

This enables:
1. **Pre-allocating** per-lane capture buffers to exact size (no reallocation during capture)
2. **Finding max_lane_bytes** across all lanes (determines DMA buffer size)
3. **Calculating padding** needed for shorter lanes (max_lane_bytes - lane_bytes)

**Example Implementation:**

```cpp
// Each controller provides static method (unchanged from FEATURE.md)
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
public:
    static constexpr int32_t calculateBytes(int32_t num_leds) {
        // START_FRAME: 4 bytes (0x00000000)
        // LED data: 4 bytes per LED (brightness + RGB)
        // END_FRAME: ((num_leds + 31) / 32) * 4 bytes (ceiling division)
        return 4 + (num_leds * 4) + (((num_leds + 31) / 32) * 4);
    }
};

class QuadSPIBulkDriver {
public:
    template<typename CONTROLLER_TYPE>
    void registerController(CONTROLLER_TYPE* controller, int num_leds, uint8_t lane_id) {
        // PHASE 1: Pre-scan - calculate exact size needed
        int32_t bytes_needed = CONTROLLER_TYPE::calculateBytes(num_leds);

        LaneInfo lane;
        lane.lane_id = lane_id;  // 0-3 for quad-SPI
        lane.num_bytes = bytes_needed;
        lane.capture_buffer.reserve(bytes_needed);  // Pre-allocate
        lane.spiImpl = controller->mSPI.getImpl();

        mLanes.push_back(lane);
        mMaxLaneBytes = fl::max(mMaxLaneBytes, bytes_needed);
    }

    void* beginShowLeds(int size) {
        // Resize DMA buffer based on max lane size (from Phase 1)
        if (mInterleavedDMABuffer.size() < mMaxLaneBytes) {
            mInterleavedDMABuffer.resize(mMaxLaneBytes);
        }

        // PHASE 2: Set capture mode - controllers will write to per-lane buffers
        for (auto& lane : mLanes) {
            auto* proxy = static_cast<QuadSPIOutputProxy*>(lane.spiImpl.get());
            proxy->beginCaptureToBuffer(&lane.capture_buffer);
        }

        return nullptr;
    }

    void endShowLeds() {
        // End capture mode
        for (auto& lane : mLanes) {
            auto* proxy = static_cast<QuadSPIOutputProxy*>(lane.spiImpl.get());
            proxy->endCapture();
        }

        // PHASE 3: Transpose from per-lane buffers → DMA buffer (with zero-padding)
        interleaveLanes();

        // Transmit via DMA
        transmitAsync();
        waitComplete();
    }
};
```

#### 7. Design Benefits Summary

**Avoids Combinatorial Explosion** (same as software SPI):
- ✅ No need for `APA102QuadController`, `WS2801QuadController`, etc.
- ✅ Existing controllers work unchanged with quad-SPI
- ✅ Same proxy pattern as software SPI
- ✅ All chipset-specific logic stays in one place

**Clean Separation of Concerns**:
- **SPIOutput**: Handles mode switching (direct vs capture)
- **Existing Controllers**: Handle chipset-specific protocol (unchanged)
- **QuadSPIBulkDriver**: Handles bit-interleaving and DMA orchestration
- **ESP32 SPI Peripheral**: Handles hardware quad-SPI transmission

**Flexibility**:
- Mixed chipsets possible (if same clock speed): `APA102 + WS2801` on different lanes
- Automatic fallback to software SPI on non-ESP32 platforms
- Easy to disable for debugging (don't share clock pins)

**Performance**:
- **Zero CPU overhead**: DMA handles all data transfer
- **4-8× faster than software SPI**: 40-80 MHz vs 6-12 MHz
- **Parallel buses**: Multiple SPI buses transmit simultaneously
- **No jitter**: Hardware clock generation ensures precise timing

### Performance Characteristics

**Transmission Time Example** (100 LEDs, 40 MHz quad-SPI clock):
- APA102: 4 bytes per LED + start/end frames = ~404 bytes
- At 40 MHz: 404 bytes × 8 bits ÷ 40,000,000 = **0.08 milliseconds**
- **4 lanes in parallel**: Same 0.08ms for all 4 strips (vs 0.32ms sequential)
- **Zero CPU load**: DMA handles transmission, CPU free for other work

**Comparison with Software SPI**:

| Approach | Clock Speed | CPU Usage | 4×100 LED Strips | Strips/Bus |
|----------|-------------|-----------|------------------|------------|
| Software SPI (FEATURE.md) | 6-12 MHz | 100% (blocking) | ~2.16ms | 4 |
| **Hardware Quad-SPI** | 40-80 MHz | 0% (DMA) | **~0.08ms** | **4** |

**Speedup**: Hardware quad-SPI is **~27× faster** with **zero CPU load**!

## User Experience

```cpp
// ESP32: Automatic quad-SPI (no user action needed)
// Up to 4 strips per SPI bus, 8 strips total on ESP32 (HSPI + VSPI)

CRGB leds1[100], leds2[150], leds3[80], leds4[200];

void setup() {
    // All share same CLOCK_PIN → automatic quad-SPI bulk mode
    FastLED.addLeds<APA102, DATA_PIN_1, CLOCK_PIN, RGB>(leds1, 100);
    FastLED.addLeds<APA102, DATA_PIN_2, CLOCK_PIN, RGB>(leds2, 150);
    FastLED.addLeds<APA102, DATA_PIN_3, CLOCK_PIN, RGB>(leds3, 80);
    FastLED.addLeds<APA102, DATA_PIN_4, CLOCK_PIN, RGB>(leds4, 200);

    // Output: "Quad-SPI: Enabled for CLOCK_PIN 18 with 4 lanes at 40 MHz"
}

void loop() {
    // Normal FastLED usage
    fill_rainbow(leds1, 100, 0, 7);
    fill_rainbow(leds2, 150, 0, 7);
    fill_rainbow(leds3, 80, 0, 7);
    fill_rainbow(leds4, 200, 0, 7);

    FastLED.show();  // DMA handles transmission (zero CPU load!)
}

// Non-ESP32 platforms: Automatic fallback to software SPI (FEATURE.md)
// Same code works everywhere - platform detection is automatic
```

## FUTURE GOALS

### Phase 1: Software SPI Fallback for Non-ESP32 Platforms

**Status**: Documented in FEATURE.md

For platforms without quad-SPI hardware:
- Fall back to software multi-lane SPI (FEATURE.md)
- Same proxy pattern, same controller code
- Only difference: CPU-spinning vs DMA transmission

### Phase 2: Octal-SPI Support (ESP32-P4)

**Status**: Future enhancement

ESP32-P4 may support octal-SPI (8 data lines):
- **8 lanes per bus** instead of 4
- Same bit-interleaving approach
- Same proxy pattern
- Just more data lines

### Phase 3: Double-Buffering for Glitch-Free Updates

**Status**: Future enhancement

Use dual DMA buffers for seamless updates:
```cpp
// While buffer A is transmitting (DMA), prepare buffer B
// Swap buffers for next frame
// Zero glitches, zero latency
```

### Phase 4: Hardware SPI on Other Platforms

**Investigation Targets**:
- Teensy 4.x: Hardware SPI with DMA
- RP2040: PIO-based parallel SPI
- STM32: Hardware SPI with DMA

**Challenge**: Platform-specific APIs, less standardized than ESP32

---

## Summary

Hardware quad-SPI provides **4-8× speed improvement** and **zero CPU overhead** compared to software SPI, while maintaining the **same proxy pattern** and **same controller code**. ESP32 platforms automatically enable quad-SPI when multiple controllers share a clock pin, with seamless fallback to software SPI on other platforms.

**Key Innovation**: Reuse of proxy pattern from FEATURE.md enables hardware quad-SPI without any controller modifications!




## ADDENDUM - DEMO CODE

```c++
#include <FastLED.h>
#include <cassert>
#include <cstdint>
#include <string.h>

//#include "draw.hpp"
#include "driver_spi.hpp"

const Colour COLOUR_BLACK(0, 0, 0);

const int END_COUNT = 0;

static Colour *to_pixel(Driver::Pixel_ptr ptr)
{
    return reinterpret_cast<Colour *>(ptr);
}

Driver_spi::Strand::Strand(Driver_spi::Strand_config const &config)
    : m_config(config), m_pixels(config.m_num_pixels, COLOUR_BLACK)
{
}

int Driver_spi::Strand::get_num_pixels()
{
    return m_pixels.size();
}

Driver::Pixel_ptr Driver_spi::Strand::get_pixel_ptr(int offset)
{
    assert(offset < get_num_pixels());
    return &m_pixels[offset];
}

void Driver_spi::Strand::draw_pixels(Colour c)
{
    for (int i = 0; i < m_pixels.size(); ++i) {
    m_pixels[i] = c;
    }
}

void Driver_spi::Strand::draw_pixel(int offset, Colour c)
{
    assert(offset < get_num_pixels());
    m_pixels[offset] = c;
}

Colour Driver_spi::Strand::get_pixel(int offset)
{
    assert(offset < get_num_pixels());
    return m_pixels[offset];
}

void Driver_spi::setup_quad_spi_device(spi_device_handle_t &spi_handle,
                       spi_host_device_t spi_host,
                       int dma_channel,
                       int clock_pin,
                       int data0_pin, int data1_pin,
                       int data2_pin, int data3_pin)
{
    spi_bus_config_t bus_config;
    memset(&bus_config, 0, sizeof(bus_config));
    bus_config.mosi_io_num = data0_pin;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = clock_pin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 65536;
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
    bus_config.flags |= SPICOMMON_BUSFLAG_QUAD;
    // bus_config.flags |= SPICOMMON_BUSFLAG_DUAL;
    bus_config.miso_io_num = data1_pin;
    bus_config.quadwp_io_num = data2_pin;
    bus_config.quadhd_io_num = data3_pin;

    spi_device_interface_config_t dev_config;
    memset(&dev_config, 0, sizeof(dev_config));
    dev_config.mode = 0;
    dev_config.clock_speed_hz = 1000000;
    dev_config.clock_speed_hz = 10000000;
    // This might be a bit aggressive in many cases.
    dev_config.clock_speed_hz = 40000000;
    dev_config.spics_io_num = -1;
    dev_config.queue_size = 5;
#if 1
    dev_config.flags = SPI_DEVICE_HALFDUPLEX;
#endif

    auto retval = spi_bus_initialize(spi_host, &bus_config, dma_channel);
    assert(retval == ESP_OK);

    retval = spi_bus_add_device(spi_host, &dev_config, &spi_handle);
    assert(retval == ESP_OK);
}

Driver_spi::Driver_spi()
{
    memset(m_quad_buffer, 0, sizeof(m_quad_buffer));
}

Driver_spi::~Driver_spi()
{
    for (int i = 0; i < SPI_DEVICE_COUNT; ++i) {
    if (m_quad_buffer[i]) {
        heap_caps_free(m_quad_buffer[i]);
        m_quad_buffer[i] = NULL;
    }
    }
}

std::shared_ptr<Driver> Driver_spi::init(
    int hspi_clock_gpio_number,
    Driver_spi::Strand_config const *hspi_configs,
    int num_hspi_configs,
    int vspi_clock_gpio_number,
    Driver_spi::Strand_config const *vspi_configs,
    int num_vspi_configs)
{
    if (num_hspi_configs != Driver_spi::STRANDS_PER_SPI ||
    num_vspi_configs != Driver_spi::STRANDS_PER_SPI) {
    return std::shared_ptr<Driver>();
    }

    auto driver = std::make_shared<Driver_spi>();
    setup_quad_spi_device(driver->m_spi_handles[0], HSPI_HOST, 1,
                      hspi_clock_gpio_number,
              hspi_configs[0].m_gpio_number,
              hspi_configs[1].m_gpio_number,
              hspi_configs[2].m_gpio_number,
              hspi_configs[3].m_gpio_number);
    setup_quad_spi_device(driver->m_spi_handles[1], VSPI_HOST, 2,
                      vspi_clock_gpio_number,
              vspi_configs[0].m_gpio_number,
              vspi_configs[1].m_gpio_number,
              vspi_configs[2].m_gpio_number,
              vspi_configs[3].m_gpio_number);

    int led_count = 0;
    for (int i = 0; i < num_hspi_configs; ++i) {
    led_count = std::max(led_count, hspi_configs[i].m_num_pixels);
    driver->m_strands.push_back(std::make_shared<Strand>(hspi_configs[i]));
    }
    for (int i = 0; i < num_vspi_configs; ++i) {
    led_count = std::max(led_count, vspi_configs[i].m_num_pixels);
    driver->m_strands.push_back(std::make_shared<Strand>(vspi_configs[i]));
    }

    driver->m_led_count = led_count;
    driver->m_data_buffer_size = sizeof(uint32_t) * (1 + led_count + END_COUNT);
    driver->m_quad_buffer_size = 4 * driver->m_data_buffer_size;
    for (int i = 0; i < SPI_DEVICE_COUNT; ++i) {
    driver->m_quad_buffer[i] = reinterpret_cast<uint32_t*>(
        heap_caps_malloc(driver->m_quad_buffer_size, MALLOC_CAP_DMA));
    if (!driver->m_quad_buffer[i]) {
        // TODO: Cleanup after allocation failure.
        return std::shared_ptr<Driver>();
    }
        memset(&driver->m_transactions[i], 0,
               sizeof(driver->m_transactions[i]));
    }
    driver->m_transaction_active = false;

    return driver;
}

std::vector<std::shared_ptr<Driver::Strand>> Driver_spi::get_strands()
{
    std::vector<std::shared_ptr<Driver::Strand>> strands;
    strands.reserve(m_strands.size());

    for (int i = 0; i < m_strands.size(); ++i) {
        strands.push_back(m_strands[i]);
    }

    return strands;
}

static inline void start_frame(uint32_t *frame)
{
    *frame = 0;
}

static inline void led_frame(uint32_t *frame, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *byte = reinterpret_cast<uint8_t *>(frame);
#if 0
    byte[0] = 0xff;
    byte[1] = b;
    byte[2] = g;
    byte[3] = r;
#endif
#if 1
    // byte[0] = (0x7 << 5) | 4;
    byte[0] = (0x7 << 5) | 8;
    byte[1] = b;
    byte[2] = g;
    byte[3] = r;
#endif
}

static inline void led_frame(uint32_t *frame, Colour c)
{
    led_frame(frame, c.red, c.green, c.blue);
}

static inline void led_frame(uint32_t *frame, CRGB c)
{
    led_frame(frame, c.r, c.g, c.b);
}

static inline void end_frame(uint32_t *frame)
{
    *frame = 0xffffffff;
}

#if 0
// a 4 0
// b 5 1
// c 6 2
// d 7 3
// dest: 76543210
static inline void interleave_nibble(uint8_t *dest,
                  uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    *dest = (d & 0x2) << 6 |
        (c & 0x2) << 5 |
        (b & 0x2) << 4 |
        (a & 0x2) << 3 |
        (d & 0x1) << 3 |
        (c & 0x1) << 2 |
        (b & 0x1) << 1 |
        (a & 0x1);
}

// a7 a6 a5 a4 a3 a2 a1 a0
// b7 b6 b5 b4 b3 b2 b1 b0
// c7 c6 c5 c4 c3 c2 c1 c0
// d7 d6 d5 d4 d3 d2 d1 d0
// dest: d7 c7 b7 a7 d6 c6 b6 a6
static inline void interleave_spi(uint32_t *dest,
               uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint8_t *dest8 = reinterpret_cast<uint8_t *>(dest);
    interleave_nibble(dest8, a >> 6, b >> 6, c >> 6, d >> 6);
    interleave_nibble(dest8 + 1, a >> 4, b >> 4, c >> 4, d >> 4);
    interleave_nibble(dest8 + 2, a >> 2, b >> 2, c >> 2, d >> 2);
    interleave_nibble(dest8 + 3, a >> 0, b >> 0, c >> 0, d >> 0);
}
#endif

static inline void interleave_spi(uint32_t *dest,
                      uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    // Promote to 32-bit unsigned integers to work with 32-bit registers
    uint32_t A = a;
    uint32_t B = b;
    uint32_t C = c;
    uint32_t D = d;

    // The destination bit 'i' is composed of: d[i], c[i], b[i], a[i]
    // which are bits 3, 2, 1, 0 of the 4-bit block for each bit position 'i' (0 to 7).
    // The bits from D are placed in positions 3, 7, 11, 15, 19, 23, 27, 31 (0 mod 4)
    // The bits from C are placed in positions 2, 6, 10, 14, 18, 22, 26, 30 (1 mod 4)
    // The bits from B are placed in positions 1, 5, 9, 13, 17, 21, 25, 29 (2 mod 4)
    // The bits from A are placed in positions 0, 4, 8, 12, 16, 20, 24, 28 (3 mod 4)
    // NOTE: This assumes little-endian byte order for the final *dest to match the original function's structure.

    // Interleave bits from D (positions 3, 7, 11, ..., 31)
    // The bit D_i needs to be shifted by (4 * i) + 3
    // This is equivalent to interleaving D with 3 zero bits and then shifting right to align.
    // For Xtensa, which has a 32-bit architecture, this is generally the fastest C approach.
    // D is a single byte (bits 0-7), we only care about those.
    D = (D | (D << 12)) & 0x000F000F;
    D = (D | (D << 6)) & 0x03030303;
    D = (D | (D << 3)) & 0x11111111; // D[i] is now at bit (4*i) + 4

    // Interleave bits from C (positions 2, 6, 10, ..., 30)
    C = (C | (C << 12)) & 0x000F000F;
    C = (C | (C << 6)) & 0x03030303;
    C = (C | (C << 3)) & 0x11111111;

    // Interleave bits from B (positions 1, 5, 9, ..., 29)
    B = (B | (B << 12)) & 0x000F000F;
    B = (B | (B << 6)) & 0x03030303;
    B = (B | (B << 3)) & 0x11111111;

    // Interleave bits from A (positions 0, 4, 8, ..., 28)
    A = (A | (A << 12)) & 0x000F000F;
    A = (A | (A << 6)) & 0x03030303;
    A = (A | (A << 3)) & 0x11111111;

    //*dest = result;
    *dest = __builtin_bswap32((D << 3) | (C << 2) | (B << 1) | A);
}

// This is endian-agnostic because we operate on the integers directly.
inline void interleave_quad_words(uint32_t *dest,
                               uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    interleave_spi(dest,
                   reinterpret_cast<uint8_t *>(&a)[0],
                   reinterpret_cast<uint8_t *>(&b)[0],
                   reinterpret_cast<uint8_t *>(&c)[0],
                   reinterpret_cast<uint8_t *>(&d)[0]);
    interleave_spi(dest + 1,
                   reinterpret_cast<uint8_t *>(&a)[1],
                   reinterpret_cast<uint8_t *>(&b)[1],
                   reinterpret_cast<uint8_t *>(&c)[1],
                   reinterpret_cast<uint8_t *>(&d)[1]);
    interleave_spi(dest + 2,
                   reinterpret_cast<uint8_t *>(&a)[2],
                   reinterpret_cast<uint8_t *>(&b)[2],
                   reinterpret_cast<uint8_t *>(&c)[2],
                   reinterpret_cast<uint8_t *>(&d)[2]);
    interleave_spi(dest + 3,
                   reinterpret_cast<uint8_t *>(&a)[3],
                   reinterpret_cast<uint8_t *>(&b)[3],
                   reinterpret_cast<uint8_t *>(&c)[3],
                   reinterpret_cast<uint8_t *>(&d)[3]);
}

#if 0
static uint8_t interleave_nibble2(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (d & 0x2) << 6 |
       (c & 0x2) << 5 |
       (b & 0x2) << 4 |
       (a & 0x2) << 3 |
       (d & 0x1) << 3 |
       (c & 0x1) << 2 |
       (b & 0x1) << 1 |
       (a & 0x1);
}

static uint32_t interleave_spi2(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return interleave_nibble2(a >> 6, b >> 6, c >> 6, d >> 6) |
           interleave_nibble2(a >> 4, b >> 4, c >> 4, d >> 4) << 8 |
           interleave_nibble2(a >> 2, b >> 2, c >> 2, d >> 2) << 16 |
           interleave_nibble2(a >> 0, b >> 0, c >> 0, d >> 0) << 24;
}
#endif

// HACK: Generate APA102 data for one strip.
std::vector<uint32_t> Driver_spi::generate_apa102_data(Strand &strand)
{
    std::vector<uint32_t> buffer(m_data_buffer_size / sizeof(uint32_t));
    start_frame(buffer.data());
    for (int i = 0; i < strand.get_num_pixels(); ++i) {
    led_frame(buffer.data() + 1 + i, strand.get_pixel(i));
    }
    for (int i = 0;
     i < (m_led_count - strand.get_num_pixels()) + END_COUNT;
     ++i) {
    end_frame(buffer.data() + 1 + strand.get_num_pixels() + i);
    }

    return buffer;
}

std::vector<uint32_t> Driver_spi::generate_apa102_data(CRGB *leds, int num_leds)
{
#if 0
    std::vector<uint32_t> buffer(m_data_buffer_size / sizeof(uint32_t));
    start_frame(buffer.data());
    for (int i = 0; i < num_leds; ++i) {
        CRGB &c = leds[i];
    led_frame(buffer.data() + 1 + i, c);
    }
    for (int i = 0;
     i < (m_led_count - num_leds) + END_COUNT;
     ++i) {
    end_frame(buffer.data() + 1 + num_leds + i);
    }
#endif
#if 1
    int word_count = m_data_buffer_size / sizeof(uint32_t);
    std::vector<uint32_t> buffer(word_count);
    for (int i = 0; i < word_count; ++i) {
        buffer[i] = generate_apa102_word(leds, num_leds, i);
    }
#endif

    return buffer;
}

uint32_t Driver_spi::generate_apa102_word(CRGB *leds, int num_leds, int word_num)
{
    uint32_t word;
    if (word_num == 0) {
        start_frame(&word);
    } else if (word_num < num_leds + 1) {
        CRGB &c = leds[word_num - 1];
    led_frame(&word, c);
    } else {
        end_frame(&word);
    }
    return word;
}

void IRAM_ATTR Driver_spi::update2(CRGB *leds, int num_leds)
{
    // TODO: Replace all of this mess.  Generate data as needed instead of
    // using temporary buffers.
#undef PROFILE_UPDATE
#ifdef PROFILE_UPDATE
    unsigned long start = micros();
#endif

    if (m_transaction_active) {
        // Await SPI transactions.
        for (int spi = 0; spi < SPI_DEVICE_COUNT; ++spi) {
            spi_transaction_t *t_result;
            spi_device_get_trans_result(m_spi_handles[spi], &t_result, portMAX_DELAY);
            // printf("spi %d finished\n", spi);
        }
        m_transaction_active = false;
    }
#ifdef PROFILE_UPDATE
    unsigned long await_end = micros();
    unsigned long spi_start[SPI_DEVICE_COUNT];
    unsigned long interleave_end[SPI_DEVICE_COUNT];
    unsigned long queue_end[SPI_DEVICE_COUNT];
#endif

    for (int spi = 0; spi < SPI_DEVICE_COUNT; ++spi) {
#ifdef PROFILE_UPDATE
        spi_start[spi] = micros();
#endif
        uint32_t w[STRANDS_PER_SPI];
    // HACK: Interleave data of each strand.
    for (unsigned i = 0; i < m_data_buffer_size; i += 4) {
            w[0] = generate_apa102_word(leds, num_leds, i / 4);
            w[1] = generate_apa102_word(leds, num_leds, i / 4);
            w[2] = generate_apa102_word(leds, num_leds, i / 4);
            w[3] = generate_apa102_word(leds, num_leds, i / 4);
#if 0
        interleave_quad_words(m_quad_buffer[spi] + i, w[0], w[1], w[2], w[3]);
#endif
#if 1
            interleave_spi(m_quad_buffer[spi] + i,
                        reinterpret_cast<uint8_t *>(&w[0])[0],
                        reinterpret_cast<uint8_t *>(&w[1])[0],
                        reinterpret_cast<uint8_t *>(&w[2])[0],
                        reinterpret_cast<uint8_t *>(&w[3])[0]);
            interleave_spi(m_quad_buffer[spi] + i + 1,
                        reinterpret_cast<uint8_t *>(&w[0])[1],
                        reinterpret_cast<uint8_t *>(&w[1])[1],
                        reinterpret_cast<uint8_t *>(&w[2])[1],
                        reinterpret_cast<uint8_t *>(&w[3])[1]);
            interleave_spi(m_quad_buffer[spi] + i + 2,
                        reinterpret_cast<uint8_t *>(&w[0])[2],
                        reinterpret_cast<uint8_t *>(&w[1])[2],
                        reinterpret_cast<uint8_t *>(&w[2])[2],
                        reinterpret_cast<uint8_t *>(&w[3])[2]);
            interleave_spi(m_quad_buffer[spi] + i + 3,
                        reinterpret_cast<uint8_t *>(&w[0])[3],
                        reinterpret_cast<uint8_t *>(&w[1])[3],
                        reinterpret_cast<uint8_t *>(&w[2])[3],
                        reinterpret_cast<uint8_t *>(&w[3])[3]);
#endif
    }

#ifdef PROFILE_UPDATE
        interleave_end[spi] = micros();
#endif
    // Generate SPI transactions.
    memset(&m_transactions[spi], 0, sizeof(m_transactions[spi]));
    m_transactions[spi].flags = SPI_TRANS_MODE_QIO;

    // Transmit quad-SPI transaction.  Length in bits.
    m_transactions[spi].length = m_quad_buffer_size * 8;
    m_transactions[spi].tx_buffer = m_quad_buffer[spi];
    spi_device_queue_trans(m_spi_handles[spi], &m_transactions[spi], portMAX_DELAY);
    // printf("spi %d queued\n", spi);
#ifdef PROFILE_UPDATE
        queue_end[spi] = micros();
#endif
    }

    m_transaction_active = true;

#ifdef PROFILE_UPDATE
    static unsigned long last_log = 0;
    unsigned LOG_TIME = 10000000;
    if (start - last_log > LOG_TIME) {
        Serial.printf("await %ld int0 %ld queue0 %ld int1 %ld queue1 %ld\n",
                      await_end - start,
                      interleave_end[0] - spi_start[0],
                      queue_end[0] - interleave_end[0],
                      interleave_end[1] - spi_start[1],
                      queue_end[1] - interleave_end[1]);
        last_log = start;
    }
#endif
}

void Driver_spi::draw_pixel(Driver::Pixel_ptr ptr, Colour c)
{
    *to_pixel(ptr) = c;
}

Colour Driver_spi::get_pixel(Driver::Pixel_ptr ptr)
{
    return *to_pixel(ptr);
}
```

---

## Unit Test Requirements

**Critical for correctness**: Bit-interleaving is complex and must be validated before hardware integration.

### Test Data: APA102 Protocol with 4 Strips

Generate test data for 4 APA102 strips with different LED counts:
- **Lane 0**: 1 LED
- **Lane 1**: 2 LEDs
- **Lane 2**: 4 LEDs
- **Lane 3**: 7 LEDs (longest)

### APA102 Protocol Structure

For each lane, generate protocol bytes:
```
[START_FRAME(4)] + [LED_DATA(num_leds × 4)] + [END_FRAME(ceil(num_leds/32) × 4)]
```

**Lane 0 (1 LED)**: 4 + 4 + 4 = **12 bytes**
```
[0x00 0x00 0x00 0x00]           // Start frame
[0xFF 0xRR 0xGG 0xBB]           // LED 0 (brightness=0xFF, R, G, B)
[0xFF 0xFF 0xFF 0xFF]           // End frame
```

**Lane 1 (2 LEDs)**: 4 + 8 + 4 = **16 bytes**
```
[0x00 0x00 0x00 0x00]           // Start frame
[0xFF 0xRR 0xGG 0xBB]           // LED 0
[0xFF 0xRR 0xGG 0xBB]           // LED 1
[0xFF 0xFF 0xFF 0xFF]           // End frame
```

**Lane 2 (4 LEDs)**: 4 + 16 + 4 = **24 bytes**

**Lane 3 (7 LEDs)**: 4 + 28 + 4 = **36 bytes** ← longest

### Test 1: Forward Interleaving

**Input**: Per-lane buffers with known test patterns

```cpp
void test_quad_spi_interleaving() {
    // Create test data with distinct patterns for each lane
    std::vector<uint8_t> lane0 = generate_apa102_data(1, 0xAA);  // 12 bytes, pattern 0xAA
    std::vector<uint8_t> lane1 = generate_apa102_data(2, 0xBB);  // 16 bytes, pattern 0xBB
    std::vector<uint8_t> lane2 = generate_apa102_data(4, 0xCC);  // 24 bytes, pattern 0xCC
    std::vector<uint8_t> lane3 = generate_apa102_data(7, 0xDD);  // 36 bytes, pattern 0xDD

    // Interleave (Phase 3: Transpose)
    std::vector<uint8_t> interleaved = quad_spi_interleave(lane0, lane1, lane2, lane3);

    // Expected size: max_lane_bytes = 36
    ASSERT_EQ(interleaved.size(), 36);

    // Verify first 4 bytes (all START frames = 0x00)
    // Lane0[0]=0x00, Lane1[0]=0x00, Lane2[0]=0x00, Lane3[0]=0x00
    // Bit-interleaved: [0000][0000][0000][0000] = 0x00 for all 4 output bytes
    ASSERT_EQ(interleaved[0], 0x00);
    ASSERT_EQ(interleaved[1], 0x00);
    ASSERT_EQ(interleaved[2], 0x00);
    ASSERT_EQ(interleaved[3], 0x00);

    // Verify bytes 4-7 (first LED brightness byte = 0xFF for all)
    // Lane0[4]=0xFF, Lane1[4]=0xFF, Lane2[4]=0xFF, Lane3[4]=0xFF
    // All bits high → interleaved should be 0xFF
    ASSERT_EQ(interleaved[4], 0xFF);
    ASSERT_EQ(interleaved[5], 0xFF);
    ASSERT_EQ(interleaved[6], 0xFF);
    ASSERT_EQ(interleaved[7], 0xFF);

    // Verify zero-padding region (Lane0 ends at byte 12)
    // After byte 12, Lane0 should contribute 0x00
    // Check byte position 13 (interleaved index varies due to bit packing)
    // This requires understanding the exact interleaving algorithm

    // More detailed assertions below...
}
```

### Test 2: Inverse Transpose (De-interleaving)

**Critical**: Verify that `de_interleave(interleave(data)) == data`

```cpp
void test_quad_spi_inverse_transpose() {
    // Original test data
    std::vector<uint8_t> lane0_orig = generate_apa102_data(1, 0xAA);
    std::vector<uint8_t> lane1_orig = generate_apa102_data(2, 0xBB);
    std::vector<uint8_t> lane2_orig = generate_apa102_data(4, 0xCC);
    std::vector<uint8_t> lane3_orig = generate_apa102_data(7, 0xDD);

    // Forward interleave
    std::vector<uint8_t> interleaved = quad_spi_interleave(
        lane0_orig, lane1_orig, lane2_orig, lane3_orig
    );

    // Inverse transpose
    auto [lane0_out, lane1_out, lane2_out, lane3_out] = quad_spi_deinterleave(
        interleaved, 12, 16, 24, 36  // Original lane sizes
    );

    // Verify exact match
    ASSERT_EQ(lane0_out, lane0_orig);
    ASSERT_EQ(lane1_out, lane1_orig);
    ASSERT_EQ(lane2_out, lane2_orig);
    ASSERT_EQ(lane3_out, lane3_orig);
}
```

### Test 3: Bit-Level Verification

**Detailed bit pattern check** for a simple known pattern:

```cpp
void test_quad_spi_bit_pattern() {
    // Simple test: 4 lanes, 1 byte each
    std::vector<uint8_t> lane0 = {0b10101010};  // 0xAA
    std::vector<uint8_t> lane1 = {0b11001100};  // 0xCC
    std::vector<uint8_t> lane2 = {0b11110000};  // 0xF0
    std::vector<uint8_t> lane3 = {0b11111111};  // 0xFF

    std::vector<uint8_t> interleaved = quad_spi_interleave(lane0, lane1, lane2, lane3);

    // For quad-SPI, 1 byte from each lane → 4 interleaved bytes
    // (due to bit interleaving: 4 bits per output byte)
    ASSERT_EQ(interleaved.size(), 4);

    // Manual calculation of expected pattern:
    // Output byte 0: bits [7] from each lane
    //   Lane3[7]=1, Lane2[7]=1, Lane1[7]=1, Lane0[7]=1
    //   → [1111] in bits 3:0 → 0x0F (assuming nibble packing)
    //
    // This requires knowledge of exact interleaving algorithm
    // See interleave_quad_words() in demo code (lines 1073-1096)

    // Expected pattern (based on demo code algorithm):
    uint8_t expected[4];
    calculate_expected_interleave(lane0[0], lane1[0], lane2[0], lane3[0], expected);

    ASSERT_EQ(interleaved[0], expected[0]);
    ASSERT_EQ(interleaved[1], expected[1]);
    ASSERT_EQ(interleaved[2], expected[2]);
    ASSERT_EQ(interleaved[3], expected[3]);
}
```

### Helper Functions (Required for Tests)

```cpp
// Generate APA102 protocol data for N LEDs with specific RGB pattern
std::vector<uint8_t> generate_apa102_data(int num_leds, uint8_t rgb_pattern) {
    std::vector<uint8_t> data;

    // Start frame
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // LED data
    for (int i = 0; i < num_leds; i++) {
        data.push_back(0xFF);          // Brightness (max)
        data.push_back(rgb_pattern);   // R
        data.push_back(rgb_pattern);   // G
        data.push_back(rgb_pattern);   // B
    }

    // End frame (ceil(num_leds / 32) * 4 bytes)
    int end_frame_bytes = ((num_leds + 31) / 32) * 4;
    for (int i = 0; i < end_frame_bytes; i++) {
        data.push_back(0xFF);
    }

    return data;
}

// Quad-SPI interleave function (matches production code)
std::vector<uint8_t> quad_spi_interleave(
    const std::vector<uint8_t>& lane0,
    const std::vector<uint8_t>& lane1,
    const std::vector<uint8_t>& lane2,
    const std::vector<uint8_t>& lane3
) {
    size_t max_bytes = std::max({lane0.size(), lane1.size(), lane2.size(), lane3.size()});
    std::vector<uint8_t> output(max_bytes);

    uint32_t* output_words = reinterpret_cast<uint32_t*>(output.data());

    for (size_t byte_idx = 0; byte_idx < max_bytes; byte_idx += 4) {
        uint32_t words[4] = {0, 0, 0, 0};

        // Read 4 bytes from each lane (or 0 if lane is shorter)
        auto read_word = [](const std::vector<uint8_t>& lane, size_t idx) -> uint32_t {
            if (idx + 3 < lane.size()) {
                return *reinterpret_cast<const uint32_t*>(&lane[idx]);
            }
            return 0;  // Zero-padding
        };

        words[0] = read_word(lane0, byte_idx);
        words[1] = read_word(lane1, byte_idx);
        words[2] = read_word(lane2, byte_idx);
        words[3] = read_word(lane3, byte_idx);

        // Use production interleave_quad_words() function
        interleave_quad_words(&output_words[byte_idx / 4], words[0], words[1], words[2], words[3]);
    }

    return output;
}

// Inverse function: de-interleave back to per-lane format
std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, std::vector<uint8_t>, std::vector<uint8_t>>
quad_spi_deinterleave(
    const std::vector<uint8_t>& interleaved,
    size_t lane0_size,
    size_t lane1_size,
    size_t lane2_size,
    size_t lane3_size
) {
    std::vector<uint8_t> lane0(lane0_size, 0);
    std::vector<uint8_t> lane1(lane1_size, 0);
    std::vector<uint8_t> lane2(lane2_size, 0);
    std::vector<uint8_t> lane3(lane3_size, 0);

    const uint32_t* input_words = reinterpret_cast<const uint32_t*>(interleaved.data());

    for (size_t byte_idx = 0; byte_idx < interleaved.size(); byte_idx += 4) {
        uint32_t interleaved_words[4];
        memcpy(interleaved_words, &input_words[byte_idx / 4], 16);

        // Reverse the interleave_quad_words() operation
        uint32_t words[4];
        deinterleave_quad_words(interleaved_words, &words[0], &words[1], &words[2], &words[3]);

        // Write back to lanes (if within bounds)
        auto write_word = [](std::vector<uint8_t>& lane, size_t idx, uint32_t word) {
            if (idx + 3 < lane.size()) {
                *reinterpret_cast<uint32_t*>(&lane[idx]) = word;
            }
        };

        write_word(lane0, byte_idx, words[0]);
        write_word(lane1, byte_idx, words[1]);
        write_word(lane2, byte_idx, words[2]);
        write_word(lane3, byte_idx, words[3]);
    }

    return {lane0, lane1, lane2, lane3};
}

// Bit-level de-interleave (inverse of interleave_quad_words)
void deinterleave_quad_words(const uint32_t interleaved[4], uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    // Reverse the bit interleaving operation
    // This is the inverse of the algorithm in lines 1027-1070

    *a = 0;
    *b = 0;
    *c = 0;
    *d = 0;

    // For each of the 4 interleaved words (32 bits each = 128 bits total)
    // Extract bits belonging to each lane
    // Lane assignment (from interleave_spi line 1069):
    //   D (lane3) at bit position 3
    //   C (lane2) at bit position 2
    //   B (lane1) at bit position 1
    //   A (lane0) at bit position 0

    for (int byte_pos = 0; byte_pos < 4; byte_pos++) {
        uint8_t interleaved_byte = reinterpret_cast<const uint8_t*>(interleaved)[byte_pos];

        for (int bit = 0; bit < 8; bit++) {
            // Extract 4-bit nibble (one bit from each lane)
            uint8_t nibble = (interleaved_byte >> (bit * 4)) & 0x0F;

            // Distribute to lanes
            uint8_t bit_a = (nibble >> 0) & 1;
            uint8_t bit_b = (nibble >> 1) & 1;
            uint8_t bit_c = (nibble >> 2) & 1;
            uint8_t bit_d = (nibble >> 3) & 1;

            // Place in output words
            int output_bit_pos = byte_pos * 8 + bit;
            *a |= (bit_a << output_bit_pos);
            *b |= (bit_b << output_bit_pos);
            *c |= (bit_c << output_bit_pos);
            *d |= (bit_d << output_bit_pos);
        }
    }
}
```

### Test File Location

Create: `tests/test_quad_spi_interleave.cpp`

### Validation Criteria

All tests must pass before hardware integration:
- ✅ Forward interleave produces expected bit patterns
- ✅ Inverse transpose exactly recovers original data
- ✅ Zero-padding works correctly for shorter lanes
- ✅ Different lane sizes (1, 2, 4, 7 LEDs) all handled correctly
- ✅ Bit-level patterns match manual calculations

### Why This Matters

Bit-interleaving bugs are **extremely difficult to debug** on hardware:
- Hardware quad-SPI shows garbled LED output (hard to interpret)
- No visibility into DMA buffer contents during transmission
- Timing-dependent failures may only occur at certain clock speeds
- **Unit tests catch these issues immediately** in software

**Bottom line**: Test the interleaving math thoroughly in software BEFORE touching hardware.

---