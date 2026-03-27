---
name: esp32-arch-review
description: Review ESP32 FastLED firmware architecture for RTOS safety, DMA correctness, LED driver patterns, memory management, and peripheral safety. Use before merging significant driver changes, new platform ports, or when auditing existing ESP32 FastLED code.
argument-hint: <file, directory, or component to review>
context: fork
agent: esp32-arch-review-agent
---

Perform a structured architecture review of ESP32 firmware code, with emphasis on FastLED driver patterns, DMA safety, RTOS correctness, and LED-specific constraints.

$ARGUMENTS

## What This Skill Reviews

### 1. FastLED Driver Architecture

Review LED driver implementations under `src/platforms/esp/`:

- [ ] Driver inherits from correct base class (`ClocklessController`, `ClockedController`, or equivalent)
- [ ] `show()` method is interrupt-safe and atomic from the LED strip's perspective
- [ ] Pixel data encoding is performed before DMA transfer starts (no on-the-fly encoding race)
- [ ] Color order (RGB/GRB/BGR) is handled at compile time, not runtime switch
- [ ] Gamma correction and color scaling applied before encoding, not after DMA
- [ ] `leds_data` / `CRGB` array not modified during active DMA transfer
- [ ] FastLED's `show_ordinal()` / `beginShow()` / `endShow()` lifecycle respected
- [ ] Brightness scaling via `FastLED.setBrightness()` applied before encoding

### 2. DMA Safety

- [ ] DMA buffers allocated with `heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)`
- [ ] DMA buffers are 4-byte aligned (use `__attribute__((aligned(4)))` or `heap_caps_aligned_alloc`)
- [ ] DMA buffer lifetime extends beyond transfer completion (no premature free)
- [ ] DMA descriptors reside in internal SRAM (not PSRAM/SPIRAM)
- [ ] Cache coherence handled: `esp_cache_msync()` called or memory marked non-cacheable for DMA regions
- [ ] Double-buffering used where needed to allow CPU-side encoding to overlap with DMA transmission
- [ ] DMA completion callback or semaphore used to signal transfer end (not busy-wait polling)
- [ ] Buffer sizes within hardware limits (RMT: 64 items/channel, I2S/SPI: configurable block size)

### 3. FreeRTOS Safety

- [ ] No blocking calls (`vTaskDelay`, `xSemaphoreTake` with timeout) inside ISRs
- [ ] ISR functions use `FromISR` API variants only (`xSemaphoreGiveFromISR`, `xQueueSendFromISR`)
- [ ] ISR handlers marked `IRAM_ATTR` (ESP32) or `__attribute__((section(".iram1")))` (other)
- [ ] No `malloc`, `new`, `fl::vector` allocation inside ISRs
- [ ] No `printf`, `FL_DBG`, `ESP_LOGI` or logging inside ISRs
- [ ] No flash access from ISRs (all ISR code and data must be in IRAM/DRAM)
- [ ] Shared state between ISR and task protected with critical section or ISR-safe primitives
- [ ] Task stack sizes adequate for FastLED usage (show() + encoding can be stack-heavy)
- [ ] No unbounded spin-wait loops without `taskYIELD()` (causes watchdog reset)
- [ ] Watchdog not starved during long `show()` calls with high LED counts

### 4. RMT-Specific Checks (WS2812/SK6812/APA106)

- [ ] RMT channel count within hardware limits (ESP32: 8, S2: 4, S3/C3/C6/H2: 4)
- [ ] RMT resolution set correctly for target LED protocol (typically 10ns for WS2812)
- [ ] RMT item encoding matches LED chipset timing spec (T0H, T0L, T1H, T1L, reset)
- [ ] RMT encoder (`rmt_encoder_t`) properly initialized and not shared between channels
- [ ] RMT callback or event queue used for completion, not polling
- [ ] RMT loop mode disabled (LED output is one-shot, not continuous)
- [ ] RMT end-of-frame reset pulse duration meets minimum spec (≥50µs for WS2812)

### 5. I2S/SPI-Specific Checks (APA102/SK9822/HD107)

- [ ] SPI clock rate set correctly for target LED chipset (APA102 max: 20MHz typical)
- [ ] SPI word size matches encoding (32-bit for APA102, 8-bit for raw SPI)
- [ ] I2S sample rate matches LED bit rate requirement
- [ ] SPI/I2S DMA buffer count adequate (≥2 for double buffering)
- [ ] SPI/I2S CS/CLK/DATA pins not conflicting with flash (GPIO 6-11 forbidden on ESP32)
- [ ] SPI transaction callback used to notify completion (not blocking transmit)

### 6. PARLIO-Specific Checks (ESP32-S3 Parallel Output)

- [ ] PARLIO clock source and divider correctly configured for target bit rate
- [ ] Data bus width matches LED strip count (1, 2, 4, 8, or 16 parallel strips)
- [ ] PARLIO DMA buffer aligned to cache line boundaries
- [ ] Output enable pin wired or bypassed correctly

### 7. Memory Management

- [ ] LED CRGB buffer allocation checked for NULL (large strips can exhaust heap)
- [ ] No large stack-allocated arrays in show() path (> 512 bytes on stack is risky)
- [ ] PSRAM usage documented when LED buffers are placed in SPIRAM
- [ ] Free heap monitored at startup and logged (alert if < 10KB before show())
- [ ] No memory leaks in driver initialization error paths
- [ ] String/buffer operations have bounds checking

### 8. Peripheral Pin Safety

- [ ] GPIO 6-11 not used for any output (ESP32 internal flash pins)
- [ ] GPIO 34-39 not used for output (input-only on original ESP32)
- [ ] Strapping pins (0, 2, 12, 15) not used without awareness of boot implications
- [ ] ADC2 channels not used alongside Wi-Fi (GPIO 0, 2, 4, 12-15, 25-27)
- [ ] Pin assignments match board schematic or documented LED strip wiring

### 9. Error Handling

- [ ] All ESP-IDF API return values checked (esp_err_t, esp_err check macros)
- [ ] Driver initialization failures reported with meaningful error messages
- [ ] Graceful degradation: driver failure doesn't crash the whole application
- [ ] Timeout values defined for all blocking operations
- [ ] Error paths clean up allocated resources (channels, DMA buffers, callbacks)

### 10. Code Quality & Maintainability

- [ ] Platform guards (`#ifdef FASTLED_ESP32`, `#ifdef CONFIG_IDF_TARGET_ESP32S3`, etc.) correct
- [ ] No magic numbers — timing constants named and commented with source (datasheet)
- [ ] Driver variant selection at compile time, not runtime (no dynamic dispatch overhead)
- [ ] Header includes minimal (no platform headers leaked into public FastLED API)
- [ ] Porting guide updated if new platform port added (see `PORTING.md`)

## Output Format

```markdown
# ESP32 Architecture Review Report

## Scope
- **Files reviewed**: [list]
- **Driver type**: [RMT / I2S / SPI / PARLIO / Other]
- **Target chip**: [ESP32 / ESP32-S2 / ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2]

## Overall Assessment
[PASS / PASS WITH NOTES / FAIL]

## Critical Issues (must fix before merge)
- [ ] [Issue description + file:line reference]

## Warnings (should fix)
- [ ] [Issue description]

## Recommendations (consider for future)
- [ ] [Suggestion]

## Checklist Results
[Filled checklist from sections above — mark each PASS / FAIL / N/A]
```

## Common FastLED ESP32 Architecture Anti-Patterns

| Anti-Pattern | Problem | Fix |
|-------------|---------|-----|
| `show()` in ISR | Blocks ISR with long DMA wait | Move to task, use semaphore from ISR |
| Stack-allocated encode buffer | Stack overflow on high LED counts | Use heap or static allocation |
| Polling DMA completion | Wastes CPU, may block IDLE | Use DMA completion callback/semaphore |
| Shared RMT channel for multiple strips | Data corruption | One channel per strip |
| `malloc()` without MALLOC_CAP_DMA | DMA transfer fails silently | Always use `heap_caps_malloc` for DMA buffers |
| Timing constants without datasheet reference | Hard to audit or adapt | Add comments citing datasheet section |
| Runtime color order switch | Branch overhead per LED | Use compile-time template parameter |
