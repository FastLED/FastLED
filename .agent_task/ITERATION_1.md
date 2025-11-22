# Iteration 1 Summary: ESP32-C6 PARLIO Clock Investigation

## Iteration Goal
Identify a working clock frequency for the ESP32-C6 PARLIO peripheral to enable WS2812 LED control.

## Work Completed

### 1. Code Modifications
- **Expanded waveform buffer** (`channel_engine_parlio.h:268-269`)
  - Changed from `fl::array<uint8_t, 4>` to `fl::array<uint8_t, 16>`
  - Allows support for slower clock frequencies with more pulses per bit

- **Added systematic frequency testing** (`channel_engine_parlio.cpp:614-654`)
  - Programmatically tests 11 different frequencies (1-80 MHz)
  - Tests each frequency by attempting to create TX unit
  - Logs success/failure for each frequency

- **Tested multiple clock configurations**
  - 10 MHz with timing parameters: 400ns, 450ns, 400ns
  - 8 MHz with timing parameters: 375ns, 500ns, 375ns

### 2. Testing Results
Performed device testing with: `uv run ci/debug_attached.py esp32c6 --upload-port COM18`

**ALL frequencies FAILED** with error: `E (XXXX) parlio-tx: parlio_select_periph_clock(245): invalid clock source frequency`

Tested frequencies:
- 1 MHz → FAIL (err=258)
- 2 MHz → FAIL (err=258)
- 2.5 MHz → FAIL (err=258)
- 4 MHz → FAIL (err=258)
- 5 MHz → FAIL (err=258)
- 8 MHz → FAIL (err=258)
- 10 MHz → FAIL (err=258)
- 16 MHz → FAIL (err=258)
- 20 MHz → FAIL (err=258)
- 40 MHz → FAIL (err=258)
- 80 MHz → FAIL (err=258)

Error code 258 = `ESP_ERR_NOT_SUPPORTED`

### 3. Research Findings

#### ESP32-C6 PARLIO Hardware Support
- ✅ ESP32-C6 **DOES have** PARLIO TX hardware (SOC_PARLIO_SUPPORTED defined)
- ✅ Official ESP-IDF documentation confirms ESP32-C6 support
- ✅ Official ESP-IDF example exists: `examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix`
- ✅ Example README explicitly lists ESP32-C6 as supported target

#### Hardware Capabilities
- Full-duplex: up to 8-bit parallel data transfer
- Half-duplex: up to 16-bit parallel data transfer
- GDMA integration for efficient transfers
- **Limitation**: Transaction length can't be controlled by DMA on ESP32-C6 (requires CPU intervention)

#### Hardware Details from Device
- Chip: ESP32-C6FH4 (QFN32)
- Revision: v0.1
- Crystal: 40 MHz
- Features: WiFi 6, BT 5, IEEE802.15.4

## Critical Finding
**ALL tested clock frequencies fail on ESP32-C6**, despite:
1. Hardware support being present
2. Official ESP-IDF examples existing for this chip
3. Documentation confirming PARLIO TX functionality

## Root Cause Hypotheses

### Hypothesis 1: ESP-IDF Version Mismatch (MOST LIKELY)
The Arduino ESP32 framework may be using an ESP-IDF version that:
- Predates full ESP32-C6 PARLIO support
- Has bugs in ESP32-C6 PARLIO clock selection
- Requires v5.1+ for ESP32-C6 PARLIO TX

**Evidence**:
- PlatformIO uses Arduino ESP32 framework, not native ESP-IDF
- Arduino framework may lag behind ESP-IDF stable releases
- Framework version shown: `framework-arduinoespressif32 @ 3.2.0` / `framework-arduinoespressif32-libs @ 5.4.0`

### Hypothesis 2: Hardware Revision Issue
Chip revision v0.1 may have silicon bugs affecting PARLIO clock configuration.

**Evidence**:
- Early revision detected (v0.1)
- PARLIO is relatively new peripheral in ESP32-C6

### Hypothesis 3: Clock Source Enum Issue
Using `PARLIO_CLK_SRC_DEFAULT` may not work; need explicit clock source selection.

**Evidence**:
- ESP-IDF allows multiple clock source options: XTAL, PLL_F80M, EXTERNAL
- DEFAULT may not be properly defined for ESP32-C6

### Hypothesis 4: Missing Initialization
May need specific GPIO configuration or clock tree initialization before PARLIO can use clocks.

**Evidence**:
- PARLIO is integrated with complex clock tree
- May require enabling specific clock gates or peripheral buses

## Files Modified
1. `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.h` (lines 268-270)
2. `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp` (lines 42-46, 580-593, 614-654)
3. `LOOP.md` (updated with iteration 1 results)

## Recommendations for Next Iteration

### Priority 1: Test Explicit Clock Sources
Modify `channel_engine_parlio.cpp` to test specific clock sources:
```cpp
config.clk_src = PARLIO_CLK_SRC_XTAL;      // 40 MHz crystal
config.clk_src = PARLIO_CLK_SRC_PLL_F80M;  // 80 MHz PLL
config.clk_src = PARLIO_CLK_SRC_EXTERNAL;  // External clock
```

### Priority 2: Check ESP-IDF Version
Investigate Arduino framework's ESP-IDF version:
- Check `.platformio/packages/framework-arduinoespressif32/`
- Verify it includes ESP32-C6 PARLIO support (v5.1+)
- Consider using native ESP-IDF if version too old

### Priority 3: Test Official Example
Build and test official ESP-IDF example on same hardware:
```bash
git clone --depth 1 --branch v5.3 https://github.com/espressif/esp-idf.git
cd esp-idf/examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix
idf.py set-target esp32c6
idf.py build flash monitor
```
If it works, compare initialization sequence with FastLED code.

### Priority 4: Fallback to RMT (Interim Solution)
While investigating PARLIO issue, enable RMT driver for ESP32-C6:
- Increase RMT priority to 200 (highest)
- Verify LEDs work with RMT
- Allows project progress while debugging PARLIO

### Priority 5: Cleanup Debug Code
Once solution found, remove frequency testing code (cpp:614-654).

## Questions for Next Agent
1. Can we check the ESP-IDF version used by Arduino framework?
2. Should we test the official ESP-IDF example to confirm hardware works?
3. Are there any other ESP32-C6 PARLIO examples in FastLED or similar projects?
4. Should we add conditional compilation to skip PARLIO on ESP32-C6 until fixed?

## Time Spent
- Code modifications: ~15 minutes
- Device testing: ~10 minutes
- Research/documentation: ~20 minutes
- Total: ~45 minutes

## Status
**BLOCKED**: Cannot proceed with PARLIO on ESP32-C6 until root cause identified.

**Workaround Available**: RMT driver should work as fallback.

**Next Iteration Required**: Yes - investigate ESP-IDF version or test official examples.
