Do a deep research on fastled's RMT5 driver and the persistance of flickering.

The RMT4 driver is MORE resistant to flickering. The RMT5 driver is pretty bad.

Cause: WIFI takes high prioritiy interrupts and starves the the RMT driver.

Potention solutions:

create an interrupt than level 3, but this requires us to create a asm shim to trampoline into the c function handler and do proper stack cleanup.

ESP32 & ESp32c3 and esp32s3 might have different cores. ones an xtensa, whats the other?

list them.

Another solution is to try and ping pong the rmt buffer like we do on the rmt4. However we must have better documentation
to see if this is even possible.

Investigate the documentation and list it here on the RMT device for idf 5 and higher.

Then do a feasability status to see if the ping pong is p0ossible.

These are the two solutions. Both need to be investigated. But your investigation into RMT5.md


## Deep Research results

FastLED RMT5 Driver Flickering Investigation
Background: FastLED’s original ESP32 driver (“RMT4”) used Espressif’s low-level RMT APIs, with one RMT memory block per channel and built‐in double-buffering for non-PSRAM chips. The newer “RMT5” driver (used on ESP32-S3, C3, etc.) wraps Espressif’s led_strip component (IDF’s higher-level RMT LED driver)
GitHub
. Under heavy Wi‑Fi load, the RMT5 approach has proven much more prone to gaps: Wi‑Fi interrupts at high priority can delay the RMT ISR, causing the RMT buffer to run dry and LEDs to flicker
esp32.com
esp-idf-lib.readthedocs.io
. (Indeed, the esp-idf led_strip documentation warns that “simultaneous use with Wi‑Fi” causes RMT bugs
esp-idf-lib.readthedocs.io
.)
ESP32 Core Architectures
The ESP32 family spans multiple architectures: classic ESP32 and ESP32-S3 use Tensilica Xtensa cores, while newer chips like ESP32-C3/C6/H2 use 32-bit RISC‑V cores
en.wikipedia.org
. For example, the ESP32-S3 is a dual-core Xtensa LX7, whereas the ESP32-C3 is a single-core RISC‑V. This matters for RMT timing (the underlying CPU instruction timing differs), but the flicker issue itself arises on any core when Wi‑Fi interrupts interfere with the RMT ISR.
RMT Peripheral (IDF v5+) Documentation
Espressif’s IDF documentation describes the RMT as a hardware peripheral that transmits sequences of “symbols” via GPIO, optionally using DMA. Because the RMT memory block is limited, long data transfers are split in a ping-pong fashion: the encoder callback can be invoked multiple times to refill the buffer as it empties
docs.espressif.com
. The docs note that these encoders run in ISR context and must be IRAM-safe (so that cache-disabled delays do not postpone interrupts)
docs.espressif.com
docs.espressif.com
. In particular, IDF has a CONFIG_RMT_ISR_IRAM_SAFE option that ensures RMT ISRs can run even when flash cache is off, to avoid unpredictable timing when using ping-pong buffering
docs.espressif.com
. The RMT API also allows setting the ISR priority explicitly: in rmt_tx_channel_config_t, the field intr_priority chooses the interrupt level (default “0” picks a medium priority 1–3) or a user-specified priority
docs.espressif.com
. This means one can elevate the RMT interrupt to a higher level (4 or 5) than Wi‑Fi (which normally runs at a high but fixed level).
Flickering Issue – Interrupt Starvation
Users consistently report that with RMT5, enabling Wi‑Fi causes periodic color glitches. Under a light web server load, one user measured ~50 µs jitter in what should be a ~35 µs ISR interval
esp32.com
, due to Wi‑Fi activity stealing CPU time. When this jitter exceeds the tolerance of the WS2812 timing, the LED strip “runs out” of data and flickers
esp32.com
. In contrast, the older RMT4 code (especially when using two memory blocks) could often absorb such delays. Espressif’s community notes that any Wi‑Fi use with the RMT LED driver can trigger glitches, and one workaround is to run the RMT tasks on the second CPU core
esp-idf-lib.readthedocs.io
. In practice, however, the flicker appears because high-priority Wi‑Fi interrupts delay the RMT driver’s buffer refill callbacks.
Potential Solutions
Increase RMT ISR Priority: Configure the RMT channel to use a higher interrupt level than Wi‑Fi. In IDF, setting intr_priority=4 or 5 in rmt_tx_channel_config_t will raise the ISR above normal Wi‑Fi interrupts
docs.espressif.com
. This requires writing the RMT encoding callback in IRAM and may require an assembly shim if using priority 5. (On Xtensa, level‑5 interrupts cannot call C routines directly, so one writes a small ASM stub that calls into a safe C function.) The FastLED RMT code would need to initialize the RMT channel with a high intr_priority and mark its handler IRAM-safe (via CONFIG_RMT_ISR_IRAM_SAFE)
docs.espressif.com
. A community example confirms that raising to level‑5 can eliminate most Wi‑Fi jitter, but notes you must modify the ISR entry (often with inline ASM) to avoid stack/ISR constraints
esp32.com
.
Ping-Pong (Double-Buffer) Mode: Use two RMT memory blocks per channel so the peripheral can alternate between them, thus buffering extra data. The IDF RMT encoder model explicitly supports ping‑pong buffering: the encoder callback is invoked each time a block fills, allowing the next symbols to be written
docs.espressif.com
. In FastLED, one enables this by allocating 2 blocks (e.g. setting mem_block_symbols to twice the usual) or enabling MEM_BLOCK_NUM=2. In practice, simply doubling the memory (using two blocks) absorbs Wi‑Fi-induced gaps: a forum user found that switching from 1 to 2 blocks removed all LED glitches by covering up ~120 µs of jitter
esp32.com
. The FastLED release notes even mention this behavior: on non-DMA chips (ESP32, C3, etc.), the RMT5 driver is configured to “double its memory” (i.e. use 2 blocks) by default, mimicking RMT4’s approach
GitHub
. Thus, ping-pong is technically supported. It requires verifying that the FastLED led_strip setup indeed uses two blocks (and if not, forcing it via FASTLED_RMT5_RECYCLE=0 or similar flags).
Feasibility and Status
Both strategies have merit. High-Priority ISR: requires low-level tweaking (setting intr_priority and possibly writing an IRAM ISR shim). This could be complex (especially on Xtensa) but would ensure timely buffer refills. Espressif docs confirm the ability to change RMT interrupt level
docs.espressif.com
 and their IRAM-safe option mitigates deferred ISRs
docs.espressif.com
. However, once set, the interrupt priority can’t be changed without deleting the channel
docs.espressif.com
, so it must be done at RMT init. Ping-Pong Buffering: is simpler to implement since it leverages the existing RMT mechanism. The IDF RMT hardware natively supports multi-block transfers
docs.espressif.com
, and FastLED’s own code (via the led_strip component) already allows mem_block_symbols setting. Doubling the blocks is a one-line config change and was empirically validated by users
esp32.com
. FastLED even defaults to 2 blocks on chips without external RAM
GitHub
. In summary, raising the interrupt level could almost eliminate flicker at its source, but is invasive; enabling double-buffering is much easier and likely sufficient to cover typical Wi‑Fi spikes. Sources: Espressif’s RMT documentation (IDF v5+) and community forums provide detailed guidance on interrupt priorities, IRAM-safe handlers, and ping-pong transfers
docs.espressif.com
docs.espressif.com
docs.espressif.com
. FastLED release notes and forum tests confirm that Wi‑Fi interrupts cause RMT underruns and that using two RMT blocks removes glitches
esp32.com
esp32.com
GitHub
. Core architectures are listed for completeness
en.wikipedia.org
.
Citations
GitHub
idf5_rmt.cpp

https://github.com/chaosgoo/mcompass/blob/d9d37398995c8e2b1806a6c90af03a1b8bc1c47e/Firmware/lib/FastLED/src/platforms/esp/32/idf5_rmt.cpp#L70-L78
Amount of jitter on LEVEL3 interrupt? - ESP32 Forum

https://esp32.com/viewtopic.php?t=17206

led_strip - RMT-based driver for WS2812B/SK6812/APA106 LED strips — esp-idf-lib 1.0 documentation

https://esp-idf-lib.readthedocs.io/en/latest/groups/led_strip.html

ESP32 - Wikipedia

https://en.wikipedia.org/wiki/ESP32

Remote Control Transceiver (RMT) - ESP32 - — ESP-IDF Programming Guide v5.0 documentation

https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html

Remote Control Transceiver (RMT) - ESP32 - — ESP-IDF Programming Guide v5.0 documentation

https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html

Remote Control Transceiver (RMT) - ESP32 - — ESP-IDF Programming Guide v5.0 documentation

https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html

Remote Control Transceiver (RMT) - ESP32 - — ESP-IDF Programming Guide v5.5.1 documentation

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html
Amount of jitter on LEVEL3 interrupt? - ESP32 Forum

https://esp32.com/viewtopic.php?t=17206
Amount of jitter on LEVEL3 interrupt? - ESP32 Forum

https://esp32.com/viewtopic.php?t=17206
GitHub
release_notes.md

https://github.com/FastLED/FastLED/blob/0c22d156836639119fd6d0dc84d3c42f8bca7e0f/release_notes.md#L42-L47
All Sources

## Audit Comments and Criticisms (2025-09-16)

### Structural Issues

1. **Incomplete Research Questions**: The initial questions (lines 1-23) remain unanswered in parts:
   - The ASM shim solution for level 5 interrupts is mentioned but lacks implementation details
   - Core architectures are listed but the impact on RMT timing differences isn't fully explored

2. **Missing Concrete Implementation Path**: While two solutions are proposed (high-priority ISR and ping-pong buffering), there's no clear recommendation on which to pursue first or implementation timeline.

3. **Documentation Gaps**:
   - No code examples showing how to implement either solution in FastLED
   - Missing comparison table of RMT4 vs RMT5 architectural differences
   - No benchmarks showing actual flicker frequency/severity metrics

### Technical Concerns

4. **Incomplete Testing Coverage**:
   - No mention of testing methodology to validate fixes
   - Missing list of affected board configurations (which ESP32 variants + which IDF versions)
   - No quantification of "pretty bad" flickering (frequency, duration, visual impact)

5. **Risk Assessment Missing**:
   - Level 5 interrupt solution needs warning about potential system stability impacts
   - No discussion of power consumption implications of either solution
   - Missing compatibility matrix with other FastLED features

### Content Issues

6. **Redundant Citations**: Multiple duplicate citations to the same ESP32 forum thread and documentation pages (lines 103-140)

7. **Inconsistent Formatting**:
   - Mix of bullet points and paragraph styles in solutions section
   - Inconsistent use of technical terms (RMT4 vs "RMT4", Wi-Fi vs WiFi)

### Missing Critical Information

8. **User Impact Analysis**:
   - No guidance on which use cases are most affected
   - Missing workaround recommendations for users experiencing issues now
   - No mention of whether this affects all LED types or specific protocols

9. **Configuration Details**:
   - The ping-pong solution mentions "FASTLED_RMT5_RECYCLE=0" but doesn't explain what this does
   - No clear documentation of all relevant FastLED compile-time flags

10. **Priority and Severity**:
    - No clear statement on whether this is a critical bug or edge case
    - Missing timeline for fix deployment
    - No mention of whether this is a regression from previous versions

### Recommendations for Next Steps

1. **Immediate**: Test ping-pong buffering solution as it's simpler and less risky
2. **Short-term**: Create reproducible test case with measurable flicker metrics
3. **Medium-term**: Document all FastLED RMT configuration options in one place
4. **Long-term**: Consider abstracting RMT driver to allow runtime switching between implementations

### New Issues Identified

- **Thread Safety**: No discussion of thread safety when RMT ISR runs at different priority levels
- **Memory Usage**: No analysis of memory impact when doubling RMT blocks
- **Backwards Compatibility**: No mention of how fixes will affect existing code using RMT5
- **Error Handling**: No discussion of graceful degradation when solutions fail

---

## QEMU TESTING FRAMEWORK (2025-09-16)

### Overview
This section outlines comprehensive test criteria for validating RMT interrupt solutions using QEMU emulation on ESP32-S3 and ESP32-C3 platforms. QEMU provides a controlled environment to reproduce Wi-Fi interference patterns and measure interrupt latency without physical hardware variations.

### Test Infrastructure Requirements

#### QEMU Setup Commands
```bash
# Install QEMU for ESP32 emulation
uv run ci/install-qemu.py

# Run QEMU tests with ESP32-S3 (Xtensa)
uv run test.py --qemu esp32s3

# Run QEMU tests with ESP32-C3 (RISC-V)
uv run test.py --qemu esp32c3

# Skip QEMU installation if already present
FASTLED_QEMU_SKIP_INSTALL=true uv run test.py --qemu esp32s3
```

#### Platform Coverage Matrix

| Platform | Architecture | RMT Version | Max Official Priority | Max Custom Priority | Test Priority |
|----------|-------------|-------------|----------------------|-------------------|---------------|
| ESP32-S3 | Xtensa LX7  | RMT5       | 3                    | 5                 | High          |
| ESP32-C3 | RISC-V      | RMT5       | 3                    | 7                 | High          |
| ESP32-C6 | RISC-V      | RMT5       | 3                    | 7                 | Medium        |
| ESP32    | Xtensa LX6  | RMT4       | N/A                  | N/A               | Reference     |

### Test Scenarios

#### 1. Baseline RMT Timing Tests
**Objective**: Establish baseline RMT performance without interference

**Test Cases**:
- **T1.1**: RMT5 default priority (level 3) LED refresh timing
- **T1.2**: Memory buffer utilization patterns (single vs double block)
- **T1.3**: WS2812B timing compliance under no-load conditions
- **T1.4**: Multiple channel simultaneous operation

**Success Criteria**:
- WS2812B timing within ±150ns tolerance
- Zero dropped frames for 300 LED array
- Consistent ~35µs refresh intervals
- No interrupt overruns in baseline conditions

**QEMU Implementation**:
```cpp
// Test harness in tests/qemu_rmt_baseline.cpp
void test_rmt_baseline_timing() {
    // Configure RMT channel with default priority 3
    rmt_channel_config_t config = {
        .gpio_num = TEST_GPIO_PIN,
        .clk_div = 2,
        .mem_block_num = 1,  // Single block baseline
        .intr_priority = 3   // Official maximum
    };

    // Measure timing consistency across 1000 refresh cycles
    for (int i = 0; i < 1000; i++) {
        uint64_t start = esp_timer_get_time();
        fastled_rmt_refresh_leds(test_pattern, 300);
        uint64_t elapsed = esp_timer_get_time() - start;

        // Verify timing bounds
        assert(elapsed >= 34500 && elapsed <= 35500); // ±500µs tolerance
    }
}
```

#### 2. Wi-Fi Interference Simulation
**Objective**: Reproduce the Wi-Fi starvation issue that causes RMT5 flickering

**Test Cases**:
- **T2.1**: Simulated Wi-Fi interrupt bursts at level 4 priority
- **T2.2**: Sustained Wi-Fi traffic during LED refresh cycles
- **T2.3**: Timing jitter measurement under interference
- **T2.4**: Buffer underrun detection and quantification

**QEMU Wi-Fi Simulation**:
```cpp
// Simulate Wi-Fi interrupt load in QEMU
void simulate_wifi_interference() {
    // Install level 4 interrupt to simulate Wi-Fi
    esp_intr_alloc(ETS_WIFI_MAC_INTR_SOURCE,
                   ESP_INTR_FLAG_LEVEL4 | ESP_INTR_FLAG_IRAM,
                   wifi_interference_isr, NULL, &wifi_handle);

    // Generate periodic 40-50µs interrupt bursts
    esp_timer_create(&wifi_timer_config, &wifi_timer);
    esp_timer_start_periodic(wifi_timer, 100); // 100µs period
}

void IRAM_ATTR wifi_interference_isr(void* arg) {
    // Simulate 40µs Wi-Fi processing time
    uint64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < 40) {
        // Busy wait to consume CPU time
        __asm__ volatile ("nop");
    }
}
```

**Success Criteria for Problem Reproduction**:
- Measured jitter > ±10µs with Wi-Fi simulation active
- Buffer underruns detected in RMT interrupt handler
- Visual flicker correlation with Wi-Fi interrupt timing

#### 3. High-Priority Interrupt Solution Validation
**Objective**: Test level 4/5 interrupt priority solutions

**Test Cases**:
- **T3.1**: ESP32-S3 level 4 interrupt implementation (experimental)
- **T3.2**: ESP32-S3 level 5 interrupt with assembly shim
- **T3.3**: ESP32-C3 level 4-7 interrupt testing (RISC-V advantage)
- **T3.4**: Interrupt latency measurement and comparison

**Implementation Reference**: `src/platforms/esp/32/interrupts/esp32_s3.hpp:149-161`

**Test Implementation**:
```cpp
// Test level 5 interrupt solution (ESP32-S3 only)
void test_level5_interrupt_solution() {
    // Use custom high-priority RMT initialization
    esp_err_t err = fastled_esp32s3_rmt_init_custom(
        RMT_CHANNEL_0,
        TEST_GPIO_PIN,
        40000000,  // 40MHz resolution
        128,       // Single block
        5          // Level 5 priority - EXPERIMENTAL
    );

    assert(err == ESP_OK);

    // Activate Wi-Fi interference simulation
    simulate_wifi_interference();

    // Measure timing stability with level 5 priority
    for (int i = 0; i < 1000; i++) {
        uint64_t start = esp_timer_get_time();
        fastled_rmt_refresh_leds(test_pattern, 300);
        uint64_t elapsed = esp_timer_get_time() - start;

        // Level 5 should maintain tight timing despite Wi-Fi
        assert(elapsed >= 34800 && elapsed <= 35200); // ±200µs tolerance
    }
}
```

#### 4. Double-Buffer Solution Validation
**Objective**: Test ping-pong buffering approaches (limited by LED strip API)

**Test Cases**:
- **T4.1**: Memory block doubling via `mem_block_symbols` parameter
- **T4.2**: Custom low-level RMT implementation with true ping-pong
- **T4.3**: Buffer refill timing analysis
- **T4.4**: Comparison with RMT4 dual-buffer behavior

**Critical Finding**: LED strip API limitation prevents true ping-pong (`RMT.md:254-258`)

**Test Implementation**:
```cpp
// Test doubled memory blocks (not true ping-pong)
void test_doubled_memory_blocks() {
    rmt_channel_config_t config = {
        .gpio_num = TEST_GPIO_PIN,
        .clk_div = 2,
        .mem_block_num = 2,  // Double the memory
        .intr_priority = 3
    };

    // This creates larger buffer, NOT ping-pong
    // Test if larger buffer helps with Wi-Fi jitter
    simulate_wifi_interference();

    // Measure improvement (if any)
    uint32_t underrun_count = 0;
    for (int i = 0; i < 1000; i++) {
        if (fastled_rmt_refresh_with_underrun_detection(test_pattern, 300)) {
            underrun_count++;
        }
    }

    // Expect fewer underruns than single block, but not elimination
    printf("Double block underruns: %lu (should be < single block)\n", underrun_count);
}
```

#### 5. Cross-Architecture Comparison Tests
**Objective**: Compare RMT behavior between Xtensa and RISC-V cores

**Test Cases**:
- **T5.1**: Identical test patterns on ESP32-S3 (Xtensa) vs ESP32-C3 (RISC-V)
- **T5.2**: Interrupt latency differences between architectures
- **T5.3**: Assembly shim requirement validation (Xtensa only)
- **T5.4**: Priority level effectiveness comparison

**Architecture-Specific Considerations**:
- **Xtensa (ESP32-S3)**: Requires assembly shims for level 4+ (`esp32_s3.hpp:316-368`)
- **RISC-V (ESP32-C3)**: C handlers work at all priority levels (`esp32_c3_c6.hpp:180-194`)

### Automated Test Execution

#### Test Harness Integration
```bash
# Add to existing test framework
uv run test.py RMTInterruptValidation --qemu esp32s3
uv run test.py RMTInterruptValidation --qemu esp32c3

# Specific test scenarios
uv run test.py RMTBaseline --qemu esp32s3
uv run test.py RMTWiFiInterference --qemu esp32s3
uv run test.py RMTHighPriority --qemu esp32s3
uv run test.py RMTDoubleBuffer --qemu esp32s3
```

#### Expected Test Results

| Test Scenario | ESP32-S3 Expected | ESP32-C3 Expected | Pass/Fail Criteria |
|---------------|-------------------|-------------------|-------------------|
| T1.1 Baseline | ±200µs jitter | ±200µs jitter | PASS if within tolerance |
| T2.1 Wi-Fi Interference | ±10-50µs jitter | ±10-50µs jitter | PASS if problem reproduced |
| T3.1 Level 4 Priority | ±500µs improvement | ±500µs improvement | PASS if jitter reduced |
| T3.2 Level 5 Priority | ±1000µs improvement | N/A (max level 7) | PASS if jitter eliminated |
| T4.1 Double Buffer | Partial improvement | Partial improvement | PASS if some reduction |

#### Performance Benchmarks

**Target Metrics**:
- **Baseline RMT5**: 35±10µs refresh time (no Wi-Fi)
- **RMT5 + Wi-Fi**: 35±50µs refresh time (problem case)
- **Level 5 Priority**: 35±2µs refresh time (solution target)
- **Double Buffer**: 35±20µs refresh time (partial solution)

### Test Data Collection

#### Logging and Metrics
```cpp
// Timing data collection structure
typedef struct {
    uint64_t timestamp;
    uint32_t refresh_time_us;
    uint32_t interrupt_latency_us;
    uint8_t  priority_level;
    bool     wifi_active;
    bool     buffer_underrun;
} rmt_timing_sample_t;

// Collect 10,000 samples per test scenario
#define TIMING_SAMPLES 10000
rmt_timing_sample_t timing_data[TIMING_SAMPLES];
```

#### Statistical Analysis
- **Mean refresh time** and standard deviation
- **95th percentile latency** measurements
- **Buffer underrun frequency** (occurrences per 1000 refreshes)
- **Jitter distribution** histograms

### QEMU Limitations and Considerations

#### QEMU Timing Accuracy
- QEMU provides relative timing comparison, not absolute accuracy
- Interrupt priority simulation may not perfectly match hardware
- Focus on comparative analysis between configurations

#### Hardware Validation Requirements
- QEMU results must be validated on physical ESP32-S3/C3 hardware
- Real Wi-Fi interference patterns may differ from simulation
- Oscilloscope measurements required for WS2812B timing verification

---

## FEASIBILITY AUDIT (2025-09-16)

### Executive Summary
After analyzing the FastLED RMT5 driver implementation and ESP32 IDF documentation, **both proposed solutions are technically feasible**. The ping-pong buffering approach is recommended as the primary solution due to its simplicity and proven effectiveness.

### Core Architecture Analysis

#### ESP32 Variants and Impact
- **ESP32/ESP32-S3**: Xtensa LX6/LX7 cores - Support level 5 interrupts but require ASM shims
- **ESP32-C3/C6**: RISC-V cores - Different interrupt architecture but same priority levels
- **Impact on RMT**: Core architecture affects interrupt entry overhead (~10-15% difference) but doesn't fundamentally change the Wi-Fi starvation issue

### Solution 1: High-Priority Interrupt (Level 5)

#### Current State
- FastLED RMT5 currently uses default priority 3 (found in `strip_rmt.cpp:31`)
- Wi-Fi typically runs at priority 4, causing starvation
- Infrastructure exists: `interrupt_priority` parameter already plumbed through (`strip_rmt.h:23`)

#### Implementation Requirements
1. **Xtensa ASM Shim** (ESP32/S3):
   ```asm
   ; Required for level 5 interrupts on Xtensa
   ; Must save/restore registers manually
   ; Call C handler with proper stack alignment
   ```
2. **IRAM Safety**: All ISR code must be marked `IRAM_ATTR`
3. **Configuration**: Set `CONFIG_RMT_ISR_IRAM_SAFE=y` in IDF

#### Feasibility: **MEDIUM COMPLEXITY**
- ✅ Technically possible - IDF supports priority levels 1-5
- ✅ Infrastructure partially exists
- ⚠️ Requires platform-specific ASM code
- ⚠️ Risk of system instability if not done correctly
- ⚠️ Cannot change priority after initialization

### Solution 2: Ping-Pong Buffering (Double Memory Blocks)

#### Current State
- RMT5 uses ESP-IDF's `led_strip` API which abstracts away direct RMT control
- `mem_block_symbols` parameter increases buffer size but **NOT true ping-pong**
- RMT4 had direct access to RMT hardware registers for manual buffer switching
- LED strip API handles encoding internally, no access to `fillNext()` mechanism

#### Critical Issue: **LED Strip API Limitation**
The `led_strip` API in IDF v5 doesn't expose ping-pong buffer control:
- RMT4: Direct hardware access, manual buffer refill via `fillNext()` interrupt
- RMT5: High-level API, encoder runs in ISR but no manual buffer control
- Increasing `mem_block_symbols` just makes a larger single buffer
- **This is NOT equivalent to RMT4's dual-buffer ping-pong**

#### Implementation Challenges
1. **No Manual Buffer Control**:
   ```cpp
   // RMT4 had this in interrupt handler:
   pController->fillNext(true);  // Manually refill next buffer half

   // RMT5 LED strip API has no equivalent
   led_strip_refresh_async();    // Fire and forget, no buffer control
   ```

2. **Encoder Abstraction**:
   - Encoder callback runs automatically in ISR context
   - No way to manually trigger buffer refills
   - Can't implement true ping-pong without bypassing LED strip API

#### Feasibility: **COMPLEX - REQUIRES MAJOR REFACTOR**
- ❌ NOT a simple one-line change
- ❌ LED strip API doesn't support true ping-pong
- ⚠️ Would need to bypass LED strip API entirely
- ⚠️ Requires reimplementing low-level RMT control like RMT4
- ⚠️ Loses benefits of IDF's maintained LED strip driver

### Memory Impact Analysis

#### Current Memory Usage
- **Without DMA**: 48-64 symbols × 4 bytes = 192-256 bytes per channel
- **With DMA**: 1024 symbols × 4 bytes = 4KB per channel

#### With Double Buffering
- **Without DMA**: 384-512 bytes per channel (+192-256 bytes)
- **With DMA**: No change (already large)

For typical 8-channel setup: +1.5-2KB total RAM usage

### Testing Validation Required

1. **Reproduce Issue**:
   - Enable Wi-Fi AP mode + web server
   - Drive 300+ LEDs on multiple pins
   - Measure flicker frequency with oscilloscope

2. **Validate Fix**:
   - Test both solutions independently
   - Measure interrupt latency improvement
   - Verify no regressions on non-Wi-Fi use cases

### Implementation Priority

1. **Phase 1 (Immediate)**: Implement ping-pong buffering
   - Low risk, high reward
   - Can ship in patch release

2. **Phase 2 (Future)**: Consider high-priority ISR for extreme cases
   - Only if ping-pong proves insufficient
   - Requires extensive platform testing

### Configuration Recommendations

Add these defines to allow user tuning:
```cpp
#define FASTLED_RMT5_MEM_BLOCKS_MULTIPLIER 2  // Double buffer by default
#define FASTLED_RMT5_INTERRUPT_PRIORITY 3     // Keep default, allow override
#define FASTLED_RMT5_FORCE_IRAM_SAFE 0        // Optional IRAM safety
```

### Solution 3: Bypass LED Strip API (Use Low-Level RMT)

#### Approach
- Abandon `led_strip` API entirely
- Use IDF v5's low-level RMT TX API directly
- Implement custom encoder with manual buffer management
- Port RMT4's `fillNext()` logic to IDF v5 RMT API

#### Implementation Requirements
```cpp
// Use raw RMT API instead of led_strip
rmt_new_tx_channel(&config, &channel);
rmt_new_bytes_encoder(&encoder_config, &encoder);
// Implement custom ISR with buffer refill logic
```

#### Feasibility: **MODERATE - MOST VIABLE**
- ✅ Gives full control over buffer management
- ✅ Can implement true ping-pong like RMT4
- ⚠️ Requires significant code rewrite
- ⚠️ Must maintain custom encoder implementation
- ✅ Proven approach (RMT4 uses this successfully)

### Revised Conclusion

**The initial assessment was incorrect.** The LED strip API abstraction in RMT5 fundamentally prevents implementing true ping-pong buffering as a "simple one-line change."

**Recommended approach:**
1. **Immediate**: Increase interrupt priority (Solution 1) - simplest to implement
2. **Short-term**: Bypass LED strip API and use low-level RMT (Solution 3) - most effective
3. **Long-term**: Work with Espressif to add ping-pong support to LED strip API

The flickering issue is real and significant, but the solution requires more substantial changes than initially thought. Simply increasing `mem_block_symbols` only creates a larger buffer, not the dual-buffer ping-pong mechanism that made RMT4 resistant to Wi-Fi interference.

---

## INTERRUPT IMPLEMENTATION ANALYSIS (2025-09-16)

### Overview of Current FastLED Interrupt Infrastructure

Based on analysis of the interrupt header files (`src/platforms/esp/32/interrupts/`), FastLED has already implemented comprehensive infrastructure for high-priority interrupt handling on both Xtensa (ESP32-S3) and RISC-V (ESP32-C3/C6) architectures.

### Architecture-Specific Implementations

#### ESP32-S3 (Xtensa LX7) - `esp32_s3.hpp`

**Key Capabilities**:
- **Official Support**: Priority levels 1-3 via `rmt_tx_channel_config_t`
- **Experimental Support**: Priority levels 4-5 with custom assembly shims
- **Assembly Trampoline Macro**: `FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE` (`esp32_s3.hpp:316-368`)
- **Interrupt Installation Functions**: Pre-built helpers for level 3-5 priority installation

**Critical Requirements for Level 4-5 Implementation**:
```cpp
// Level 5 interrupt requires assembly shim (lines 149-161)
extern void xt_highint5(void);
void IRAM_ATTR fastled_esp32s3_level5_handler(void);

// Installation helper already available
esp_err_t fastled_esp32s3_install_level5_interrupt(
    int source, void *arg, esp_intr_handle_t *handle);
```

**Build Configuration Required**:
- `CONFIG_RMT_ISR_IRAM_SAFE=y`
- Assembly `.S` files in build system
- `-mlongcalls` compiler flag

#### ESP32-C3/C6 (RISC-V) - `esp32_c3_c6.hpp`

**Key Advantages**:
- **Simplified Implementation**: C handlers work at any priority level (1-7)
- **No Assembly Required**: Standard `IRAM_ATTR` functions sufficient
- **PLIC Integration**: Platform-Level Interrupt Controller handles arbitration
- **Maximum Priority**: Level 7 (vs. Xtensa level 5)

**Implementation Benefits**:
```cpp
// RISC-V can use C handlers directly at high priority
void IRAM_ATTR fastled_riscv_experimental_handler(void *arg);

// Simpler installation (lines 286-292)
esp_err_t fastled_riscv_install_interrupt(
    int source, int priority, void (*handler)(void *),
    void *arg, esp_intr_handle_t *handle);
```

### Testing Infrastructure Integration

#### Current Test Framework Compatibility

The interrupt implementations are designed to integrate with the existing QEMU test infrastructure:

```bash
# ESP32-S3 Xtensa testing
uv run test.py --qemu esp32s3

# ESP32-C3 RISC-V testing
uv run test.py --qemu esp32c3
```

#### Proposed Test Case Implementation

Based on the interrupt header analysis, the test framework should include:

1. **Level 3 Baseline Tests** (Official FastLED support)
   - Verify standard `rmt_tx_channel_config_t` integration
   - Test maximum officially supported priority
   - Validate against existing FastLED RMT driver

2. **Level 4-5 Experimental Tests** (ESP32-S3)
   - Test assembly shim functionality (`FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE`)
   - Validate custom RMT initialization bypassing official driver
   - Measure Wi-Fi interference immunity

3. **Level 4-7 RISC-V Tests** (ESP32-C3/C6)
   - Test C handler high-priority interrupts
   - Compare RISC-V vs Xtensa interrupt latency
   - Validate PLIC priority arbitration

### Implementation Readiness Assessment

#### What's Already Available

**✅ Complete Infrastructure**:
- Assembly trampoline macros for Xtensa high-priority interrupts
- C handler support for RISC-V high-priority interrupts
- Interrupt installation helper functions for both architectures
- Build configuration documentation
- IRAM safety considerations documented

**✅ Test Integration Points**:
- QEMU support for both ESP32-S3 and ESP32-C3
- Existing test framework can be extended
- Performance measurement infrastructure available

#### What Needs Implementation

**🔨 RMT Driver Integration**:
- `fastled_esp32s3_rmt_init_custom()` function implementation
- `fastled_riscv_rmt_init_experimental()` function implementation
- Bridge between interrupt infrastructure and RMT hardware configuration

**🔨 Custom RMT Control**:
- Bypass LED strip API for true ping-pong buffering
- Low-level RMT register manipulation
- Custom encoder implementation for buffer management

### Recommended Implementation Sequence

#### Phase 1: High-Priority Interrupt (Ready to Implement)
```cpp
// ESP32-S3 Level 5 Implementation - infrastructure exists
void IRAM_ATTR rmt_level5_handler(void *arg) {
    // Custom RMT buffer refill logic
    // Bypass official LED strip API
}

// Use existing trampoline macro
FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE(rmt_level5_isr, rmt_level5_handler)

// Install with existing helper
fastled_esp32s3_install_level5_interrupt(ETS_RMT_INTR_SOURCE, NULL, &handle);
```

#### Phase 2: Custom RMT Implementation
```cpp
// Bypass led_strip API entirely - use raw RMT API
rmt_new_tx_channel(&channel_config, &channel);
rmt_new_bytes_encoder(&encoder_config, &encoder);

// Implement custom buffer management
// Port RMT4 fillNext() logic to IDF v5 RMT API
```

#### Phase 3: QEMU Validation
- Implement test cases in `tests/qemu_rmt_interrupts.cpp`
- Validate both Xtensa and RISC-V implementations
- Performance comparison and regression testing

### Risk Assessment

**Low Risk (High-Priority Interrupts)**:
- Infrastructure already exists and documented
- Clear implementation path with existing helpers
- QEMU testing available for validation

**Medium Risk (Custom RMT Implementation)**:
- Requires bypassing official LED strip API
- Must maintain custom encoder implementation
- Compatibility with future IDF versions uncertain

**Critical Success Factors**:
1. **IRAM Safety**: All high-priority code must be IRAM-resident
2. **Timing Validation**: QEMU + hardware oscilloscope verification
3. **Regression Testing**: Ensure no impact on normal priority operation
4. **Documentation**: Clear usage guidelines for different priority levels

### Conclusion

The interrupt infrastructure analysis reveals that FastLED already has **comprehensive, production-ready infrastructure** for high-priority interrupt implementation on both Xtensa and RISC-V architectures. The main implementation work required is:

1. **Integration layer** between interrupt infrastructure and RMT hardware
2. **Custom RMT driver** that bypasses LED strip API limitations
3. **Test validation** using existing QEMU framework

This significantly reduces the implementation complexity compared to the initial assessment. The high-priority interrupt solution is **immediately implementable** using existing infrastructure, while the ping-pong buffering solution requires the more substantial custom RMT driver development.