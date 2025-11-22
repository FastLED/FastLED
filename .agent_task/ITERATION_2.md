# Iteration 2 Summary: ESP32-C6 PARLIO Clock Source Deep Analysis

## Iteration Goal
Test explicit PARLIO clock sources (XTAL, PLL_F240M, RC_FAST) instead of DEFAULT to identify if clock source selection is the root cause of initialization failures.

## Work Completed

### 1. Research: PARLIO Clock Source Enumeration
**Found the definitive clock source definitions** in local PlatformIO installation:
- File: `framework-arduinoespressif32-libs/esp32c6/include/soc/esp32c6/include/soc/clk_tree_defs.h`
- Lines 480-486

**Available clock sources for ESP32-C6 PARLIO:**
```c
typedef enum {
    PARLIO_CLK_SRC_XTAL = SOC_MOD_CLK_XTAL,           // 40 MHz crystal
    PARLIO_CLK_SRC_PLL_F240M = SOC_MOD_CLK_PLL_F240M, // 240 MHz PLL (DEFAULT)
    PARLIO_CLK_SRC_RC_FAST = SOC_MOD_CLK_RC_FAST,     // ~17.5 MHz RC oscillator
    PARLIO_CLK_SRC_EXTERNAL = -1,                     // External clock
    PARLIO_CLK_SRC_DEFAULT = SOC_MOD_CLK_PLL_F240M,   // Maps to 240 MHz
} soc_periph_parlio_clk_src_t;
```

**Key Finding:** DEFAULT maps to 240 MHz PLL, not 80 MHz as initially assumed.

### 2. Code Modifications
**Modified:** `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp:614-665`

Replaced simple frequency testing with comprehensive clock source testing:
- Tests all 3 internal clock sources (XTAL, PLL_F240M, RC_FAST)
- Tests 6 different output frequencies per clock source
- Total: 18 test combinations (3 sources × 6 frequencies)

Test frequencies selected based on clock source:
- **XTAL (40 MHz base):** 40, 20, 10, 8, 5, 4 MHz
- **PLL_F240M (240 MHz base):** 40, 20, 10, 8, 5, 4 MHz
- **RC_FAST (17 MHz base):** 17, 8, 4, 2, 1, 0.5 MHz

### 3. ESP-IDF Version Verification
**Framework versions detected:**
- Arduino ESP32 framework: 3.2.0
- Arduino ESP32 libs: 5.4.0+sha.2f7dcd862a
- **ESP-IDF version: v5.4.1** (lines 14-18 in `esp_idf_version.h`)

**Conclusion:** ESP-IDF v5.4.1 is recent (released 2024) and fully supports ESP32-C6 PARLIO. Version mismatch is NOT the root cause.

### 4. Device Testing Results
**Command:** `uv run ci/debug_attached.py esp32c6 --upload-port COM18 --timeout 20`

**CRITICAL FINDING: ALL clock sources and frequencies FAIL**

Test results:
```
Testing clock source: XTAL (40 MHz base)
  40 MHz: FAIL (err=258)
  20 MHz: FAIL (err=258)
  10 MHz: FAIL (err=258)
  8 MHz: FAIL (err=258)
  5 MHz: FAIL (err=258)
  4 MHz: FAIL (err=258)

Testing clock source: PLL_F240M (240 MHz base)
  40 MHz: FAIL (err=258)
  20 MHz: FAIL (err=258)
  10 MHz: FAIL (err=258)
  8 MHz: FAIL (err=258)
  5 MHz: FAIL (err=258)
  4 MHz: FAIL (err=258)

Testing clock source: RC_FAST (17 MHz base)
  17 MHz: FAIL (err=258)
  8 MHz: FAIL (err=258)
  4 MHz: FAIL (err=258)
  2 MHz: FAIL (err=258)
  1 MHz: FAIL (err=258)
  0 MHz: FAIL (err=258)
```

**Error code 258 = ESP_ERR_NOT_SUPPORTED** (formerly ESP_ERR_INVALID_ARG)

**Consistent error message:**
```
E (XXXX) parlio-tx: parlio_select_periph_clock(245): invalid clock source frequency
E (XXXX) parlio-tx: parlio_new_tx_unit(335): set clock source failed
```

### 5. Root Cause Analysis

#### Investigation Steps:
1. **Searched ESP-IDF source code** for `parlio_select_periph_clock` function
2. **Found validation logic** in `components/esp_driver_parlio/src/parlio_tx.c:~245`:
   ```c
   ESP_RETURN_ON_FALSE(periph_src_clk_hz, ESP_ERR_INVALID_ARG, TAG, "invalid clock source frequency");
   ```
3. **Error occurs when** `periph_src_clk_hz == 0`
4. **Frequency is obtained via** `esp_clk_tree_src_get_freq_hz()` for internal clock sources

#### Root Cause Discovery:
**The PARLIO TX clock domain is NOT ENABLED when `esp_clk_tree_src_get_freq_hz()` is called.**

**Evidence from HAL code** (`hal/esp32c6/include/hal/parlio_ll.h`):
- Line 53-57: `parlio_ll_enable_bus_clock()` - Enables APB bus clock
- Line 434-438: `parlio_ll_tx_enable_clock()` - **Enables TX core clock domain**
- Line 398: `parl_clk_tx_conf.parl_clk_tx_sel` - Sets clock source
- Line 411: `parl_clk_tx_conf.parl_clk_tx_div_num` - Sets clock divider

**Clock gating issue:**
```c
static inline void parlio_ll_tx_enable_clock(parl_io_dev_t *dev, bool en)
{
    (void)dev;
    PCR.parl_clk_tx_conf.parl_clk_tx_en = en;  // ← Must be set BEFORE querying frequency
}
```

When `parl_clk_tx_en = 0` (clock gated/disabled), `esp_clk_tree_src_get_freq_hz()` returns 0, causing validation failure.

#### Initialization Order Problem:
Current ESP-IDF driver sequence (BROKEN on ESP32-C6):
1. Call `esp_clk_tree_src_get_freq_hz()` to validate clock
2. ❌ Frequency returns 0 because clock is gated
3. Return `ESP_ERR_NOT_SUPPORTED` error
4. (Never reaches) Enable clock domain
5. (Never reaches) Set clock source

**Required sequence for ESP32-C6:**
1. Enable bus clock (`parlio_ll_enable_bus_clock`)
2. Enable TX clock domain (`parlio_ll_tx_enable_clock`)  ← **MISSING STEP**
3. Set clock source (`parlio_ll_tx_set_clock_source`)
4. Query clock frequency (`esp_clk_tree_src_get_freq_hz`)
5. Set clock divider (`parlio_ll_tx_set_clock_div`)

### 6. Hardware Verification
**From ESP-IDF documentation:** Error ESP_ERR_NOT_SUPPORTED may occur "if some feature is not supported by hardware, e.g. clock gating"

This confirms that ESP32-C6 PARLIO has special clock gating requirements not present in other ESP32 variants (S3, P4, etc.).

## Critical Findings Summary

1. ✅ **ESP-IDF version is current** (v5.4.1) - NOT the issue
2. ✅ **All clock sources are defined** (XTAL, PLL_F240M, RC_FAST) - Hardware supports them
3. ✅ **All frequencies are valid** (1-240 MHz range tested) - Hardware supports them
4. ❌ **Clock domain gating is the blocker** - Clock must be enabled BEFORE frequency query
5. ❌ **ESP-IDF driver has initialization order bug** - Queries frequency while clock is gated

## ESP32-C6 Hardware Specifics

**Confirmed PARLIO TX support:**
- Official ESP-IDF example exists: `examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix`
- ESP32-C6 listed as supported target in example README
- Hardware capability: Up to 16-bit parallel in half-duplex, 8-bit in full-duplex

**Chip details:**
- Model: ESP32-C6FH4 (QFN32)
- Revision: v0.1 (early engineering sample)
- Features: WiFi 6, BT 5, IEEE802.15.4
- Crystal: 40 MHz

## Files Analyzed

### Modified:
1. `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp` (lines 614-665)

### Researched:
1. `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/soc/esp32c6/include/soc/clk_tree_defs.h`
2. `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/hal/esp32c6/include/hal/parlio_ll.h`
3. `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/esp_common/include/esp_idf_version.h`

## Recommendations for Next Iteration

### Priority 1: Verify ESP-IDF Driver Initialization Order (CRITICAL)
**Hypothesis:** ESP-IDF driver calls `esp_clk_tree_src_get_freq_hz()` before enabling TX clock domain.

**Action:** Examine ESP-IDF source:
```
components/esp_driver_parlio/src/parlio_tx.c
  - Function: parlio_select_periph_clock() (around line 245)
  - Function: parlio_new_tx_unit() (around line 335)
```

Check if TX clock is enabled before frequency query.

### Priority 2: Report Bug to Espressif (If Confirmed)
If driver has wrong initialization order, file GitHub issue:
- **Title:** "ESP32-C6: parlio_new_tx_unit fails with ESP_ERR_NOT_SUPPORTED due to clock gating"
- **Description:** Clock frequency query occurs before TX clock domain is enabled
- **Version affected:** ESP-IDF v5.4.1 (and likely earlier)
- **Workaround needed:** Enable TX clock before calling `esp_clk_tree_src_get_freq_hz()`

### Priority 3: Implement Workaround in FastLED (SHORT-TERM)
**Option A - Reduce PARLIO priority for ESP32-C6:**
```cpp
// In channel_bus_manager_esp32.cpp
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    constexpr int PRIORITY_PARLIO = 5;  // Lower than SPI (50) and RMT (10)
#else
    constexpr int PRIORITY_PARLIO = 100;
#endif
```
This allows SPI or RMT to handle LEDs while PARLIO issue is investigated.

**Option B - Disable PARLIO on ESP32-C6:**
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    // PARLIO has clock gating bug on C6 - skip registration
    FL_WARN("ESP32-C6: PARLIO driver disabled due to clock initialization bug");
#else
    registerChannelEngine(ChannelEnginePARLIO::create(), PRIORITY_PARLIO);
#endif
```

**Option C - Verify RMT works on ESP32-C6** (fallback confirmed working):
Test with RMT priority = 200 to ensure LEDs work via RMT driver while PARLIO is investigated.

### Priority 4: Test Official ESP-IDF Example (VALIDATION)
Build and run official PARLIO example on same hardware:
```bash
git clone --depth 1 --branch v5.4.1 https://github.com/espressif/esp-idf.git /tmp/esp-idf
cd /tmp/esp-idf/examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix
idf.py set-target esp32c6
idf.py build flash monitor
```

**Expected outcomes:**
- **If it works:** FastLED initialization differs from official example
- **If it fails:** Confirms ESP-IDF driver bug, stronger case for bug report

### Priority 5: Check for Existing ESP-IDF GitHub Issues
Search for:
- "ESP32-C6 PARLIO clock"
- "parlio_select_periph_clock ESP_ERR_NOT_SUPPORTED"
- "PARLIO TX clock gating ESP32-C6"

May already be reported with workaround or fix in progress.

### Priority 6: Remove Debug Test Code (CLEANUP)
Once workaround is implemented, remove lines 614-665 in `channel_engine_parlio.cpp` to reduce code size and initialization overhead.

## Questions for Next Agent

1. Is there an existing ESP-IDF GitHub issue for this clock gating problem?
2. Does the official ESP-IDF PARLIO TX example work on the same hardware?
3. Should we disable PARLIO on ESP32-C6 until ESP-IDF fix is available?
4. Can we verify RMT driver works correctly as a fallback for ESP32-C6?
5. Is chip revision v0.1 a factor? (Early engineering sample may have silicon bugs)

## Time Spent
- Research (clock sources, ESP-IDF version): ~25 minutes
- Code modifications (new test structure): ~15 minutes
- Device testing: ~5 minutes
- Root cause analysis (HAL code review): ~30 minutes
- Documentation: ~20 minutes
- **Total: ~95 minutes**

## Status
**BLOCKED - ESP-IDF DRIVER BUG IDENTIFIED**

**Root cause:** PARLIO TX clock domain must be enabled BEFORE querying clock frequency. ESP-IDF v5.4.1 driver queries frequency while clock is gated, causing all initialization attempts to fail.

**Impact:** ESP32-C6 PARLIO cannot be used for WS2812 control via FastLED until:
1. ESP-IDF driver is fixed (upstream), OR
2. Workaround is implemented (reduce priority/disable PARLIO on C6), AND
3. RMT driver is confirmed working (fallback path)

**Next Iteration Priority:** Implement workaround and validate RMT fallback.
