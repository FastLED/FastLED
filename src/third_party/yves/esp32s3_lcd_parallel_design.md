# ESP32-S3 I80/LCD Parallel LED Driver — 50 ns Timing Resolution, Multi-Chipset, 16-Lane

**Author:** you (with Assistant)  
**Target:** ESP32-S3, ESP-IDF (4.4+ / 5.x) and Arduino-as-component  
**Goal:** Drive up to 16 WS28xx-style "clockless" LED strips in parallel via the LCD (I80) peripheral with a uniform 50 ns timebase, allowing per-lane chipset timings (e.g., WS2812, WS2816) mixed within the same frame.

**Key idea:** Use the existing FastLED chipsets.h timing triplets (T1/T2/T3 in ns) verbatim, convert them to slot counts on a 50 ns grid, and generate an N-words-per-bit DMA stream (bit-sliced across 16 lanes). Transpose stays the same; timing is fully data-driven.

## 1) Requirements & Constraints

- **Hardware:** ESP32-S3 with LCD_CAM (I80 mode), GDMA, PSRAM recommended (8 MB ideal).
- **GPIO:** Up to 16 data lanes (D0…D15) + WR (PCLK). (DC/CS not used.)
- **Resolution:** Δt = 50 ns slot size (PCLK ≈ 20 MHz).
- **Parallelism:** Up to 16 LED strips in lockstep (one bit per lane).
- **Chipset mix:** Any WS28xx family whose Tbit ≈ T1+T2+T3 fits our chosen N.
- **Reliability:** Non-blocking streaming (single long DMA), double-buffered encode.
- **Logging:** UART or ring-buffered; avoid USB-CDC during transfers.

## 2) Timing Model (from FastLED)

FastLED defines clockless LED timing as three intervals per bit:
- **T1:** Rising edge → LOW point for bit 0
- **T2:** Additional HIGH time for bit 1
- **T3:** LOW tail to complete the bit

Total Tbit = T1 + T2 + T3.

We do not recompute; we consume the numbers from chipsets.h:
- `FASTLED_WS2812_T1/T2/T3` (ns)
- `FASTLED_WS2816_T1/T2/T3` (ns)
- …and so on.

## 3) Driver Architecture

### 3.1 Blocks
- **Encoder (CPU):** For each pixel and color bit, builds N 16-bit words (one per 50 ns slot). Word bit k corresponds to lane k (strip k). "HIGH run" length is computed from chipset timings per lane.
- **Transpose core:** Existing transpose16x1_noinline2 (or equivalent) remains; it rearranges per-lane bits into 16-bit parallel words. Timing is orthogonal to transpose.
- **I80 / LCD stream (GDMA):** One tx_color() (or equivalent) per frame using esp_lcd_panel_io_i80. Queue depth = 1. pclk_hz = 20 MHz (50 ns slots).
- **Double buffering:** Two PSRAM buffers (front streaming, back encoding). Swap on transfer-done ISR/semaphore.

### 3.2 Data Flow (one frame)
1. App submits per-lane CRGB arrays (or FastLED CRGBSet).
2. Encoder pulls T1/T2/T3 per lane (chipset) and precomputes:
   - S_T1 = ceil(T1 / 50ns)
   - S_T2 = round(T2 / 50ns)
   - S_T3 = ceil(T3 / 50ns)
   - N_lane = S_T1 + S_T2 + S_T3
3. Bit-length harmonization (see §4): choose N_bit = max(N_lane) across all active lanes for this frame; each lane's LOW tail pads to N_bit.
4. For each pixel index i:
   - For C in {G,R,B}:
     - For bit b = 7..0:
       - Zero N_bit 16-bit words; for each lane k:
         - hs0 = S_T1(k); hs1 = S_T1(k) + S_T2(k)
         - If bit is 1 for lane k: set words [0 .. hs1-1] bit k
         - else (bit 0): set words [0 .. hs0-1] bit k
       - Append these N_bit words to the DMA buffer.
5. After all pixels/colors, append latch gap by idling WR (LOW) for ≥ max reset among lanes (commonly 300 µs).
6. Kick one DMA transfer; encode next frame into the other buffer.

## 4) Bit-Length Harmonization (mixing chipsets)

All lanes share the same PCLK and therefore the same number of slots per bit (N_bit) at any instant. For mixing:
- Compute N_lane = ceil(T1/Δt) + round(T2/Δt) + ceil(T3/Δt) per lane.
- Choose N_bit = max(N_lane) across all lanes in the frame.
- For each lane, pad extra LOW slots at the end of the bit so that its bit duration equals N_bit * Δt.

(This keeps the shorter timing families compliant without stretching their HIGH durations.)

**Why ceil on T1/T3 and round on T2?**
Bias ensures minimum high is never shorter than spec (T1 is a minimum), while T2 often has wider tolerance; rounding keeps error centered. Adjust if your datasheet mandates otherwise.

## 5) Clocking & Slots

- **Slot size:** Δt = 50 ns → PCLK = 20 MHz (io_config.pclk_hz = 20'000'000 in esp-lcd).
- **Typical N_bit (examples):**
  - WS2812: T1350ns, T2600–700ns, T3~300–350ns → N ≈ 7 + 12 + 7 = 26 (≈1.3 µs)
  - WS2816: T1350ns, T2600–700ns, T3~250–300ns → N ≈ 25–26

Use N_bit = 26 for both → common bit ≈ 1.30 µs (well within tolerance).

If you must hit 1.25 µs exact, choose N_bit = 25 and bias T2 rounding down for WS2816 (verify with LA). The scheme supports any N_bit; memory scales linearly.

## 6) Memory & Throughput

### 6.1 Buffer size (one frame, one buffer)
- Per bit: N_bit words (each 16 bits → 2 bytes).
- Per color byte: 8 * N_bit words.
- Per pixel (GRB): 24 * N_bit words = 48 * N_bit bytes.
- For L LEDs/strip, 16 lanes in parallel: Bytes/frame = L * 48 * N_bit.

(Note: 16 lanes are encoded into the same words; no ×16 factor.)

**Example:** Δt=50 ns, N_bit = 26, L=1000 →
Bytes/frame ≈ 1000 * 48 * 26 ≈ 1,248,000 B ≈ 1.19 MB per buffer.
Double-buffering: ~2.38 MB. With 8 MB PSRAM, very comfortable.

### 6.2 Frame time
- Per-LED time ≈ 24 * N_bit * Δt
- For N_bit=26, Δt=50 ns → 31.2 µs/LED.
- Frame ≈ L * 31.2 µs + reset:
  - 300 LEDs/strip → ~9.66 ms + 0.30 ms → ~100 FPS max theoretical.
  - 1000 LEDs/strip → ~31.5 ms + 0.30 ms → ~31 FPS.

Throughput is timing-bound, not DMA-bound. 20 MB/s bus load (20 MHz × 2 B) is within S3 GDMA+PSRAM capability with long descriptors.

## 7) Concurrency, Jitter, and Stability

- **Pin mapping:** Ensure no overlap with I2S audio or other high-speed peripherals. (GPIO matrix is flexible; a pin cannot be double-booked.)
- **DMA:** Let esp-lcd allocate its own GDMA channel. Use one transfer per frame; trans_queue_depth = 1.
- **Buffers:** Prefer PSRAM for big frames. Use esp-lcd's draw-buffer helpers or aligned heap_caps_aligned_alloc with PSRAM caps; keep audio buffers (if any) in internal RAM.
- **ISR & tasks:**
  - Pin LCD ISR to one core; run encode task on the other.
  - Avoid USB-CDC logging during transfers; use UART or ring buffer flushed on transfer-done.
- **Clocking:** Keep CPU clock fixed (no light sleep / DFS).
- **Measured jitter mitigation:** If logic-analyzer shows p-p jitter > ~80–100 ns, raise N_bit by 1 (adds 50 ns slack), or bias S_T1 up by one slot to protect minimum highs.

## 8) Public API (proposed)

```cpp
// --- Types ---
enum class LedChipset { WS2812, WS2816, WS2813, Custom };

struct LaneConfig {
    int           gpio;        // Data pin for this lane
    LedChipset    chipset;     // Which timing triplet to use
    // Optional per-lane override:
    uint32_t T1_ns = 0, T2_ns = 0, T3_ns = 0;
};

struct DriverConfig {
    fl::vector<LaneConfig> lanes; // size 1..16
    uint32_t pclk_hz = 20'000'000; // 50 ns slots
    uint32_t latch_us = 300;       // reset gap
    bool     use_psram = true;     // buffers in PSRAM
    int      queue_depth = 1;      // must be 1 for long frames
};

class LcdLedDriverS3 {
public:
    bool begin(const DriverConfig& cfg);  // alloc buffers, create i80 bus/io
    void attachStrips(CRGB* strips[16], int leds_per_strip);
    bool show();                          // encode back buffer + start DMA
    bool busy() const;                    // DMA active?
    void wait();                          // block until done
    void setLaneChipset(int lane, LedChipset c);
    // Optional: set per-lane custom timings
    void setLaneTimings(int lane, uint32_t T1, uint32_t T2, uint32_t T3);
private:
    // internals: buffer ptrs, esp_lcd handles, slot counts per lane, etc.
};
```

- `begin` reads FastLED's `FASTLED_<CHIP>_T*` macros (from chipsets.h) and caches per-lane (S_T1, S_T2, S_T3).
- `show` performs per-bit N-slot emission with per-lane high-run, followed by one tx_color().

## 9) Encoder Core (sketch)

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

## 10) IDF Integration (I80 / esp-lcd)

- **Bus:** esp_lcd_new_i80_bus(bus_config) with .bus_width = 16, .data_gpio_nums[] = lane pins, .pclk_io_num = WR, clk_src = LCD_CLK_SRC_DEFAULT.
- **IO:** esp_lcd_new_panel_io_i80(io_config) with .pclk_hz = 20'000'000, .trans_queue_depth = 1, .on_color_trans_done = isr, .user_ctx = this.
- **DMA buffers:** Use esp_lcd_i80_alloc_draw_buffer() (IDF 5.x) or aligned PSRAM alloc; ensure 4/8-byte alignment and long descriptors.
- **Transfer:** use data-only write (tx_color) with a dummy "command" if required by the API (many drivers use 0x2C).

## 11) PlatformIO Configuration

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

## 12) Test Plan

1. **Unit timing:** On a logic analyzer, verify for each lane:
   - T1_meas ≈ S_T1 * 50 ns, T1+T2 ≈ HS1 * 50 ns, Tbit ≈ N_bit * 50 ns
   - Check min High is ≥ datasheet minimum after jitter.
2. **Mixing test:** Assign alternating lanes to WS2812/WS2816; show a bit-stress pattern (010101… then 101010…); verify both decode correctly.
3. **Long chain:** 16 lanes × 1000 LEDs; verify stable @ >30 FPS with double buffer.
4. **Stress:** Concurrent I2S audio or Wi-Fi idle on; confirm no decode errors (or move audio buffers to IRAM).
5. **Regression:** Change N_bit (25 vs 26) and ensure behavior within tolerance.

## 13) Performance & Limits

- **Memory bound more than CPU:** encoding is linear and cache-friendly; PSRAM BW sufficient at 20 MB/s bus.
- **Upper LED count:** With 8 MB PSRAM and N_bit=26:
  - Bytes/LED ≈ 48 * 26 = 1248 B
  - Two buffers → ~2.5 kB per LED total → ~3,200 LEDs total is ~4 MB;
  - but remember the formula already assumes all 16 lanes combined (no ×16).
  - From §6 example: ~1.2 MB per 1000 LEDs → ~6,500 LEDs fits 8 MB comfortably with headroom.
- **Jitter tolerance:** With 50 ns slots, quantization ±25 ns; remaining jitter sources (PLL, GDMA) are typically <<50 ns. If edge cases arise, bias S_T1/S_T2.

## 14) Known Gotchas & Remedies

- **USB-CDC prints stall frames:** Route logs to UART or buffer+flush between frames.
- **Pin conflicts:** Double-check board schematics; some S3 boards pre-wire USB-JTAG to certain GPIOs.
- **Queue depth >1:** Leads to interleaving and potential gaps; keep 1.
- **PSRAM hiccups:** Use dma_burst_size and alignment options (IDF 5.x) and consider moving hot metadata to IRAM.

## 15) Why this Design

- **Deterministic timing:** Everything derives from FastLED's canonical timings; no magic constants.
- **Mix-and-match:** Per-lane T1/T2/T3 on a common 50 ns grid enables mixed WS281x families in one pass.
- **Scalable:** Increase/decrease N_bit by 1 to trade precision vs memory without touching peripheral setup.
- **Simple streaming:** One DMA per frame; encode overlaps transfer; minimal ISR complexity.

## 16) Next Steps

- Implement the slot-based encoder and lane timing table that pulls T1/T2/T3 from chipsets.h.
- Integrate with existing transpose and esp-lcd init; add logic-analyzer hooks.
- Ship a demo: lanes 0–7 WS2812, lanes 8–15 WS2816, 300 LEDs each; show gradient + binary stress.
- Add runtime switch: per-lane chipset can be changed between frames (recompute slot counts, keep N_bit as max).

## Implementation Status

This design document serves as the specification for the `esp32s3_clockless_i2s.h` driver implementation in the FastLED library.