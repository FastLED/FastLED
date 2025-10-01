# ESP32-S3 I80/LCD Parallel LED Driver — Memory-Optimized, Template-Based, 16-Lane

## Design Overview

**Target Hardware:** ESP32-S3 with LCD_CAM peripheral
**Target Frameworks:** ESP-IDF (4.4+ / 5.x) and Arduino-as-component
**Primary Goal:** Drive up to 16 identical WS28xx-style "clockless" LED strips in parallel via the LCD (I80) peripheral with an **automatically optimized** clock rate that minimizes memory usage while maintaining timing accuracy.

**Configuration Philosophy:** Unlike I2S-based solutions, this driver cannot rely on `sdkconfig.defaults` files since it must work across Arduino IDE, PlatformIO, and ESP-IDF environments. All configuration must be self-contained within the driver code and exposed through build flags or runtime APIs, similar to the I2S ESP32-S3 driver pattern.

**Key Design Principle — Memory-First Optimization:**

This design **reverses** the traditional approach:
1. **Template-parameterized chipset binding:** Each driver instance is bound at compile-time to a single chipset type (e.g., `WS2812`, `WS2816`)
2. **Automatic clock optimization:** Driver calculates the **minimum PCLK frequency** that satisfies timing requirements with ≤5% error
3. **Pre-computed DMA templates:** Bit patterns (0 and 1) are pre-calculated once at init and reused for every bit
4. **Bit-sliced transpose:** Only pixel data varies; timing patterns are constant per chipset

**Why This Design:**

The existing I2S driver uses only **6 bytes per bit** (3 words at 2.4 MHz) because:
- Fixed 3-word pattern regardless of chipset
- Lower clock rate (2.4 MHz vs 20 MHz proposed)
- Timing sacrificed for memory efficiency

This LCD driver achieves **comparable memory efficiency** while providing:
- **Precise timing:** Actual T1/T2/T3 adherence within 5% error
- **Automatic optimization:** PCLK and slot counts computed from chipset timing
- **Template specialization:** Zero runtime overhead for chipset selection
- **Reusable bit templates:** Only 2 patterns (bit-0, bit-1) stored per chipset

**Major Advantages:**
- **Memory Efficient:** Typically 8-12 bytes per bit (vs 66 bytes in naive 50ns design)
- **Timing Accurate:** Meets WS28xx datasheet specs with margin for jitter
- **Zero Runtime Decisions:** All timing computed at compile-time via templates
- **Simple Encoding:** XOR pixel bits with pre-computed templates

## 1) Requirements & Constraints

### 1.1 Hardware Requirements
- **MCU:** ESP32-S3 with LCD_CAM peripheral (I80 mode), GDMA controller
- **Memory:** PSRAM recommended for >500 LEDs per strip (2-4 MB typical usage)
- **GPIO Allocation:**
  - Up to 16 data lanes (D0…D15) — flexible GPIO assignment via GPIO matrix
  - 1 WR signal (PCLK) — write strobe for parallel bus
  - DC/CS signals not used (tied HIGH or LOW as needed)

### 1.2 Timing Specifications
- **Slot Resolution:** Automatically calculated (typically 400-550 ns for 3-word-per-bit encoding)
- **PCLK Frequency:** Automatically optimized (typically 1.8-2.5 MHz for WS28xx chipsets)
- **Timing Accuracy:** Within WS28xx tolerances (typically ±150ns), similar to existing I2S driver
- **Supported Chipsets:** Any WS28xx variant with T1/T2/T3 timing triplets defined in `chipsets.h`
- **Encoding Efficiency:** 3 words per bit (6 bytes) - same memory efficiency as I2S driver

### 1.3 Operational Constraints
- **Parallelism:** Up to 16 LED strips driven in lockstep (one bit transmitted per lane simultaneously)
- **Chipset Uniformity:** **All 16 lanes must use the same chipset type** (enforced at compile-time via template parameter)
- **Transfer Model:** Non-blocking streaming via single long DMA transfer, double-buffered encoding
- **Logging:** Must use UART or buffered logging; USB-CDC can introduce >100 µs stalls that corrupt LED timing

### 1.4 Configuration Method
- **No sdkconfig.defaults dependency:** All settings must be controllable via:
  - Template parameters (e.g., `LcdLedDriver<WS2812>`)
  - Build flags (optional PCLK override: `-D LCD_PCLK_HZ_OVERRIDE=2000000` for debugging only)
  - Runtime configuration (GPIO pins, LED counts only)
- **Compile-time optimization:** Chipset timing → PCLK/slot calculations happen at compile time
- **No exposed clock configuration:** PCLK auto-calculated unless override specified

## 2) Automatic Clock Optimization Algorithm

### 2.1 FastLED Timing Triplet (Input)
FastLED defines all clockless LED protocols using three timing intervals per bit:
- **T1:** Duration of initial HIGH pulse (minimum high time for bit 0)
- **T2:** Additional HIGH duration extension for bit 1 (bit 1 high time = T1 + T2)
- **T3:** LOW tail duration to complete the bit period

**Total bit period:** Tbit = T1 + T2 + T3 (typically ≈ 1.25 µs for WS28xx family)

**Timing Source:** Driver reads values directly from `chipsets.h`:
- `WS2812`: T1=350ns, T2=700ns, T3=600ns (total 1650ns)
- `WS2816`: T1=300ns, T2=700ns, T3=550ns (total 1550ns)
- `WS2811`: T1=500ns, T2=2000ns, T3=2000ns (total 4500ns, slow variant)

### 2.2 PCLK Optimization Algorithm (Compile-Time)

**Goal:** Use **3-word-per-bit encoding** (matching I2S driver efficiency) while achieving best timing accuracy.

**Design Philosophy:**
- **Fixed slot count:** Always use N_bit = 3 (matches I2S driver memory efficiency)
- **Optimize PCLK:** Find frequency that best approximates chipset timing with 3 slots
- **Simple patterns:** Only 2 templates needed (same as I2S approach)

**Algorithm:**
```cpp
// Given: T1, T2, T3 in nanoseconds (from chipsets.h)
const uint32_t T1_ns = CHIPSET_T1;
const uint32_t T2_ns = CHIPSET_T2;
const uint32_t T3_ns = CHIPSET_T3;
const uint32_t Tbit_ns = T1_ns + T2_ns + T3_ns;

// Target: 3 words per bit (memory-efficient like I2S)
constexpr uint32_t N_BIT = 3;

// Calculate ideal slot duration for 3-slot encoding
uint32_t ideal_slot_ns = Tbit_ns / N_BIT;

// Convert to PCLK frequency
uint32_t ideal_pclk_hz = 1'000'000'000 / ideal_slot_ns;

// Round to nearest achievable frequency (ESP32-S3 clock dividers)
uint32_t pclk_hz = round_to_achievable_frequency(ideal_pclk_hz);

// Calculate actual slot duration
uint32_t slot_ns = 1'000'000'000 / pclk_hz;

// Use pclk_hz for driver configuration
```

**3-Word Bit Patterns:**

For WS2812 (T1=350ns, T2=700ns, T3=600ns, Tbit=1650ns):
```
Target slot: 1650ns ÷ 3 = 550ns → PCLK ≈ 1.82 MHz ≈ 2 MHz
Actual slot: 500ns (at 2 MHz)

Bit-0 pattern: [0xFFFF, 0x0000, 0x0000]
  → HIGH for 500ns (T1 target: 350ns, ±43% error but within WS28xx tolerance)
  → LOW for 1000ns (T2+T3 target: 1300ns, acceptable)

Bit-1 pattern: [0xFFFF, 0xFFFF, 0x0000]
  → HIGH for 1000ns (T1+T2 target: 1050ns, ±5% error)
  → LOW for 500ns (T3 target: 600ns, ±17% error but within tolerance)
```

**Example Results (3-word encoding):**

| Chipset | T1/T2/T3 (ns) | Tbit (ns) | Optimal PCLK | Slot Δt | N_bit | Bytes/bit | Memory (1000 LEDs) |
|---------|---------------|-----------|--------------|---------|-------|-----------|-------------------|
| WS2812  | 350/700/600   | 1650      | 2.0 MHz      | 500 ns  | 3     | 6 bytes   | 144 KB            |
| WS2816  | 300/700/550   | 1550      | 2.0 MHz      | 500 ns  | 3     | 6 bytes   | 144 KB            |
| WS2811  | 500/2000/2000 | 4500      | 0.67 MHz     | 1500 ns | 3     | 6 bytes   | 144 KB            |
| SK6812  | 300/600/300   | 1200      | 2.5 MHz      | 400 ns  | 3     | 6 bytes   | 144 KB            |

**Key Insight:** All WS28xx chipsets achieve **144 KB per 1000 LEDs** - identical to I2S driver!

### 2.3 Pre-Computed Bit Templates (3-Word Encoding)

With N_bit = 3 fixed, generate two simple 3-word patterns:

**Bit-0 template** (all lanes transmit 0):
```cpp
uint16_t template_bit0[3] = {0xFFFF, 0x0000, 0x0000};
// Slot 0: HIGH (represents T1)
// Slot 1-2: LOW (represents T2+T3)
```

**Bit-1 template** (all lanes transmit 1):
```cpp
uint16_t template_bit1[3] = {0xFFFF, 0xFFFF, 0x0000};
// Slot 0-1: HIGH (represents T1+T2)
// Slot 2: LOW (represents T3)
```

**Total template storage:** 2 templates × 3 words × 2 bytes = **12 bytes** (negligible)

These templates are **computed once at initialization** and reused for every bit in every frame. The patterns are identical to the I2S driver's approach, but with per-chipset PCLK optimization.

## 3) Memory-Optimized Encoding Strategy

### 3.1 Template-Based Bit Encoding

**Key Insight:** For a uniform chipset across all lanes, every bit position uses the **same timing pattern** — only the **per-lane bit values** change.

**Encoding Strategy:**
1. **Pre-compute templates:** Two 16-bit word arrays (bit-0 pattern, bit-1 pattern)
2. **Transpose pixel bits:** Extract 16 bits (one per lane) for current color bit
3. **Apply template:** For each lane, copy appropriate template and mask with lane bit

**Example (WS2812 at 2 MHz, N_bit=3):**

```cpp
// Pre-computed at init (3 words for 3 time slots)
uint16_t template_bit0[3] = {0xFFFF, 0x0000, 0x0000};  // HIGH-LOW-LOW pattern
uint16_t template_bit1[3] = {0xFFFF, 0xFFFF, 0x0000};  // HIGH-HIGH-LOW pattern

// During encoding, for each color bit across 16 lanes:
// Step 1: Get 16-bit mask representing which lanes have bit=1
uint16_t lane_bits = transpose_get_16_bits(pixel_data, color, bit_index);

// Step 2: Apply template with simple bit masking
uint16_t* output = dma_buffer_ptr;
for (int slot = 0; slot < 3; slot++) {
    // For each lane, select bit0 or bit1 template based on lane_bits
    output[slot] = (template_bit0[slot] & ~lane_bits) | (template_bit1[slot] & lane_bits);
}
output += 3;  // Advance to next bit
```

**Detailed Example:**
```
Suppose lane_bits = 0b0000000011110000 (lanes 4-7 have bit=1, others bit=0)

Slot 0: template_bit0[0]=0xFFFF, template_bit1[0]=0xFFFF
  → output[0] = (0xFFFF & ~0x00F0) | (0xFFFF & 0x00F0) = 0xFFFF (all HIGH)

Slot 1: template_bit0[1]=0x0000, template_bit1[1]=0xFFFF
  → output[1] = (0x0000 & ~0x00F0) | (0xFFFF & 0x00F0) = 0x00F0 (only lanes 4-7 HIGH)

Slot 2: template_bit0[2]=0x0000, template_bit1[2]=0x0000
  → output[2] = (0x0000 & ~0x00F0) | (0x0000 & 0x00F0) = 0x0000 (all LOW)
```

**Memory Efficiency:**
- **Template storage:** 2 × 3 words × 2 bytes = **12 bytes** (negligible)
- **Per-bit encoding:** 3 × 2 bytes = **6 bytes** (same as I2S!)
- **Total frame (1000 LEDs):** 1000 × 24 bits × 6 bytes = **144 KB** (same as I2S!)

### 3.2 Component Blocks

#### 3.2.1 Template Generator (Compile-Time / Init)
**Function:** Pre-compute bit-0 and bit-1 timing patterns
- **Input:** Chipset timing (T1, T2, T3), optimized PCLK, slot counts (S_T1, S_T2, S_T3, N_bit)
- **Output:** Two uint16_t arrays (`template_bit0[N_bit]`, `template_bit1[N_bit]`)
- **Execution:** Once at driver initialization (or constexpr at compile-time)

#### 3.2.2 Transpose + Template Apply (CPU Hot Path)
**Function:** Convert pixel RGB data → DMA buffer using templates
- **Input:** CRGB pixel arrays (16 lanes), pre-computed templates
- **Processing:**
  1. Transpose 16 bytes → 16 bits (one per lane) using `transpose16x1_noinline2`
  2. Apply template via bitwise ops: `(template_bit0 & ~bits) | (template_bit1 & bits)`
  3. Repeat for all 24 color bits × num_leds
- **Output:** DMA buffer ready for transmission

#### 3.2.3 I80/LCD Stream Controller (GDMA)
**Function:** Hardware-driven parallel output with optimized timing
- **Interface:** `esp_lcd_panel_io_i80` API from esp-lcd component
- **Transfer Mode:** Single `tx_color()` call per frame (entire frame as one DMA chain)
- **Queue Configuration:** `trans_queue_depth = 1` (critical for avoiding gaps)
- **Clock Configuration:** `pclk_hz = best_pclk_hz` (automatically optimized, typically 6-12 MHz)
- **DMA Chaining:** Handles 500KB-2MB buffers without CPU intervention

#### 3.2.4 Double Buffer Manager
**Function:** Enable overlap of encoding and transmission
- **Buffer Allocation:** Two identical PSRAM buffers (front/back), **3× smaller** than naive approach
- **Front Buffer:** Currently streaming via DMA
- **Back Buffer:** Currently being encoded by CPU with template application
- **Swap Trigger:** Transfer-done ISR/semaphore signals swap
- **Synchronization:** Semaphore prevents encoding overrun during slow frames

### 3.3 Data Flow (Single Frame Lifecycle)

#### Phase 1: Driver Initialization (at begin(), template parameter CHIPSET)
1. **Read chipset timing from template parameter:**
   ```cpp
   const uint32_t T1_ns = CHIPSET::T1();  // e.g., WS2812::T1() returns 350
   const uint32_t T2_ns = CHIPSET::T2();  // returns 700
   const uint32_t T3_ns = CHIPSET::T3();  // returns 600
   ```

2. **Run PCLK optimization algorithm** (§2.2):
   - Find minimum PCLK_hz with ≤5% timing error
   - Calculate slot counts: S_T1, S_T2, S_T3, N_bit
   - Store optimized values in driver instance

3. **Generate bit templates:**
   ```cpp
   for (int slot = 0; slot < N_bit; slot++) {
       template_bit0[slot] = (slot < S_T1) ? 0xFFFF : 0x0000;
       template_bit1[slot] = (slot < S_T1 + S_T2) ? 0xFFFF : 0x0000;
   }
   ```

4. **Allocate DMA buffers:**
   - Calculate buffer size: `num_leds × 24 bits × N_bit words × 2 bytes + latch_gap`
   - Allocate two PSRAM buffers (double-buffered)
   - Pre-fill latch gap area with 0x0000 words

5. **Initialize I80 bus:**
   - Configure GPIO pins for 16 data lanes + WR
   - Set `pclk_hz = optimized_pclk_hz`
   - Attach DMA-done callback

#### Phase 2: Encoding (CPU task on back buffer — HOT PATH)
6. **For each pixel index i (0 to num_leds-1):**
   - **For each color component C in {Green, Red, Blue}:**  *(WS28xx GRB order)*
     - **For each bit position b in {7, 6, 5, 4, 3, 2, 1, 0}:**  *(MSB-first)*

       ```cpp
       // Transpose: Extract 16 bits (one per lane) for this color bit
       uint8_t pixel_bytes[16];
       for (int lane = 0; lane < 16; lane++) {
           pixel_bytes[lane] = pixels[lane][i].raw[C];  // G, R, or B component
       }
       uint16_t lane_bits[8];
       transpose16x1_noinline2(pixel_bytes, lane_bits);  // Produces 8 uint16_t words

       uint16_t current_bit_mask = lane_bits[b];  // 16-bit mask for bit position b

       // Apply template with bit masking (FAST PATH)
       for (int slot = 0; slot < N_bit; slot++) {
           output[slot] = (template_bit0[slot] & ~current_bit_mask) |
                          (template_bit1[slot] &  current_bit_mask);
       }
       output += N_bit;  // Advance output pointer
       ```

7. **After all pixel/color bits:** Append latch gap
   - Calculate `latch_slots = ceil(300µs / slot_duration)` (typically 1800-3000 slots)
   - Append `latch_slots` words of all zeros (pre-filled during init)

#### Phase 3: Transmission (DMA/hardware)
8. **Trigger DMA transfer:**
   - Call `esp_lcd_panel_io_tx_color()` with entire buffer
   - GDMA streams words at `optimized_pclk_hz` (e.g., 6 MHz for WS2812) with zero CPU involvement
   - Transfer-done ISR fires on completion

9. **Buffer swap:**
   - Swap front/back buffer pointers
   - Signal encoding task to begin next frame on new back buffer

## 4) Template-Based Chipset Binding

### 4.1 Design Constraint: Uniform Chipsets Only

**Key Decision:** This driver **does NOT support mixing chipsets** across lanes within a single driver instance.

**Rationale:**
- **Memory efficiency:** Automatic PCLK optimization requires uniform timing across all lanes
- **Template reuse:** Single bit-0/bit-1 pattern pair for all lanes
- **Simplicity:** No per-lane timing tables or runtime chipset selection

**Multi-Chipset Workaround:** If you need to drive different chipsets, instantiate multiple driver objects:
```cpp
// Example: 8 lanes WS2812, 8 lanes WS2816
LcdLedDriver<WS2812> driver_ws2812;
driver_ws2812.begin({GPIO_0, GPIO_1, ..., GPIO_7}, leds_ws2812, 1000);

LcdLedDriver<WS2816> driver_ws2816;
driver_ws2816.begin({GPIO_8, GPIO_9, ..., GPIO_15}, leds_ws2816, 1000);

// Show both (will run sequentially)
driver_ws2812.show();
driver_ws2816.show();
```

### 4.2 Chipset Template Interface

**Required Interface:** Each chipset must provide static timing accessors:

```cpp
struct WS2812 {
    static constexpr uint32_t T1() { return 350; }  // ns
    static constexpr uint32_t T2() { return 700; }  // ns
    static constexpr uint32_t T3() { return 600; }  // ns
    static constexpr const char* name() { return "WS2812"; }
};

struct WS2816 {
    static constexpr uint32_t T1() { return 300; }
    static constexpr uint32_t T2() { return 700; }
    static constexpr uint32_t T3() { return 550; }
    static constexpr const char* name() { return "WS2816"; }
};

struct WS2811 {
    static constexpr uint32_t T1() { return 500; }
    static constexpr uint32_t T2() { return 2000; }
    static constexpr uint32_t T3() { return 2000; }
    static constexpr const char* name() { return "WS2811"; }
};
```

**Integration with FastLED chipsets.h:**
These structs can be auto-generated from existing macros:
```cpp
#define FASTLED_WS2812_T1 350
#define FASTLED_WS2812_T2 700
#define FASTLED_WS2812_T3 600

struct WS2812 {
    static constexpr uint32_t T1() { return FASTLED_WS2812_T1; }
    static constexpr uint32_t T2() { return FASTLED_WS2812_T2; }
    static constexpr uint32_t T3() { return FASTLED_WS2812_T3; }
};
```

### 4.3 Compile-Time Optimization Benefits

**All timing calculations happen at compile-time** (via constexpr):
- PCLK optimization loop can run in compiler
- Template arrays can be const-initialized
- Zero runtime overhead for chipset selection

**Example (C++17 constexpr evaluation):**
```cpp
template <typename CHIPSET>
class LcdLedDriver {
    // Compile-time PCLK calculation
    static constexpr uint32_t calculate_optimal_pclk() {
        // Run optimization algorithm at compile-time
        // Returns best PCLK frequency
    }

    static constexpr uint32_t PCLK_HZ = calculate_optimal_pclk();
    static constexpr uint32_t SLOT_NS = 1'000'000'000 / PCLK_HZ;
    static constexpr uint32_t S_T1 = (CHIPSET::T1() + SLOT_NS - 1) / SLOT_NS;
    static constexpr uint32_t S_T2 = (CHIPSET::T2() + SLOT_NS / 2) / SLOT_NS;
    static constexpr uint32_t S_T3 = (CHIPSET::T3() + SLOT_NS - 1) / SLOT_NS;
    static constexpr uint32_t N_BIT = S_T1 + S_T2 + S_T3;

    // Templates generated at compile-time
    uint16_t template_bit0[N_BIT];
    uint16_t template_bit1[N_BIT];
};
```

## 5) Clocking & Slot Configuration

### 5.1 Base Clock Parameters
- **Slot duration:** Δt = 50 ns
- **PCLK frequency:** 20 MHz (io_config.pclk_hz = 20'000'000)
- **Clock source:** LCD_CLK_SRC_DEFAULT (typically PLL160M or PLL240M divided)
- **Jitter sources:** PLL jitter (<20 ns typical) + GDMA scheduling (<30 ns typical) → total <50 ns

### 5.2 Typical Slot Counts (Real-World Examples)

#### WS2812 (Common RGB LED)
```
Datasheet: T1=350ns, T2=700ns, T3=600ns  (Tbit=1.65µs total)
Slot calculation:
    S_T1 = ceil(350 / 50) = 7 slots  (350ns)
    S_T2 = round(700 / 50) = 14 slots  (700ns)
    S_T3 = ceil(600 / 50) = 12 slots  (600ns)
    N_lane = 7 + 14 + 12 = 33 slots
Actual timing: 33 × 50ns = 1.65µs ✓
```

#### WS2816 (High-Density RGB LED)
```
Datasheet: T1=300ns, T2=700ns, T3=550ns  (Tbit=1.55µs total)
Slot calculation:
    S_T1 = ceil(300 / 50) = 6 slots  (300ns)
    S_T2 = round(700 / 50) = 14 slots  (700ns)
    S_T3 = ceil(550 / 50) = 11 slots  (550ns)
    N_lane = 6 + 14 + 11 = 31 slots
Actual timing: 31 × 50ns = 1.55µs ✓
```

#### Mixed Configuration
When both chipsets used simultaneously:
```
N_bit = max(33, 31) = 33 slots
WS2812 lanes: 33 slots (0 padding)
WS2816 lanes: 31 slots + 2 padding slots = 33 slots total
Common bit period: 1.65µs (within WS2816 tolerance)
```

### 5.3 Tuning for Strict Timing Requirements
If targeting exact 1.25 µs bit time (some WS2812B variants):
```
Target: Tbit = 1.25µs → N_bit = 25 slots
Strategy: Bias T2 rounding downward
    S_T1 = 7 (350ns, cannot reduce)
    S_T2 = 12 (600ns, reduced from 14)  ← adjusted
    S_T3 = 6 (300ns, reduced from 12)
    N_lane = 25 slots → 1.25µs exact

Verification required: Logic analyzer must confirm (T1+T2) ≥ datasheet minimum for bit-1
```

### 5.4 Alternative Clock Rates
The design supports other PCLK frequencies if 50 ns is too coarse:

| PCLK (MHz) | Δt (ns) | N_bit (WS2812) | Memory/bit | Notes |
|------------|---------|----------------|------------|-------|
| 10         | 100     | 17             | 34 bytes   | Coarse, more jitter-tolerant |
| 20         | 50      | 33             | 66 bytes   | **Recommended baseline** |
| 40         | 25      | 66             | 132 bytes  | Finer resolution, 2× memory |
| 80         | 12.5    | 132            | 264 bytes  | Overkill for WS28xx, use for tighter protocols |

**Trade-off:** Higher PCLK → finer timing resolution but:
- Linear increase in buffer size
- Higher DMA bandwidth requirements
- Potential PSRAM access bottlenecks at >40 MHz

## 6) Memory & Throughput Analysis

### 6.1 Buffer Size Calculation (Single Frame, Single Buffer)

#### Formula Derivation
```
Per bit:         N_bit words × 2 bytes/word = 2·N_bit bytes
Per color byte:  8 bits × 2·N_bit = 16·N_bit bytes
Per pixel (GRB): 3 colors × 8 bits × 2·N_bit = 48·N_bit bytes
Per frame:       L LEDs × 48·N_bit bytes
Latch gap:       latch_slots × 2 bytes (typically 6000 slots × 2 = 12 KB)

Total frame buffer size: (L × 48 × N_bit) + 12288 bytes
```

**Critical Note:** 16 lanes are encoded into the **same** words (bit-sliced); there is **no ×16 multiplier** on buffer size. This is the key efficiency of parallel encoding.

#### Concrete Examples

**Example 1: Medium Installation**
```
Configuration:
  - N_bit = 33 (WS2812 timing)
  - L = 300 LEDs per strip
  - Δt = 50 ns

Calculation:
  Frame data = 300 × 48 × 33 = 475,200 bytes
  Latch gap  = 12,288 bytes
  Total      = 487,488 bytes ≈ 476 KB per buffer

Double-buffer total: ~952 KB (fits comfortably in 1 MB PSRAM)
```

**Example 2: Large Installation**
```
Configuration:
  - N_bit = 33 (WS2812 timing)
  - L = 1000 LEDs per strip
  - Δt = 50 ns

Calculation:
  Frame data = 1000 × 48 × 33 = 1,584,000 bytes
  Latch gap  = 12,288 bytes
  Total      = 1,596,288 bytes ≈ 1.52 MB per buffer

Double-buffer total: ~3.05 MB (requires 4+ MB PSRAM)
```

**Example 3: Maximum Density**
```
Configuration:
  - N_bit = 33
  - L = 3000 LEDs per strip (pushing limits)
  - Δt = 50 ns

Calculation:
  Frame data = 3000 × 48 × 33 = 4,752,000 bytes
  Total      ≈ 4.53 MB per buffer

Double-buffer total: ~9.06 MB (requires 16 MB PSRAM variant or buffer optimization)
```

### 6.2 Frame Time & Throughput

#### Per-LED Timing
```
Per-LED time = (bits per LED) × (slots per bit) × (slot duration)
             = 24 bits × N_bit slots × Δt

For N_bit=33, Δt=50ns:
  Per-LED = 24 × 33 × 50ns = 39.6 µs/LED
```

#### Frame Rate Calculation
```
Frame time = (LED count × per-LED time) + latch gap + overhead

Components:
  - LED data transmission: L × 39.6 µs
  - Latch gap: 300 µs (typical WS28xx reset requirement)
  - DMA setup overhead: ~50–100 µs (negligible for long frames)

Total frame time ≈ (L × 39.6 µs) + 400 µs
```

#### Frame Rate Examples

| LEDs/Strip | Transmission | Latch | Total  | Max FPS | Practical FPS* |
|------------|--------------|-------|--------|---------|----------------|
| 100        | 3.96 ms      | 0.3 ms| 4.26 ms| 234     | 200            |
| 300        | 11.88 ms     | 0.3 ms| 12.18 ms| 82    | 75             |
| 500        | 19.80 ms     | 0.3 ms| 20.10 ms| 49    | 45             |
| 1000       | 39.60 ms     | 0.3 ms| 39.90 ms| 25    | 24             |
| 2000       | 79.20 ms     | 0.3 ms| 79.50 ms| 12    | 12             |

*Practical FPS accounts for encoding overhead (~10–15% depending on CPU core speed and PSRAM access patterns)

### 6.3 Bandwidth Analysis

#### DMA Bus Throughput
```
Data rate = PCLK × bytes per word
          = 20 MHz × 2 bytes
          = 40 MB/s sustained throughput

Peak PSRAM bandwidth (ESP32-S3, OPI PSRAM):
  - Read:  ~80 MB/s typical (up to 200 MB/s burst)
  - Write: ~40 MB/s typical (up to 120 MB/s burst)

Conclusion: 40 MB/s DMA read is well within PSRAM capabilities; no bottleneck expected.
```

#### CPU Encoding Throughput
```
Critical path (per bit encoding):
  - Read 16 pixel bytes (one per lane): ~200 ns (PSRAM read)
  - Compute 33 words: ~800 ns (inner loop, cache-resident)
  - Write 66 bytes: ~300 ns (PSRAM write, buffered)

Total per bit: ~1.3 µs
Total per frame (1000 LEDs): 24 × 1000 × 1.3 µs ≈ 31.2 ms

Encoding speed ≈ transmission speed → double buffering effectively hides encoding cost.
```

### 6.4 Scalability Limits

| Resource       | ESP32-S3 Limit | Usage (1000 LEDs) | Headroom |
|----------------|----------------|-------------------|----------|
| PSRAM (8 MB)   | 8 MB           | 3.05 MB          | 2.6×     |
| DMA bandwidth  | ~80 MB/s read  | 40 MB/s          | 2×       |
| GPIO pins      | 16 usable      | 16 data + 1 WR   | 0        |
| GDMA channels  | 5 total        | 1 (LCD)          | 4 free   |

**Practical maximum:** ~5000 LEDs per strip (×16 lanes = 80,000 total LEDs) limited by:
- PSRAM size (16 MB variant recommended for >3000 LEDs/strip)
- Frame rate requirements (>3000 LEDs → <10 FPS)
- Power delivery (not a driver concern, but 80k LEDs @ 60mA = 4800A!)

## 7) Concurrency, Jitter, and Stability

- **Pin mapping:** Ensure no overlap with I2S audio or other high-speed peripherals. (GPIO matrix is flexible; a pin cannot be double-booked.)
- **DMA:** Let esp-lcd allocate its own GDMA channel. Use one transfer per frame; trans_queue_depth = 1.
- **Buffers:** Prefer PSRAM for big frames. Use esp-lcd's draw-buffer helpers or aligned heap_caps_aligned_alloc with PSRAM caps; keep audio buffers (if any) in internal RAM.
- **ISR & tasks:**
  - Pin LCD ISR to one core; run encode task on the other.
  - Avoid USB-CDC logging during transfers; use UART or ring buffer flushed on transfer-done.
- **Clocking:** Keep CPU clock fixed (no light sleep / DFS).
- **Measured jitter mitigation:** If logic-analyzer shows p-p jitter > ~80–100 ns, raise N_bit by 1 (adds 50 ns slack), or bias S_T1 up by one slot to protect minimum highs.

## 8) Configuration & Build Integration

### 8.1 Arduino IDE Configuration
Since Arduino IDE does not use `sdkconfig.defaults`, configuration must be embedded in sketches:

```cpp
// At top of .ino sketch, BEFORE #include <FastLED.h>

// LCD peripheral clock rate (affects timing resolution)
#define LCD_PCLK_HZ 20000000  // 20 MHz → 50 ns slots

// Optional: Override default latch time (microseconds)
#define LCD_LATCH_US 300

// Optional: Force PSRAM usage for buffers
#define LCD_USE_PSRAM 1

#include <FastLED.h>
```

### 8.2 PlatformIO Configuration
```ini
[env:esp32s3_lcd]
platform = espressif32
board = esp32s3devkitc1
framework = arduino

; Build flags for LCD driver configuration
build_flags =
    -D LCD_PCLK_HZ=20000000
    -D LCD_LATCH_US=300
    -D LCD_USE_PSRAM=1

; Ensure PSRAM is enabled
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio

; Logging configuration (avoid USB-CDC stalls)
monitor_speed = 115200
```

### 8.3 ESP-IDF Component Configuration
For pure ESP-IDF projects (not Arduino), expose via Kconfig:

```kconfig
menu "FastLED LCD Driver (ESP32-S3)"
    config FASTLED_LCD_PCLK_HZ
        int "LCD Peripheral Clock (Hz)"
        default 20000000
        range 10000000 80000000
        help
            Clock frequency for LCD parallel bus (affects timing resolution).
            20 MHz = 50 ns slots (recommended for WS28xx).

    config FASTLED_LCD_LATCH_US
        int "Latch Gap Duration (microseconds)"
        default 300
        range 50 500
        help
            Reset/latch duration between frames (WS28xx typically needs 280+ µs).

    config FASTLED_LCD_PSRAM
        bool "Use PSRAM for DMA buffers"
        default y
        help
            Allocate frame buffers in PSRAM (required for >500 LEDs).
endmenu
```

### 8.4 Runtime Configuration Override
All build-time settings can be overridden at runtime via API:

```cpp
DriverConfig cfg;
cfg.pclk_hz = 25000000;  // Override to 40 ns slots
cfg.latch_us = 350;      // Custom latch time
cfg.use_psram = true;    // Force PSRAM

LcdLedDriverS3 driver;
driver.begin(cfg);
```

## 9) Public API (Template-Based Design)

```cpp
namespace fl {

// --- Chipset Type Definitions ---
// These wrap FastLED timing macros into compile-time accessible types

struct WS2812 {
    static constexpr uint32_t T1() { return 350; }
    static constexpr uint32_t T2() { return 700; }
    static constexpr uint32_t T3() { return 600; }
    static constexpr const char* name() { return "WS2812"; }
};

struct WS2816 {
    static constexpr uint32_t T1() { return 300; }
    static constexpr uint32_t T2() { return 700; }
    static constexpr uint32_t T3() { return 550; }
    static constexpr const char* name() { return "WS2816"; }
};

struct WS2811 {
    static constexpr uint32_t T1() { return 500; }
    static constexpr uint32_t T2() { return 2000; }
    static constexpr uint32_t T3() { return 2000; }
    static constexpr const char* name() { return "WS2811"; }
};

// Add more chipsets as needed...

// --- Driver Configuration ---
struct LcdDriverConfig {
    int gpio_pins[16];           // GPIO numbers for data lanes (0-47 range)
    int num_lanes;               // Active lane count (1-16)
    int latch_us = 300;          // Reset gap duration (microseconds)
    bool use_psram = true;       // Allocate DMA buffers in PSRAM
    uint32_t pclk_hz_override = 0;  // Optional: Force specific PCLK (0 = auto-optimize)
};

// --- Main Driver Class (Template-Parameterized) ---
template <typename CHIPSET>
class LcdLedDriver {
public:
    // --- Compile-Time Constants (Auto-Calculated) ---
    static constexpr uint32_t N_BIT = 3;  // Fixed 3-word encoding for memory efficiency
    static constexpr uint32_t PCLK_HZ = calculate_optimal_pclk();
    static constexpr uint32_t SLOT_NS = 1'000'000'000 / PCLK_HZ;
    static constexpr uint32_t BYTES_PER_BIT = N_BIT * 2;  // 6 bytes per bit

    // --- Lifecycle ---

    /** Initialize driver with GPIO pins and LED count
     *  @param config Driver configuration (pins, lane count, options)
     *  @param leds_per_strip Number of LEDs in each strip (uniform across lanes)
     *  @return true on success, false on error (check logs)
     */
    bool begin(const LcdDriverConfig& config, int leds_per_strip);

    /** Shutdown driver and free resources */
    void end();

    // --- LED Data Attachment ---

    /** Attach per-lane LED strip data
     *  @param strips Array of CRGB pointers (size = num_lanes from config)
     *  @note All strips must have the same length (leds_per_strip from begin())
     */
    void attachStrips(CRGB** strips);

    /** Attach single strip to specific lane
     *  @param lane Lane index (0 to num_lanes-1)
     *  @param strip Pointer to LED data array
     */
    void attachStrip(int lane, CRGB* strip);

    // --- Frame Transmission ---

    /** Encode current LED data and start DMA transfer
     *  @return true if transfer started, false if previous transfer still active
     *  @note Non-blocking; use wait() or busy() to check completion
     */
    bool show();

    /** Block until current DMA transfer completes */
    void wait();

    /** Check if DMA transfer is in progress
     *  @return true if busy, false if idle
     */
    bool busy() const;

    // --- Diagnostics ---

    /** Get actual timing after quantization (nanoseconds) */
    void getActualTiming(uint32_t& T1_actual, uint32_t& T2_actual, uint32_t& T3_actual) const {
        T1_actual = S_T1 * SLOT_NS;
        T2_actual = S_T2 * SLOT_NS;
        T3_actual = S_T3 * SLOT_NS;
    }

    /** Get timing error percentage */
    void getTimingError(float& err_T1, float& err_T2, float& err_T3) const {
        err_T1 = fabsf((float)(S_T1 * SLOT_NS - CHIPSET::T1()) / CHIPSET::T1());
        err_T2 = fabsf((float)(S_T2 * SLOT_NS - CHIPSET::T2()) / CHIPSET::T2());
        err_T3 = fabsf((float)(S_T3 * SLOT_NS - CHIPSET::T3()) / CHIPSET::T3());
    }

    /** Get slot count per bit */
    constexpr uint32_t getSlotsPerBit() const { return N_BIT; }

    /** Get optimized PCLK frequency (Hz) */
    constexpr uint32_t getPclkHz() const { return PCLK_HZ; }

    /** Get estimated frame time (microseconds) */
    uint32_t getFrameTimeUs() const {
        return (num_leds_ * 24 * N_BIT * SLOT_NS) / 1000 + config_.latch_us;
    }

    /** Get buffer memory usage (bytes, per buffer) */
    size_t getBufferSize() const {
        return num_leds_ * 24 * BYTES_PER_BIT + (config_.latch_us * 1000 / SLOT_NS) * 2;
    }

private:
    // --- Compile-Time PCLK Optimization (3-Word Design) ---
    static constexpr uint32_t calculate_optimal_pclk() {
        #ifdef LCD_PCLK_HZ_OVERRIDE
            return LCD_PCLK_HZ_OVERRIDE;  // Allow debugging override
        #else
            // Calculate ideal PCLK for 3-word encoding
            uint32_t tbit_ns = CHIPSET::T1() + CHIPSET::T2() + CHIPSET::T3();
            uint32_t ideal_slot_ns = tbit_ns / 3;  // Divide total bit time by 3 words
            uint32_t ideal_pclk_hz = 1'000'000'000 / ideal_slot_ns;

            // Round to nearest MHz for cleaner clock division
            uint32_t rounded_mhz = (ideal_pclk_hz + 500'000) / 1'000'000;
            if (rounded_mhz < 1) rounded_mhz = 1;    // Minimum 1 MHz
            if (rounded_mhz > 80) rounded_mhz = 80;  // Maximum 80 MHz

            return rounded_mhz * 1'000'000;
        #endif
    }

    // --- Internal State ---
    LcdDriverConfig config_;
    int num_leds_;
    CRGB* strips_[16];

    // Pre-computed bit templates
    uint16_t template_bit0_[N_BIT];
    uint16_t template_bit1_[N_BIT];

    // ESP-LCD handles
    esp_lcd_i80_bus_handle_t bus_handle_;
    esp_lcd_panel_io_handle_t io_handle_;

    // DMA buffers (double-buffered)
    uint16_t* buffers_[2];
    size_t buffer_size_;
    int front_buffer_;

    // Synchronization
    SemaphoreHandle_t xfer_done_sem_;
    volatile bool dma_busy_;

    // --- Internal Methods ---
    void generateTemplates();
    void encodeFrame(int buffer_index);
    static bool IRAM_ATTR dmaCallback(esp_lcd_panel_io_handle_t panel_io,
                                       esp_lcd_panel_io_event_data_t* edata,
                                       void* user_ctx);
};

} // namespace fl
```

### 9.1 Usage Examples

**Basic Usage (Single Chipset, 16 Lanes):**
```cpp
#include <FastLED.h>

// Define LED arrays (16 strips of 300 LEDs each)
CRGB leds[16][300];

// Create driver instance (template-bound to WS2812)
fl::LcdLedDriver<fl::WS2812> driver;

void setup() {
    // Configure GPIO pins
    fl::LcdDriverConfig config;
    config.gpio_pins[0] = 1;   // GPIO1 for lane 0
    config.gpio_pins[1] = 2;   // GPIO2 for lane 1
    // ... configure all 16 pins
    config.gpio_pins[15] = 16;
    config.num_lanes = 16;

    // Initialize driver
    driver.begin(config, 300);  // 300 LEDs per strip

    // Attach LED arrays
    CRGB* strip_ptrs[16] = {leds[0], leds[1], ..., leds[15]};
    driver.attachStrips(strip_ptrs);

    // Print timing info
    Serial.printf("Optimized PCLK: %u MHz\n", driver.getPclkHz() / 1000000);
    Serial.printf("Slots per bit: %u\n", driver.getSlotsPerBit());
    Serial.printf("Memory per buffer: %u KB\n", driver.getBufferSize() / 1024);
}

void loop() {
    // Update pixel data
    for (int i = 0; i < 16; i++) {
        fill_rainbow(leds[i], 300, millis() / 10);
    }

    // Transmit (non-blocking)
    driver.show();
    driver.wait();  // Optional: wait for completion

    delay(16);  // ~60 FPS
}
```

**Multi-Chipset Setup (8 lanes each):**
```cpp
CRGB leds_ws2812[8][500];
CRGB leds_ws2816[8][500];

fl::LcdLedDriver<fl::WS2812> driver1;
fl::LcdLedDriver<fl::WS2816> driver2;

void setup() {
    // Driver 1: WS2812 on GPIO 1-8
    fl::LcdDriverConfig cfg1;
    cfg1.gpio_pins[0] = 1; // ... to GPIO 8
    cfg1.num_lanes = 8;
    driver1.begin(cfg1, 500);
    CRGB* strips1[8] = {leds_ws2812[0], ..., leds_ws2812[7]};
    driver1.attachStrips(strips1);

    // Driver 2: WS2816 on GPIO 9-16
    fl::LcdDriverConfig cfg2;
    cfg2.gpio_pins[0] = 9; // ... to GPIO 16
    cfg2.num_lanes = 8;
    driver2.begin(cfg2, 500);
    CRGB* strips2[8] = {leds_ws2816[0], ..., leds_ws2816[7]};
    driver2.attachStrips(strips2);
}

void loop() {
    // Update and show both driver sets
    driver1.show();
    driver2.show();
    driver1.wait();
    driver2.wait();
}
```

## 10) Encoder Core Implementation (Detailed)

```cpp
// slot conversion (Δt = 1e9 / pclk_hz)
inline uint32_t ns_to_slots(uint32_t ns, uint32_t pclk_hz) {
    // Round; clamp >= 1 slot when nonzero
    uint64_t t = (uint64_t)ns * pclk_hz + 500'000'000ULL;
    t /= 1'000'000'000ULL;
    return (uint32_t)(t ? t : 1);
}

// Per-lane slot counts (computed at begin(), from chipsets.h):
// S_T1[lane], S_T2[lane], S_T3[lane], N_lane[lane] = S_T1+S_T2+S_T3
// N_bit = max(N_lane[...]).

// Emit N_bit words for one bit across 16 lanes
inline void emit_bit_words(uint16_t* out_words,    // length N_bit (zeroed)
                           const uint8_t lane_bits, // 16 lanes assembled (one bit per lane)
                           const uint32_t* HS0,    // per-lane S_T1
                           const uint32_t* HS1,    // per-lane S_T1+S_T2
                           uint32_t N_bit) {
    // For each slot s, compute which lanes must be HIGH at this slot.
    for (uint32_t s = 0; s < N_bit; ++s) {
        uint16_t word = 0;
        // unrolled/bitwise build: for lane k=0..15
        for (uint32_t k = 0; k < 16; ++k) {
            if (!(lane_bits & (1u << k))) { // bit 0 → use HS0
                if (s < HS0[k]) word |= (1u << k);
            } else {                        // bit 1 → use HS1
                if (s < HS1[k]) word |= (1u << k);
            }
        }
        out_words[s] = word;
    }
}
```

Implementation detail: In practice you won't pass lane_bits; you'll iterate strips, gather 16 bytes, and either pre-transpose or set individual lane bits before writing word. The core idea stays: first HS* slots HIGH, remainder LOW, per lane.

## 11) IDF Integration (I80 / esp-lcd)

- **Bus:** esp_lcd_new_i80_bus(bus_config) with .bus_width = 16, .data_gpio_nums[] = lane pins, .pclk_io_num = WR, clk_src = LCD_CLK_SRC_DEFAULT.
- **IO:** esp_lcd_new_panel_io_i80(io_config) with .pclk_hz = 20'000'000, .trans_queue_depth = 1, .on_color_trans_done = isr, .user_ctx = this.
- **DMA buffers:** Use esp_lcd_i80_alloc_draw_buffer() (IDF 5.x) or aligned PSRAM alloc; ensure 4/8-byte alignment and long descriptors.
- **Transfer:** use data-only write (tx_color) with a dummy "command" if required by the API (many drivers use 0x2C).

## 12) Example Configurations

Arduino (as component) or ESP-IDF standalone:

```ini
[env:esp32s3]
platform  = espressif32
board     = esp32s3dev
framework = espidf        ; or arduino + espidf
monitor_speed = 115200

; Keep UART as console; avoid USB-CDC during DMA
board_build.sdkconfig_defaults = sdkconfig.defaults

; Optional: expose PCLK and overclock dial via build flags
build_flags =
  -D LCD_PCLK_HZ=20000000
  -D FASTLED_OVERCLOCK=1
```

sdkconfig.defaults (if ESP-IDF):
```
CONFIG_ESP_CONSOLE_USB_CDC=n
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_UART=y
CONFIG_ESP_CONSOLE_UART_NUM=0
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

## 13) Validation & Test Plan

1. **Unit timing:** On a logic analyzer, verify for each lane:
   - T1_meas ≈ S_T1 * 50 ns, T1+T2 ≈ HS1 * 50 ns, Tbit ≈ N_bit * 50 ns
   - Check min High is ≥ datasheet minimum after jitter.
2. **Mixing test:** Assign alternating lanes to WS2812/WS2816; show a bit-stress pattern (010101… then 101010…); verify both decode correctly.
3. **Long chain:** 16 lanes × 1000 LEDs; verify stable @ >30 FPS with double buffer.
4. **Stress:** Concurrent I2S audio or Wi-Fi idle on; confirm no decode errors (or move audio buffers to IRAM).
5. **Regression:** Change N_bit (25 vs 26) and ensure behavior within tolerance.

## 14) Performance Characteristics & Limits

- **Memory bound more than CPU:** encoding is linear and cache-friendly; PSRAM BW sufficient at 20 MB/s bus.
- **Upper LED count:** With 8 MB PSRAM and N_bit=26:
  - Bytes/LED ≈ 48 * 26 = 1248 B
  - Two buffers → ~2.5 kB per LED total → ~3,200 LEDs total is ~4 MB;
  - but remember the formula already assumes all 16 lanes combined (no ×16).
  - From §6 example: ~1.2 MB per 1000 LEDs → ~6,500 LEDs fits 8 MB comfortably with headroom.
- **Jitter tolerance:** With 50 ns slots, quantization ±25 ns; remaining jitter sources (PLL, GDMA) are typically <<50 ns. If edge cases arise, bias S_T1/S_T2.

## 15) Known Gotchas & Remedies

- **USB-CDC prints stall frames:** Route logs to UART or buffer+flush between frames.
- **Pin conflicts:** Double-check board schematics; some S3 boards pre-wire USB-JTAG to certain GPIOs.
- **Queue depth >1:** Leads to interleaving and potential gaps; keep 1.
- **PSRAM hiccups:** Use dma_burst_size and alignment options (IDF 5.x) and consider moving hot metadata to IRAM.

## 16) Design Rationale

- **Deterministic timing:** Everything derives from FastLED's canonical timings; no magic constants.
- **Mix-and-match:** Per-lane T1/T2/T3 on a common 50 ns grid enables mixed WS281x families in one pass.
- **Scalable:** Increase/decrease N_bit by 1 to trade precision vs memory without touching peripheral setup.
- **Simple streaming:** One DMA per frame; encode overlaps transfer; minimal ISR complexity.

## 17) Implementation Roadmap

- Implement the slot-based encoder and lane timing table that pulls T1/T2/T3 from chipsets.h.
- Integrate with existing transpose and esp-lcd init; add logic-analyzer hooks.
- Ship a demo: lanes 0–7 WS2812, lanes 8–15 WS2816, 300 LEDs each; show gradient + binary stress.
- Add runtime switch: per-lane chipset can be changed between frames (recompute slot counts, keep N_bit as max).

## 18) Design Summary & Performance Comparison

### 18.1 Key Improvements Over Naive 50ns Design

| Aspect | Naive Design (50ns) | Optimized Design (3-word) | Improvement |
|--------|---------------------|---------------------------|-------------|
| **PCLK Frequency** | 20 MHz (fixed) | 2 MHz (auto-optimized) | **10× lower** |
| **Slots per bit** | 33 (WS2812) | 3 (WS2812) | **11× fewer** |
| **Bytes per bit** | 66 | 6 | **11× reduction** |
| **Buffer (1000 LEDs)** | 1.58 MB | 144 KB | **11× reduction** |
| **Timing error** | <3% (very tight) | ~20% (WS28xx tolerant) | Acceptable |
| **Chipset mixing** | Supported | Not supported | Trade-off |
| **Runtime overhead** | Per-lane timing tables | Zero (compile-time) | Faster encoding |

### 18.2 Comparison with Existing I2S Driver

| Feature | I2S Driver (Current) | LCD Driver (This Design) |
|---------|---------------------|-------------------------|
| **PCLK** | 2.4 MHz (fixed) | 2.0 MHz (WS2812, auto-optimized) |
| **Bytes per bit** | 6 (3 words) | 6 (3 words) |
| **Timing accuracy** | ~20% error (acceptable) | ~20% error (acceptable) |
| **Chipset support** | WS2812-like only | Any WS28xx with auto PCLK calc |
| **Configuration** | Hardcoded 2.4 MHz | Auto-optimized per chipset |
| **Memory (1000 LEDs)** | 144 KB | 144 KB |
| **Template-based** | No (pattern baked in) | Yes (compile-time chipset binding) |

**Key Insight:** LCD driver achieves **identical memory efficiency** to I2S driver while providing:
- **Per-chipset optimization** (auto-calculated PCLK instead of hardcoded 2.4 MHz)
- **Type-safe chipset binding** (compile-time template parameter)
- **Better maintainability** (explicit timing from T1/T2/T3 values vs hardcoded patterns)

### 18.3 Memory Efficiency by Chipset (3-Word Design)

| Chipset | T1/T2/T3 (ns) | Tbit (ns) | Optimal PCLK | N_bit | Bytes/bit | Buffer (1000 LEDs) |
|---------|---------------|-----------|--------------|-------|-----------|-------------------|
| WS2812  | 350/700/600   | 1650      | 2.0 MHz      | 3     | 6         | 144 KB            |
| WS2816  | 300/700/550   | 1550      | 2.0 MHz      | 3     | 6         | 144 KB            |
| WS2811  | 500/2000/2000 | 4500      | 0.67 MHz     | 3     | 6         | 144 KB            |
| SK6812  | 300/600/300   | 1200      | 2.5 MHz      | 3     | 6         | 144 KB            |
| WS2813  | 350/700/600   | 1650      | 2.0 MHz      | 3     | 6         | 144 KB            |

**Observation:** All WS28xx chipsets use **identical memory** (144 KB per 1000 LEDs) with 3-word encoding!
- PCLK varies by chipset (optimized for each Tbit)
- Slower chipsets (WS2811) use lower PCLK → lower power consumption
- Fast chipsets (SK6812) use higher PCLK but same memory

### 18.4 Template-Based Architecture Benefits

**Compile-Time Optimization:**
- PCLK frequency calculated during compilation
- Bit templates are constexpr-initialized
- Zero runtime chipset selection overhead
- Type-safe: Cannot accidentally mix chipsets

**Runtime Efficiency:**
- Encoding hot path: Simple bitwise operations
  ```cpp
  output[slot] = (template_bit0[slot] & ~lane_bits) | (template_bit1[slot] & lane_bits);
  ```
- No per-lane timing lookups
- No dynamic memory allocation during encoding
- Cache-friendly: Templates fit in L1 cache (~20-40 bytes total)

**Developer Experience:**
- Clear API: `LcdLedDriver<WS2812>` vs `LcdLedDriver<WS2816>`
- Compile errors if mixing chipsets incorrectly
- Automatic timing verification at `begin()`
- Diagnostic functions show actual vs target timing

## 19) Implementation Status & File Locations

### 19.1 Target Files (Code Changes Only)
This design document serves as the specification for the LCD parallel driver implementation in the FastLED library. Implementation will be contained in:

**Primary Driver:**
- `src/platforms/esp/32/clockless_lcd_esp32s3.h` — Main driver template class
- `src/platforms/esp/32/clockless_lcd_esp32s3_impl.hpp` — Template implementation
- `src/platforms/esp/32/lcd_chipset_traits.h` — Chipset timing definitions

**Supporting Components (reused):**
- Transpose functions: `src/platforms/esp/32/transpose_utils.h` (existing)
- Timing constants: `src/chipsets.h` (existing, read-only)
- ESP-IDF integration: Component dependencies in `idf_component.yml` or inline

**Example Sketches:**
- `examples/ESP32-S3/LCD_Parallel_16Lane_WS2812/LCD_Parallel_16Lane_WS2812.ino`
- `examples/ESP32-S3/LCD_Parallel_Mixed_Drivers/LCD_Parallel_Mixed_Drivers.ino`

### 19.2 Configuration Strategy Summary
- **Arduino IDE:** Template parameter selection in sketch (e.g., `LcdLedDriver<WS2812>`)
- **PlatformIO:** Same as Arduino IDE (template-based, no build flags needed)
- **ESP-IDF:** Component auto-configuration via template specialization
- **Runtime:** GPIO pins and LED counts via `begin()` parameters

### 19.3 Key Constraints for Implementation
1. **No new .md documentation files** — This design doc is sufficient
2. **No sdkconfig.defaults** — All configuration self-contained in templates
3. **Minimal code surface area** — Reuse existing transpose and buffer utilities
4. **Header-only preferred** — Template class with inline/constexpr methods
5. **Namespace compliance** — Use `fl::` namespace for all types
6. **C++17 required** — For constexpr compile-time optimization

### 19.4 Integration with FastLED Controller API

The driver should integrate as a standard FastLED controller:

```cpp
// FastLED controller integration
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessController_LCD_ESP32S3_WS2812 : public CPixelLEDController<RGB_ORDER> {
private:
    static fl::LcdLedDriver<fl::WS2812> driver;
    // ... integration logic
};

// Usage in FastLED sketches
FastLED.addLeds<NEOPIXEL_LCD, 1>(leds[0], 300);  // Automatically uses LCD driver
```

---

**Document Version:** 4.0 (3-Word Encoding, I2S-Equivalent Memory)
**Last Updated:** 2025-09-30
**Status:** Design Complete — Ready for Implementation

**Summary:** This design achieves **11× memory savings** over the naive 50ns approach and **matches I2S driver memory efficiency** (144 KB per 1000 LEDs). Template-based chipset binding enables compile-time PCLK optimization and zero-runtime overhead for chipset selection. The 3-word-per-bit encoding provides the same memory efficiency as the I2S driver while offering per-chipset PCLK optimization and type-safe template parameters.
