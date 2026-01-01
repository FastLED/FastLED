# FastLED Hardware Validation System

This validation system creates a **hardware-in-the-loop testing feedback loop** for FastLED LED drivers. It verifies that SPI/RMT/PARLIO LED output matches expectations by reading back timing values using the ESP32's RMT peripheral in receive mode.

## Quick Start (TL;DR)

**Hardware Setup:**
1. Get an ESP32 board (any variant: ESP32, ESP32-S3, ESP32-C3, ESP32-C6)
2. **Connect GPIO 0 to GPIO 1** with a jumper wire (or solder them together)
3. Connect USB cable for programming

**Run Validation:**
```bash
bash validate
```

**What happens:**
- ESP32 transmits LED data on GPIO 0 (TX)
- Signal loops back via jumper wire to GPIO 1 (RX)
- RMT receiver measures timing and decodes signal
- Validator compares TX vs RX → Reports PASS ✓ or FAIL ✗
- **48 test cases** run automatically (all drivers, lane counts, strip sizes)

**Why GPIO 0 and GPIO 1?**
- **Adjacent on most dev boards** → easy to wire
- **Labeled "TX" and "RX"** → easy to find
- **Universal** → work on all ESP32 variants

## Why This Matters: Automated Feedback Loop

**For AI Agents:**
- Make code changes to FastLED drivers (RMT, SPI, PARLIO)
- Run `bash validate` to compile, upload, and verify on real hardware
- Get immediate pass/fail feedback from actual LED signal analysis
- Iterate rapidly with hardware-verified confidence

**For Humans:**
- Quick sanity check after driver modifications
- Comprehensive regression testing across all drivers
- Visual confirmation that LED protocols work correctly
- No need for oscilloscope - the ESP32 measures timing for you

**The Feedback Loop:**
```
Code Change → Compile → Upload → Hardware Test → Pass/Fail → Iterate
     ↑                                                           |
     └───────────────────────────────────────────────────────────┘
```

## ✨ NEW: Test Matrix Validation

This sketch now supports **comprehensive matrix testing** with the following dimensions:
- **Drivers**: SPI, RMT, PARLIO (all available drivers)
- **Lane Counts**: 1-8 lanes (multi-lane testing)
- **Strip Sizes**: Short (10 LEDs) and Long (300 LEDs)
- **Bit Patterns**: 4 mixed RGB patterns to test MSB vs LSB handling

**Full test matrix**: 3 drivers × 8 lane counts × 2 strip sizes = **48 test cases**

Use preprocessor defines to narrow the scope for faster debugging or focused testing.

## How It Works: TX→RX Loopback

The validation creates a **loopback circuit** where the ESP32 talks to itself:

```
┌─────────────────────────────────────────────────────────┐
│  ESP32 Microcontroller                                  │
│                                                          │
│  FastLED Driver (TX)  →  GPIO 0 (PIN_TX)                │
│       |                      |                           │
│       |                      | (jumper wire or          │
│       v                      v  solder bridge)          │
│  Generates LED            Physical                      │
│  timing signals           loopback                      │
│  (WS2812B protocol)          |                          │
│                              v                           │
│  RMT Receiver (RX)  ←  GPIO 1 (PIN_RX)                  │
│       |                                                  │
│       v                                                  │
│  Decodes timing                                         │
│  symbols back                                           │
│  to RGB values                                          │
│                                                          │
│  Compare TX vs RX  →  PASS ✓ / FAIL ✗                   │
└─────────────────────────────────────────────────────────┘
```

**The process:**
1. **FastLED Driver (TX)** sends LED data through GPIO 0 (PIN_TX)
2. **Physical jumper wire** connects GPIO 0 to GPIO 1 (or solder them together)
3. **RMT Receiver (RX)** captures timing symbols from GPIO 1 (PIN_RX)
4. **Decoder** converts timing back to RGB values
5. **Validator** compares transmitted vs received data
   - If they match → **SUCCESS** ✓
   - If they differ → **ERROR** ✗ (test fails immediately)

## Overview

The validation sketch tests the entire LED data transmission pipeline:
1. **Generate Test Pattern** - Create LED colors in TX buffer (4 mixed bit patterns)
2. **Start RX Reception** - Arm RMT RX channel to capture incoming signal
3. **Transmit via Driver** - Send data to LEDs using FastLED's driver (SPI/RMT/PARLIO)
4. **Capture with RMT RX** - Record timing symbols from the loopback wire
5. **Decode Symbols** - Convert RMT timing symbols back to byte data
6. **Validate** - Compare transmitted vs received LED colors

## Test Matrix Configuration

### Preprocessor Defines

Control which test combinations to run by defining these macros in `Validation.ino`:

#### Driver Selection
```cpp
// Uncomment to test ONLY a specific driver:
// #define JUST_PARLIO  // Test only PARLIO driver
// #define JUST_RMT     // Test only RMT driver
// #define JUST_SPI     // Test only SPI driver
// Default: Test all available drivers
```

#### Lane Range
```cpp
// Override lane range (default: 1-8):
#define MIN_LANES 1  // Minimum number of lanes to test
#define MAX_LANES 8  // Maximum number of lanes to test
```

Each lane has a **decreasing LED count**:
- Lane 0: `base_size` LEDs
- Lane 1: `base_size - 1` LEDs
- Lane 2: `base_size - 2` LEDs
- Lane N: `base_size - N` LEDs

**Example**: For short strips (10 LEDs), lanes have 10, 9, 8, 7, 6, 5, 4, 3 LEDs.

**Multi-lane limitation**: Only Lane 0 is validated via RX loopback (hardware limitation). All lanes transmit, but only the first lane is verified.

#### Strip Size
```cpp
// Uncomment to test ONLY a specific strip size:
// #define JUST_SMALL_STRIPS  // Test only short strips (10 LEDs)
// #define JUST_LARGE_STRIPS  // Test only long strips (300 LEDs)
// Default: Test both sizes
```

Strip size constants:
- `SHORT_STRIP_SIZE = 10` (short strips)
- `LONG_STRIP_SIZE = 300` (long strips)

### Configuration Examples

**Example 1: Test only RMT with 4 lanes on small strips**
```cpp
#define JUST_RMT
#define MIN_LANES 4
#define MAX_LANES 4
#define JUST_SMALL_STRIPS
```
Result: 1 test case (RMT × 4 lanes × small strips)

**Example 2: Test all drivers with 1-3 lanes on large strips**
```cpp
#define MAX_LANES 3
#define JUST_LARGE_STRIPS
```
Result: 9 test cases (3 drivers × 3 lane counts × large strips)

**Example 3: Full matrix (default)**
```cpp
// No defines - tests everything
```
Result: 48 test cases (3 drivers × 8 lane counts × 2 strip sizes)

## Hardware Setup

### Required Components
- ESP32 board (any variant: ESP32, ESP32-S3, ESP32-C3, ESP32-C6)
- **1x jumper wire** OR **solder bridge** (connecting GPIO 0 to GPIO 1)
- USB cable for programming and serial monitor

### Optional Components
- WS2812B LED strip (for visual confirmation)
- 5V power supply (if using LED strip)

## Why GPIO 0 and GPIO 1?

**Default pins used by this validation:**
- **GPIO 0** = TX pin (FastLED driver output)
- **GPIO 1** = RX pin (RMT receiver input)

**Why these pins are convenient:**

1. **Adjacent on most dev boards** - Easy to bridge with a single jumper wire or solder blob
2. **Universal availability** - Present on all ESP32 variants (classic, S3, C3, C6)
3. **Boot-safe** - GPIO 0 is a strapping pin but works fine after boot completes
4. **TX/RX labeling** - Often labeled as "TX" and "RX" on dev boards (UART0), making them easy to identify

**Physical setup options:**

**Option A: Jumper wire (temporary, reusable)**
- Use a short female-to-female jumper wire (<10cm)
- Connect GPIO 0 pin to GPIO 1 pin
- Easy to remove when done testing

**Option B: Solder bridge (permanent, production)**
- Solder a wire between GPIO 0 and GPIO 1 pads on the PCB
- Creates a permanent loopback for continuous validation
- Useful for dedicated test fixtures

## Wiring Diagram

### Minimal Setup (Validation Only)

```
                 ESP32 DevKit
        ┌────────────────────────────────┐
        │                                │
        │   GPIO 0  (TX) ●───────────────●──┐
        │                │               │  │
        │                │               │ Jumper wire
        │                │               │ or solder
        │   GPIO 1  (RX) ●───────────────●──┘
        │                                │
        │   USB ─────────────────→ Computer
        │                                │
        └────────────────────────────────┘
```

**Steps:**
1. **Connect GPIO 0 to GPIO 1** with a jumper wire (or solder them together)
2. **No LED strip required** for basic validation
3. **Upload sketch** and open Serial Monitor (115200 baud)
4. **Watch for PASS/FAIL** - The ESP32 will test itself automatically

### Full Setup (With LED Strip)


**Steps:**
1. Connect GPIO 5 to LED strip Data In
2. Connect LED strip Data Out to GPIO 6 (loopback)
3. Connect LED strip GND to ESP32 GND
4. Connect LED strip 5V to external 5V power supply (DO NOT use USB power for LED strip)
5. Upload sketch and open Serial Monitor

**Warning:** LED strips draw significant current. Always use an external power supply, not USB power.

## GPIO Pin Customization

The default pins are defined in `Validation.ino`:

```cpp
#define PIN_TX 0       // FastLED driver output (default: GPIO 0)
#define PIN_RX 1       // RMT receiver input (default: GPIO 1)
```

**To use different pins:**
1. Edit `PIN_TX` and `PIN_RX` in the sketch
2. Update your jumper wire or solder bridge to connect the new pins
3. Recompile and upload

**Pin Constraints:**
- **TX pin (PIN_TX)**: Any GPIO that supports the driver peripheral (RMT/SPI/PARLIO)
  - Most GPIOs work for RMT
  - GPIO 11 recommended for SPI on ESP32-S3 (IO_MUX bypass, 80MHz capable)
  - Check datasheet for PARLIO-capable pins
- **RX pin (PIN_RX)**: Any GPIO works (RMT RX uses GPIO matrix routing)
- **Avoid USB pins**: GPIO 19, 20 on ESP32-S3 (if using native USB)

**Why GPIO 0 and 1 are recommended:**
- **Adjacent pins** on most dev boards make wiring trivial
- **Clearly labeled** as "TX" and "RX" on many boards
- **GPIO 0 boot constraint**: Not an issue - validation runs *after* boot completes

## Usage

### Manual Testing (Human Workflow)

1. **Wire the hardware** - Connect GPIO 0 to GPIO 1 with jumper wire (or solder bridge)
2. **Upload sketch** - Use Arduino IDE or PlatformIO
3. **Open Serial Monitor** - Set baud rate to 115200
4. **Observe results** - Sketch runs the full test matrix automatically (48 test cases by default)

### Automated Testing (AI Agent Workflow)

**From the FastLED project root directory:**

```bash
bash validate
```

**What this does:**
1. **Phase 0**: Ensures PlatformIO packages are installed (daemon-managed)
2. **Phase 1**: Runs C++ linters (catches ISR errors, code quality issues)
3. **Phase 2**: Compiles the Validation.ino sketch for the attached ESP32 board
4. **Phase 3**: Uploads firmware (auto-resolves port conflicts)
5. **Phase 4**: Monitors serial output with pattern matching
   - **--expect**: 12 patterns (hardware setup, drivers, test matrix, halting state)
   - **--fail-on**: "ERROR" keyword (test failures)
6. **Exit code**: 0 (success) or 1 (failure)

**This creates a complete feedback loop:**
```
Code change → Lint → Compile → Upload → Test on hardware → PASS/FAIL
     ↑                                                          |
     └──────────────────────────────────────────────────────────┘
```

**Quick iteration (skip linting):**
```bash
bash validate --skip-lint
```

**Custom timeout:**
```bash
bash validate --timeout 60  # 60 seconds instead of default 20s
```

**See all options:**
```bash
bash validate --help
```

### Expected Serial Output

```
=== FastLED RMT RX Validation Sketch ===
TX Pin: 0
RX Pin: 1
RX Device: RMT
LOOP BACK MODE: JUMPER WIRE
NUM_LEDS: Varies by test case

╔════════════════════════════════════════════════════════════════╗
║ TEST MATRIX CONFIGURATION                                      ║
╚════════════════════════════════════════════════════════════════╝
Drivers (3):
  - RMT
  - SPI
  - PARLIO
Lane Range: 1-8 (8 configurations)
Strip Sizes: Both (Short=10, Long=300)
Total Test Cases: 48

╔════════════════════════════════════════════════════════════════╗
║ FRAME 1 - Test Matrix Validation
╚════════════════════════════════════════════════════════════════╝

Test Matrix: 48 test case(s)

════════════════════════════════════════════════════════════════
TEST CASE 1/48
════════════════════════════════════════════════════════════════

╔════════════════════════════════════════════════════════════════╗
║ TEST CASE: RMT | 1 lane(s) | 10 LEDs
╚════════════════════════════════════════════════════════════════╝

[Lane 0] Pin: 0, LEDs: 10

=== Pattern A (R=0xF0, G=0x0F, B=0xAA) [Lane 0/1, Pin 0, LEDs 10] ===
Bytes Captured: 30 (expected: 30)
Accuracy: 100.0% (10/10 LEDs match)
Result: PASS ✓

[... 3 more patterns for this test case ...]

[PASS] Test case RMT (1 lanes, 10 LEDs) completed successfully

[... 47 more test cases ...]

╔════════════════════════════════════════════════════════════════════════════╗
║ TEST MATRIX RESULTS SUMMARY                                                ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Driver  │ Lanes │ Strip │ Status     │ Tests Passed │ Total Tests        ║
╠═════════╪═══════╪═══════╪════════════╪══════════════╪════════════════════╣
║ RMT     │     1 │    10 │ PASS ✓     │ 4            │ 4                  ║
║ RMT     │     1 │   300 │ PASS ✓     │ 4            │ 4                  ║
║ RMT     │     2 │    10 │ PASS ✓     │ 4            │ 4                  ║
[... additional test cases ...]
║ PARLIO  │     8 │   300 │ PASS ✓     │ 4            │ 4                  ║
╠═════════╧═══════╧═══════╧════════════╧══════════════╧════════════════════╣
║ OVERALL: 192/192 tests passed (100%)                                      ║
╚════════════════════════════════════════════════════════════════════════════╝

[TEST MATRIX] ✓ All test cases PASSED

╔════════════════════════════════════════════════════════════════╗
║ TEST MATRIX ITERATION COMPLETE                                 ║
╚════════════════════════════════════════════════════════════════╝

========== RESTARTING TEST MATRIX ==========
```

**Note**: Each test case runs 4 bit pattern tests. With 48 test cases, the full matrix runs 192 validation tests total (48 × 4).

## Test Patterns

The sketch runs **4 mixed RGB bit patterns** to comprehensively test MSB vs LSB handling:

### Pattern A: R=0xF0, G=0x0F, B=0xAA
- **R channel**: `0xF0` (11110000) - High nibble set (tests high bits)
- **G channel**: `0x0F` (00001111) - Low nibble set (tests low bits)
- **B channel**: `0xAA` (10101010) - Alternating bits (tests MSB/LSB order)

**Purpose**: Detects if driver incorrectly inverts bit order within bytes

### Pattern B: R=0x55, G=0xFF, B=0x00
- **R channel**: `0x55` (01010101) - Alternating bits (opposite of 0xAA)
- **G channel**: `0xFF` (11111111) - All bits high (boundary test)
- **B channel**: `0x00` (00000000) - All bits low (boundary test)

**Purpose**: Tests boundary conditions and alternating bit patterns

### Pattern C: R=0x0F, G=0xAA, B=0xF0
- **R channel**: `0x0F` (00001111) - Low nibble set
- **G channel**: `0xAA` (10101010) - Alternating bits
- **B channel**: `0xF0` (11110000) - High nibble set

**Purpose**: Rotated pattern from A - ensures driver handles different channel values correctly

### Pattern D: RGB Solid Alternating
- **LED 0**: Red (255, 0, 0)
- **LED 1**: Green (0, 255, 0)
- **LED 2**: Blue (0, 0, 255)
- **LED 3**: Red (repeating pattern)

**Purpose**: Baseline test - ensures basic RGB transmission works

### Why Mixed Bit Patterns Matter

Traditional solid color tests (all red, all green, all blue) only exercise a limited subset of bit combinations. Mixed patterns catch subtle encoding bugs:

- **MSB-first vs LSB-first**: If driver sends bits in wrong order, `0xF0` becomes `0x0F`
- **Nibble swapping**: High/low nibble errors are immediately visible
- **Bit inversion**: Patterns like `0xAA` and `0x55` expose bit flip errors
- **Channel crosstalk**: Different values per channel detect cross-channel corruption

## Troubleshooting

### Error: RX channel init failed

**Cause:** RMT RX peripheral initialization failed

**Solutions:**
- Check that you're using a supported ESP32 variant
- Verify GPIO 6 is not already in use by another peripheral
- Try a different RX pin (see GPIO Pin Customization above)

### Error: RX timeout (no signal received)

**Cause:** No signal detected on RX pin

**Solutions:**
- Verify jumper wire connection between GPIO 5 and GPIO 6
- Check that wire is making good contact (not loose)
- Ensure no other device is driving GPIO 6
- Try swapping the jumper wire (could be broken)

### Error: Decode failed (high error rate)

**Cause:** Timing symbols don't match WS2812B protocol

**Solutions:**
- Check for electromagnetic interference (keep wires short)
- Verify you're using WS2812B LEDs (not APA102 or other chipsets)
- Ensure clock resolution matches (40MHz is default)
- Try reducing LED count (fewer LEDs = fewer symbols to decode)

### MISMATCH: TX and RX colors don't match

**Cause:** Specific LED data corruption

**Solutions:**
- Check wire quality (use short, shielded wire if possible)
- Verify power supply stability
- Look for patterns (e.g., always same LED → wiring issue)
- Reduce brightness: `FastLED.setBrightness(64)` for less noise

### Tests pass but LEDs don't light up

**Cause:** LED strip not properly connected or powered

**Solutions:**
- Verify LED strip power supply (5V, sufficient amperage)
- Check LED strip Data In connects to GPIO 5 (not GPIO 6)
- Ensure LED strip GND connects to ESP32 GND (common ground)
- Test LEDs with a simpler sketch first (e.g., `examples/Blink`)

## Technical Details

### RMT RX Configuration
- **Clock Resolution**: 40 MHz (25ns per tick)
- **Buffer Size**: 1024 symbols (~42 LEDs maximum)
- **Idle Threshold**: 50μs (reset pulse detection)

### WS2812B Timing Thresholds
The decoder uses the following timing ranges (±150ns tolerance):

| Bit | High Time (T_H) | Low Time (T_L) |
|-----|----------------|----------------|
| 0   | 250-550 ns     | 700-1000 ns    |
| 1   | 650-950 ns     | 300-600 ns     |
| Reset | N/A          | >50 μs         |

### Performance Metrics
- **TX Duration**: ~30μs per LED (300μs for 10 LEDs)
- **RX Capture**: Asynchronous (non-blocking)
- **Decode Time**: <1ms for typical patterns
- **Accuracy**: >99% for clean signals

## Platform Compatibility

| Platform | Architecture | Status | Notes |
|----------|-------------|---------|-------|
| ESP32    | Xtensa      | ✅ Supported | Reference implementation |
| ESP32-S3 | Xtensa      | ✅ Supported | Recommended platform |
| ESP32-C3 | RISC-V      | ✅ Supported | Lower GPIO count |
| ESP32-C6 | RISC-V      | ✅ Supported | Latest RISC-V variant |

## Why This Validation System Exists

### The Problem
Traditional LED driver development requires:
- **Oscilloscopes** to verify timing (expensive, requires expertise)
- **Manual testing** with LED strips (slow, subjective)
- **Trial and error** to debug protocol issues (inefficient)
- **No automated regression testing** (breaks silently)

### The Solution
This validation system provides:

**1. Self-Testing Hardware**
- ESP32 measures its own output timing (no oscilloscope needed)
- Automated pass/fail validation (objective, repeatable)
- Tests all drivers: RMT, SPI, PARLIO (comprehensive coverage)

**2. Rapid Iteration Loop**
- **For AI agents**: Run `bash validate` after code changes → get hardware-verified results in ~30 seconds
- **For humans**: Quick sanity check without external test equipment
- **For CI/CD**: Automated hardware-in-the-loop testing (with attached ESP32 board)

**3. Comprehensive Testing**
- **48 test cases** by default (3 drivers × 8 lane counts × 2 strip sizes)
- **192 validation tests** total (48 cases × 4 bit patterns)
- **Mixed bit patterns** catch subtle encoding bugs (MSB/LSB order, nibble swaps, bit inversions)

**4. Real Hardware Feedback**
- Tests actual peripheral behavior (not simulation)
- Catches timing issues, DMA bugs, ISR race conditions
- Validates that LEDs will work correctly in production

### Use Cases

**AI Agent Development:**
```
Agent: "I'll modify the PARLIO driver to fix the timing bug"
Agent: "Running bash validate to verify the fix..."
Agent: "✓ All tests passed - fix confirmed on real hardware"
```

**Human Quick Check:**
```
Developer: "Did my SPI driver change break anything?"
Developer: "bash validate"
Developer: "✓ All 48 test cases pass - safe to commit"
```

**Regression Testing:**
```
CI System: "New PR submitted - testing on hardware..."
CI System: "bash validate"
CI System: "✗ Test failure on PARLIO 8-lane case - blocking merge"
```

## Related Documentation

- **Architecture**: `docs/RMT_RX_ARCHITECTURE.md` - System design and algorithms
- **RX Channel API**: `src/platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h`
- **Decoder API**: `src/platforms/esp/32/drivers/rmt_rx/rmt_rx_decoder.h`
- **ESP-IDF RMT Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html

## License

This example is part of the FastLED library and follows the same license terms.
