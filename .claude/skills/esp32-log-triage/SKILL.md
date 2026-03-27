---
name: esp32-log-triage
description: Parse and classify ESP32 serial log output to identify FastLED-related errors, RMT/I2S/SPI driver faults, timing violations, RTOS issues, and crash signatures. Use when debugging unexpected device behavior, boot failures, or LED output problems on ESP32.
argument-hint: <paste serial log output or describe the symptom>
context: fork
agent: esp32-log-triage-agent
---

Systematically analyze ESP32 serial log output to identify and classify issues, with special focus on FastLED driver faults, LED timing errors, and RTOS problems.

$ARGUMENTS

## What This Skill Does

1. **Log Structure Parsing**: Identify ESP-IDF log levels, tags, timestamps, and reset reasons
2. **FastLED-Specific Pattern Detection**: Find RMT driver faults, I2S/SPI timing violations, LED buffer corruption
3. **Crash Signature Classification**: Match Guru Meditation types to root causes
4. **RTOS Health Check**: Detect watchdog resets, task starvation, and priority issues
5. **Boot Sequence Analysis**: Trace initialization failures, missing peripherals, partition errors
6. **Triage Report Generation**: Produce structured root cause hypothesis and next steps

## ESP-IDF Log Format

```
[LEVEL][TAG] (TIMESTAMP) MESSAGE
```

Severity levels (E > W > I > D > V):
- `E` — Error (critical failures, always investigate)
- `W` — Warning (potential issues, context-dependent)
- `I` — Info (normal operation markers)
- `D` — Debug (detailed tracing, enabled per component)
- `V` — Verbose (maximum detail)

## FastLED-Specific Patterns to Watch

### RMT Driver (WS2812/SK6812/APA106 via RMT)
```
E (rmt): rmt_ll_write_memory: ...         → RMT buffer write fault
E (RMT): install_rmt failed               → RMT channel allocation failure
W (RMT): RMT timeout                      → LED strip not responding
E (FastLED): RMT encoder error            → Timing encoding failed
rmt: rmt_config_err                       → Wrong RMT clock or resolution
```

### I2S Driver (APA102/SK9822 high-speed or parallel output)
```
E (I2S): i2s_driver_install failed        → I2S DMA setup error
E (I2S): DMA buffer not enough            → Increase DMA buffer count
W (I2S): i2s_write timeout                → DMA stall, check task priority
```

### SPI Driver (APA102, SK9822, HD107)
```
E (SPI): spi_bus_initialize failed        → SPI bus conflict or pin issue
E (SPI): spi_device_transmit failed       → Transmission error
W (SPI): DMA not aligned                  → Buffer alignment issue for DMA
```

### PARLIO Driver (ESP32-S3 parallel output)
```
E (PARLIO): parlio_tx_unit_init failed    → PARLIO DMA setup error
E (PARLIO): parlio clock config failed    → Clock source conflict
```

### LED Buffer / Memory
```
E (heap): Failed to alloc N bytes         → Heap exhaustion (too many LEDs?)
W (heap): Heap corruption detected        → Memory overwrite, check LED array bounds
assert failed: led_count > 0             → Zero-length LED strip configured
```

## Reset Reason Classification

```
rst:0x1 (POWERON_RESET)     → Normal power-on, no concern
rst:0x3 (SW_RESET)          → Software reset (intentional or crash recovery)
rst:0x7 (TG0WDT_SYS_RESET) → Task watchdog: show() blocking too long?
rst:0x8 (TG1WDT_SYS_RESET) → Interrupt watchdog: ISR took too long
rst:0xf (BROWNOUT_RESET)    → Insufficient power (LEDs drawing too much current?)
rst:0xc (SW_CPU_RESET)      → Panic/abort → look for Guru Meditation above
```

## Crash Signature Reference

| Pattern | Likely Cause |
|---------|-------------|
| `LoadProhibited` + EXCVADDR ~0x0 | NULL pointer (uninitialized LED strip?) |
| `StoreProhibited` | Write to read-only or invalid memory |
| `InstrFetchProhibited` | Jump to invalid address (corrupt function pointer) |
| `IllegalInstruction` | Stack overflow or heap corruption |
| `IntegerDivideByZero` | Division by zero in LED math |
| `StackOverflow` + task name | Task stack too small for FastLED show() |
| `abort()` called | Assertion failure, check log lines above |
| `Cache disabled but cache memory range accessed` | ISR accessing flash (IRAM_ATTR missing) |

## FastLED-Specific Watchdog Patterns

FastLED's `show()` call can trigger watchdog resets if:
- LED count is very high (>1000 LEDs, long DMA transfer)
- `show()` is called from a task with watchdog enabled
- RMT/I2S wait loop runs without yielding

Look for patterns like:
```
Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
 - IDLE (CPU 0)
```
Combined with FastLED show() in the call stack — this means the main task is blocking the idle task.

## Memory Patterns

```
Free heap: XXXX bytes                     → Monitor across frames; declining = leak
DRAM free: XXXX bytes                     → Internal RAM; FastLED LED buffers go here
SPIRAM free: XXXX bytes                   → PSRAM; check if LED buffers placed here
```

FastLED LED buffers: each CRGB is 3 bytes. 1000 LEDs = 3KB heap per strip.

## Triage Steps

### Step 1: Identify Reset Reason
Find the first `rst:` line in the log — this tells you how the device started.

### Step 2: Extract Crash Signature
Search for `Guru Meditation`, `LoadProhibited`, `StoreProhibited`, `abort()` — decode the backtrace.

### Step 3: Classify the Issue

| Category | Indicators |
|----------|-----------|
| Power issue | Brownout resets, unstable readings |
| FastLED RMT fault | RMT errors, `rmt_config_err`, `install_rmt failed` |
| Heap exhaustion | `Failed to alloc`, declining free heap |
| Watchdog | TG0WDT/TG1WDT reset, show() blocking |
| ISR violation | `Cache disabled but cache memory range accessed` |
| Stack overflow | `StackOverflow` + task name |
| NULL pointer | EXCVADDR ~0x00000000 |

### Step 4: Check FastLED Initialization Sequence

A healthy FastLED ESP32 boot looks like:
```
I (xxx) FastLED: Registered N LEDs on pin Y
I (xxx) rmt: rmt channel N ... successfully
```

If either of these is missing, FastLED never initialized correctly.

### Step 5: Decode Backtrace (if crash)

```bash
# Extract backtrace addresses from log
xtensa-esp32-elf-addr2line -pfiaC -e build/firmware.elf 0xADDR1 0xADDR2

# Or use idf.py monitor for live decoding
idf.py -p /dev/ttyUSB0 monitor
```

## Output Format

```markdown
# ESP32 Log Triage Report

## Summary
- **Reset reason**: [from boot log]
- **Issue type**: [Power / RMT fault / Heap / Watchdog / ISR violation / Stack overflow / NULL pointer]
- **Severity**: [CRITICAL / HIGH / MEDIUM / LOW]
- **FastLED component affected**: [RMT / I2S / SPI / PARLIO / LED buffer / None]

## Key Events (chronological)
1. [timestamp] [event]
2. [timestamp] [event]

## Error Patterns
- `[pattern]`: [count] occurrences — [interpretation]

## Root Cause Hypothesis
[Most likely cause based on evidence]

## Recommended Next Steps
1. [Action — specific to FastLED or ESP32]
2. [Action]

## Raw Evidence
[Relevant log excerpts with timestamps]
```

## Quick Reference Tips

- **Brownout resets with many LEDs**: Add current limiting or larger capacitor on 5V rail — LEDs draw ~60mA each at full white
- **Watchdog with RMT**: Call `esp_task_wdt_reset()` around show() or move FastLED show() to a dedicated task without watchdog
- **RMT channel allocation failure**: ESP32 has 8 RMT channels; check for conflicts with other RMT users (IR remote, etc.)
- **Heap allocation failure for LED buffer**: On memory-constrained boards, reduce LED count or place buffer in PSRAM with `MALLOC_CAP_SPIRAM`
- **Cache disabled ISR fault**: FastLED ISR handlers must be in IRAM — check `IRAM_ATTR` on all ISR functions
