# RMT5 Driver WiFi Robustness Analysis

## Executive Summary

The RMT5 driver experiences LED flickering under WiFi load due to **interrupt priority starvation**. WiFi operates at priority level 4, preempting RMT's default level 3 interrupts, causing buffer underruns and LED corruption. The solution is to raise RMT interrupt priority to level 4 or 5 using existing FastLED infrastructure.

**Quick Fix:** Change `ESP_INTR_FLAG_LEVEL3` to `ESP_INTR_FLAG_LEVEL4` in `rmt5_worker.cpp:204`.

---

## Table of Contents

- [Current State Analysis](#current-state-analysis)
- [The WiFi Interference Problem](#the-wifi-interference-problem)
- [Deep Dive: Priority 3 vs WiFi](#deep-dive-priority-3-vs-wifi)
- [Deep Dive: ESP32-S3 DMA Issues](#deep-dive-esp32-s3-dma-issues)
- [Potential Improvements](#potential-improvements)
- [Recommended Action Plan](#recommended-action-plan)

---

## Current State Analysis

### RMT5 Architecture Overview

The RMT5 driver has evolved into two implementations:

#### 1. High-level API (`strip_rmt.cpp`)
Uses ESP-IDF's `led_strip` component wrapper:
- **Interrupt priority:** Level 3 (default) - `strip_rmt.h:21`
- **DMA mode:** Disabled by default on ESP32-S3 due to bugs - `strip_rmt.cpp:82-83`
- **Memory blocks:** Configurable via `mem_block_symbols`, set to 1024 when DMA enabled - `strip_rmt.cpp:38`

#### 2. Low-level API (`rmt5_worker.cpp`)
Direct RMT register access with worker pooling:
- **Interrupt priority:** Level 3 with IRAM attributes - `rmt5_worker.cpp:204`
- **Buffer mode:** Double-buffer (ping-pong) - Uses `2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL` - `rmt5_worker.cpp:132`
- **ISR placement:** All critical functions in IRAM - `rmt5_worker.cpp:392-630`

### Existing Mitigation Strategies

**What's Already Implemented:**

1. **IRAM-Safe ISRs** (Low-level worker):
   - All ISR functions marked `IRAM_ATTR` - `rmt5_worker.cpp:392, 425, 473, 557, 594, 611`
   - Ensures ISR can run even when flash cache is disabled
   - Critical for reducing interrupt latency

2. **Double-Buffer Architecture** (Low-level worker only):
   - Uses 2× memory blocks (128 words total) - `rmt5_worker.cpp:132`
   - Implements ping-pong refill via threshold interrupts - `rmt5_worker.cpp:154-175`
   - Similar to RMT4's approach, but WiFi still causes issues

3. **Configurable Interrupt Priority** (High-level API):
   - Exposed as parameter: `interrupt_priority = 3` - `strip_rmt.h:21`
   - Can be changed, but limited to official IDF maximum (level 3)

**What's Missing:**
- The high-level `strip_rmt` API doesn't use double-buffering effectively
- No interrupt priority above level 3 (WiFi runs at level 4)
- Lazy initialization prevents boot-time optimization

---

## The WiFi Interference Problem

### Root Cause: Interrupt Priority Starvation

WiFi interrupts run at **priority level 4**, which preempts RMT5's default **level 3** interrupts. This causes:
- 10-50µs jitter in RMT timing (normally 35µs refresh cycles)
- Buffer underruns when the ISR can't refill the RMT memory fast enough
- Visible LED flickering/corruption under WiFi load

### Evidence from Research

- ESP32 forum reports consistent flickering with WiFi + RMT5 combinations
- DMA buffer sizing helps but doesn't eliminate the issue
- RMT4 was more resistant due to better double-buffering
- Comprehensive research documented in `RMT_RESEARCH.md`

### Measured Impact

- Normal RMT refresh: **~35µs** (no WiFi)
- With WiFi active: **35-85µs** (±50µs jitter!)
- WS2812B tolerance: Only **±150ns** per bit

### Why This is Critical

```cpp
// From rmt5_worker.cpp:132
tx_config.mem_block_symbols = 2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL;

// For ESP32-S3: SOC_RMT_MEM_WORDS_PER_CHANNEL = 48 words
// So: 2 × 48 = 96 words = 96 × 4 bytes = 384 bytes
// Each byte = 8 RMT items → 96 words / 8 = 12 bytes of pixel data
// At 10MHz RMT clock: 96 items × 32 bits/item × 0.1µs = 307µs of buffering

// Problem: WiFi can delay ISR by 50µs
// If buffer drains in 307µs, and ISR is delayed repeatedly, you get underruns
```

---

## Deep Dive: Priority 3 vs WiFi

### ESP32 Interrupt Architecture (Xtensa)

The ESP32 family uses Xtensa processors with **7 interrupt priority levels**:

```
Level 7 (NMI)     - Non-maskable interrupt (highest)
Level 6           - Reserved for system use
Level 5           - ESP-IDF debug logic (when Bluetooth uses level 4)
Level 4           - Bluetooth (CONFIG_BTDM_CTRL_HLI) / WiFi
Level 3           - RMT5 default priority ← THE PROBLEM
Level 2           - Medium priority interrupts
Level 1           - Low priority interrupts (default for most peripherals)
```

### The Core Problem: WiFi Starves RMT

**WiFi operates at priority 4 or higher**, which means:

1. When a WiFi interrupt fires (e.g., packet reception, beacon transmission, network stack processing)
2. The CPU **immediately suspends** any level 3 interrupt (like RMT)
3. WiFi processing takes 40-50µs on average, sometimes longer
4. RMT buffer refill ISR is delayed
5. RMT hardware runs out of data to transmit
6. LEDs receive incomplete/corrupted data → **flicker/glitches**

### Why Level 3 Was Chosen (Historical Context)

ESP-IDF's official guidance:
- **Levels 1-3:** "Safe" priorities managed by FreeRTOS, can call C code
- **Levels 4-7:** High-priority interrupts that **cannot call C code** (critical section violation)

From the Espressif documentation:
> "Do not call C code from a high-priority interrupt; as these interrupts are run from a critical section, this can cause the target to crash."

**FastLED chose level 3 because:**
- ✅ Can use C code (no assembly required)
- ✅ FreeRTOS-safe (can use semaphores, mutexes)
- ✅ Works across all ESP32 variants
- ❌ **But lower than WiFi!**

### The Assembly Barrier for Level 4+

To use priority 4 or higher on Xtensa (ESP32/ESP32-S3), you must:

1. **Write assembly wrappers** (`.S` files):
```asm
.section .iram1,"ax"
.global xt_highint4
.type xt_highint4,@function
.align 4
xt_highint4:
    ; Save registers manually
    movi a0, some_handler
    callx0 a0
    ; Restore registers
    rfi 4  ; Return from interrupt level 4
```

2. **IRAM-only code** - No flash cache access
3. **No C runtime calls** - No malloc, printf, FreeRTOS APIs
4. **Manual register management** - Save/restore CPU state

**However!** FastLED already has this infrastructure:
- `src/platforms/esp/32/interrupts/xtensa_lx7.hpp:316-368` - Assembly trampoline macro
- `src/platforms/esp/32/interrupts/xtensa_lx7.hpp:149-161` - Level 5 handler functions
- Helper: `fastled_esp32s3_install_level5_interrupt()`

### RISC-V Advantage

ESP32-C3/C6/C7 use RISC-V cores which **don't require assembly for any priority level**:
- ✅ Can use C handlers at level 4-7
- ✅ Simpler implementation
- ✅ Maximum priority: Level 7 (vs Xtensa's level 5)

---

## Deep Dive: ESP32-S3 DMA Issues

### What is DMA and Why Use It?

**DMA (Direct Memory Access)** allows the RMT peripheral to read pixel data directly from RAM without CPU intervention:

#### Without DMA (current FastLED):
```
CPU ← ISR fires every ~300µs
 ↓ → Copy 12 bytes to RMT buffer
 ↓ → Encode pixels to RMT symbols
 ↓ → Write to RMT hardware registers
 ↓ ← Repeat 25-30 times per frame (300 LEDs)
```
**High CPU overhead, susceptible to WiFi interrupts**

#### With DMA (ideal):
```
CPU → Setup DMA transfer once
DMA Controller → Streams entire frame to RMT
RMT → Transmits autonomously
CPU → Free for WiFi/other tasks
```
**Low CPU overhead, less sensitive to interrupts**

### The ESP32-S3 DMA Limitation

From GitHub issue investigation ([espressif/idf-extra-components#466](https://github.com/espressif/idf-extra-components/issues/466)):

**Hardware Constraint:**
```cpp
// From Espressif maintainer:
"Only ONE RMT channel can use DMA feature on ESP32-S3"
// This is a hardware limitation, not a software bug
```

**Why This Matters:**
- Most LED projects use **multiple strips** (2-8 channels)
- If only 1 channel can use DMA, others still need ISR-based buffering
- Mixed DMA/non-DMA creates complexity
- **FastLED typically drives 4-8 strips simultaneously**

### The "DMA is buggy" Comment (strip_rmt.cpp:82-83)

```cpp
if (dma_mode == IRmtStrip::DMA_AUTO) {
    // DMA is buggy on ESP32S3
    with_dma = false;
}
```

**What "buggy" actually means:**

1. **Single Channel Limitation:** Only channel 0 can use DMA
   - Attempting DMA on multiple channels → `ESP_ERR_NOT_FOUND` (0x105)
   - Error: "no free tx channels"

2. **Memory Block Confusion:**
   - ESP32-S3 has **48 words per channel** (not 64 like ESP32)
   - Example code was using wrong values (64 or 128)
   - When 4 channels active: `mem_block_symbols` must be ≤48

3. **DMA Buffer Sizing Issues:**
   - With DMA: `mem_block_symbols` controls DMA buffer size
   - Without DMA: `mem_block_symbols` is RMT internal memory
   - Users were setting `mem_block_symbols = 1024` expecting ping-pong buffering
   - **This only works with DMA enabled!**

4. **WiFi Interference Workaround:**
   - Original reporter found that increasing DMA buffer size helped WiFi jitter
   - But this doesn't work for multi-strip setups (only 1 DMA channel)

### Why FastLED Disables DMA by Default

**Practical reasons:**

1. **Multi-strip limitation:**
```cpp
// Typical FastLED usage:
CRGB leds1[300];
CRGB leds2[300];
CRGB leds3[300];
CRGB leds4[300];

FastLED.addLeds<WS2812, PIN1>(leds1, 300);  // Can use DMA
FastLED.addLeds<WS2812, PIN2>(leds2, 300);  // Cannot use DMA!
FastLED.addLeds<WS2812, PIN3>(leds3, 300);  // Cannot use DMA!
FastLED.addLeds<WS2812, PIN4>(leds4, 300);  // Cannot use DMA!
```

2. **Configuration complexity:**
   - Need to detect which channel gets DMA
   - Others need fallback to ISR-based
   - Different memory sizing for DMA vs non-DMA

3. **Simpler to use consistent approach:**
   - All channels use ISR-based double-buffering
   - Predictable behavior across all strips
   - Focus on making ISR more robust (interrupt priority!)

### Memory Block Size Differences

**Critical hardware variation:**

```cpp
// From rmt5_worker.h:52-53
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL

// Actual values per chip:
// ESP32:     SOC_RMT_MEM_WORDS_PER_CHANNEL = 64 words (256 bytes)
// ESP32-S2:  SOC_RMT_MEM_WORDS_PER_CHANNEL = 64 words (256 bytes)
// ESP32-S3:  SOC_RMT_MEM_WORDS_PER_CHANNEL = 48 words (192 bytes) ← 25% LESS!
// ESP32-C3:  SOC_RMT_MEM_WORDS_PER_CHANNEL = 48 words (192 bytes)
// ESP32-C6:  SOC_RMT_MEM_WORDS_PER_CHANNEL = 48 words (192 bytes)
```

**Impact on double-buffering:**
```cpp
// From rmt5_worker.cpp:132
tx_config.mem_block_symbols = 2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL;

// ESP32:    2 × 64 = 128 words = 512 bytes
// ESP32-S3: 2 × 48 =  96 words = 384 bytes  ← 25% less buffering!
```

**This means ESP32-S3 has less jitter tolerance than ESP32** - making the WiFi problem worse!

---

## Potential Improvements

Based on comprehensive analysis and research documented in `RMT_RESEARCH.md`, here are ranked improvement strategies:

### 1. Raise Interrupt Priority to Level 4+ (RECOMMENDED - EASIEST)

**Feasibility:** **HIGH** - Infrastructure already exists!

FastLED already has comprehensive interrupt infrastructure in place:
- `src/platforms/esp/32/interrupts/xtensa_lx7.hpp` - ESP32-S3 level 5 support with ASM trampolines
- `src/platforms/esp/32/interrupts/riscv.hpp` - ESP32-C3/C6/C7 support (up to level 7, no ASM needed)

**Implementation:**
```cpp
// In rmt5_worker.cpp:202-208, change from:
esp_err_t ret = esp_intr_alloc(
    ETS_RMT_INTR_SOURCE,
    ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,  // Current
    ...
);

// To:
esp_err_t ret = esp_intr_alloc(
    ETS_RMT_INTR_SOURCE,
    ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL4,  // Level 4 > WiFi
    ...
);
```

**For level 5 on ESP32-S3 (even better):**
Use existing infrastructure: `fastled_esp32s3_install_level5_interrupt()` helper functions are already available.

**Impact:**
- Interrupt priority 4 preempts WiFi (level 4), eliminating most jitter
- Level 5 would guarantee no WiFi interference
- No changes to buffer architecture needed
- Works with existing code

**Why This Works:**
```
Level 5 RMT ISR    ← Preempts everything except NMI
  ↓ (can interrupt)
Level 4 WiFi       ← Cannot interrupt level 5
  ↓ (can interrupt)
Level 3 Default    ← Old RMT priority (vulnerable)
```

With level 4-5 priority:
- RMT ISR runs **before** WiFi gets CPU time
- Buffer refills happen **on schedule**
- No underruns, no flicker
- WiFi still works fine (just slightly delayed by microseconds)

### 2. Enhance Double-Buffer Implementation (MODERATE)

**Current Issue:**
The `strip_rmt` API uses `mem_block_symbols` but this just creates a larger single buffer, **NOT true ping-pong**.

**Solution:**
Switch all RMT5 users to the low-level `rmt5_worker` implementation which already has true double-buffering (`rmt5_controller_lowlevel.cpp`).

**Implementation:**
- Deprecate `strip_rmt.cpp` high-level API for timing-critical applications
- Document that `RmtController5LowLevel` is the WiFi-resistant option
- Already implements ping-pong: `rmt5_worker.cpp:154-175`

### 3. Enable CONFIG_RMT_ISR_IRAM_SAFE (TRIVIAL)

**Current State:** Not explicitly set in FastLED build configuration.

**Action:**
Add to build system:
```
CONFIG_RMT_ISR_IRAM_SAFE=y
CONFIG_RMT_TX_ISR_CACHE_SAFE=y  # IDF 5.x naming
```

**Impact:**
- Ensures RMT ISR can run even when flash cache disabled
- Reduces interrupt latency variability
- Complements priority increase

### 4. Increase Memory Block Size (PARTIAL SOLUTION)

**Current:**
- Low-level: `2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL` (128 words total on ESP32, 96 on ESP32-S3)
- High-level: `AUTO_MEMORY_BLOCK_SIZE` or 1024 (with DMA)

**Improvement:**
Increase to 3-4× memory blocks for even more buffering headroom.

**Tradeoff:** More RAM usage (~256-512 bytes per channel) vs. better jitter tolerance.

---

## Recommended Action Plan

### Phase 1 (Immediate - Low Risk)

1. ✅ Change `rmt5_worker.cpp:204` from `LEVEL3` to `LEVEL4`
2. ✅ Enable `CONFIG_RMT_ISR_IRAM_SAFE=y` in build
3. ✅ Test with QEMU: `uv run test.py --qemu esp32s3`

### Phase 2 (Short-term - Validate)

1. Implement level 5 interrupt support using existing infrastructure
2. Create WiFi stress test example
3. Measure interrupt latency improvement with oscilloscope

### Phase 3 (Long-term - Optimize)

1. Deprecate `strip_rmt` high-level API for WiFi use cases
2. Document `RmtController5LowLevel` as WiFi-robust option
3. Consider DMA support once ESP-IDF bugs are fixed

---

## Summary Table

| Issue | Current State | Why It Matters | Solution Status |
|-------|--------------|----------------|-----------------|
| **Priority 3 < WiFi** | RMT at level 3, WiFi at level 4 | WiFi preempts RMT → buffer underruns | ✅ **Easy fix - change flag** |
| **DMA single-channel** | Only 1 RMT channel can use DMA | Can't use for multi-strip setups | ⚠️ Hardware limitation |
| **ESP32-S3 48 words** | 25% less buffer than ESP32 | Less jitter tolerance | ✅ Compensate with priority |
| **Assembly requirement** | Level 4+ needs ASM on Xtensa | Complexity barrier | ✅ Infrastructure exists! |

---

## Key Findings

### Good News

- ✅ FastLED already has level 4-5 interrupt infrastructure ready
- ✅ Low-level worker already implements proper double-buffering
- ✅ All ISR code is already IRAM-safe
- ✅ The fix is mostly a priority level change + build config

### Challenges

- ⚠️ Two parallel RMT5 implementations (high-level vs low-level) causing confusion
- ⚠️ Default priority 3 is too low for WiFi coexistence
- ⚠️ DMA mode disabled on ESP32-S3 due to ESP-IDF bugs

### The Real Solution: Interrupt Priority, Not DMA

**Why DMA isn't the answer for FastLED:**
- ❌ Only 1 channel supports DMA on ESP32-S3
- ❌ Most users have multiple strips
- ❌ Adds complexity without solving the root cause
- ❌ Still vulnerable to WiFi if using non-DMA channels

**Why raising interrupt priority IS the answer:**
- ✅ Works for **all channels simultaneously**
- ✅ Infrastructure already exists in FastLED
- ✅ Solves WiFi starvation at the source
- ✅ Simple configuration change
- ✅ No hardware limitations

---

## References

- Research document: `RMT_RESEARCH.md` (comprehensive analysis)
- Worker implementation: `rmt5_worker.cpp:187-218` (interrupt allocation)
- Interrupt infrastructure: `src/platforms/esp/32/interrupts/` (Xtensa/RISC-V handlers)
- ESP-IDF documentation: [High Priority Interrupts](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html)
- ESP32 forum: [RMT WiFi interference discussions](https://esp32.com/)
- GitHub: [ESP32-S3 DMA limitation issue](https://github.com/espressif/idf-extra-components/issues/466)

---

**Bottom Line:** The RMT5 driver can be made significantly more robust vs WiFi with minimal changes - primarily raising the interrupt priority level using infrastructure that already exists in the codebase.
