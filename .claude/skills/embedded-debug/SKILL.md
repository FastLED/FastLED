---
name: embedded-debug
description: Firmware crash analysis, stack trace decoder, and register dump interpreter for ESP32/ARM/AVR platforms. Use when debugging device crashes, panics, guru meditation errors, hard faults, or analyzing core dumps.
argument-hint: <crash output or description of the issue>
context: fork
agent: embedded-debug-agent
---

Analyze firmware crash output, decode stack traces, and diagnose embedded system failures.

$ARGUMENTS

## What This Skill Does

1. **Crash Analysis**: Parse panic handlers, guru meditation errors, hard faults, watchdog resets
2. **Stack Trace Decoding**: Decode raw addresses to function names using ELF binaries
3. **Register Dump Interpretation**: Analyze CPU register state at crash time
4. **Root Cause Identification**: Correlate crash data with source code to find the bug
5. **Fix Recommendations**: Suggest specific code changes to prevent the crash

## Supported Platforms

| Platform | Crash Types |
|----------|-------------|
| ESP32 (all variants) | Guru Meditation, LoadProhibited, StoreProhibited, InstrFetchProhibited, watchdog reset, brownout, cache errors |
| ARM Cortex-M (STM32, Teensy, nRF) | HardFault, MemManage, BusFault, UsageFault, watchdog reset |
| AVR (Arduino Uno/Mega) | Stack overflow (silent corruption), watchdog reset, undefined opcode |
| RISC-V (ESP32-C3/C6/H2) | Illegal instruction, load/store fault, breakpoint |

## How To Use

### With crash output
```
/embedded-debug
Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled.
Core 0 register dump:
PC      : 0x400d1234  PS      : 0x00060030  A0      : 0x800d5678
...
```

### With a description
```
/embedded-debug Device reboots every 30 seconds, serial shows "rst:0x3 (SW_RESET)"
```

### With a core dump
```
/embedded-debug Analyze the core dump from the last crash
```

## What You'll Get

- Decoded stack trace with source file and line numbers
- Explanation of the crash type and what triggered it
- Identification of the faulty code path
- Specific fix recommendations with code examples
- Prevention strategies (stack size tuning, watchdog config, null checks)

## ESP32 Crash Quick Reference

### Exception Type → Root Cause

| Exception | EXCVADDR hint | Likely Root Cause |
|-----------|--------------|------------------|
| `LoadProhibited` | ~0x00000000 | NULL pointer dereference (uninitialized LED strip?) |
| `LoadProhibited` | Other | Read from freed or out-of-bounds memory |
| `StoreProhibited` | Any | Write to read-only or invalid memory address |
| `InstrFetchProhibited` | Any | Jump to invalid/NULL function pointer or corrupt vtable |
| `IllegalInstruction` | Any | Stack overflow corrupting code, or corrupt heap |
| `IntegerDivideByZero` | N/A | Division by zero in LED index math |
| `Unaligned` | Any | Misaligned memory access (DMA buffer not 4-byte aligned) |
| `Cache disabled but cache memory range accessed` | Any | ISR accessing flash — add `IRAM_ATTR` to ISR function |

### Key Register Meanings (ESP32 Xtensa)

| Register | Meaning |
|----------|---------|
| `PC` | Program Counter — where the crash happened |
| `EXCVADDR` | Address that caused the fault (NULL pointer, invalid write target) |
| `A1` | Stack pointer — compare to task stack base to detect overflow |
| `A0` | Return address — calling function |

### ISR Violation Pattern

**Symptoms**: `Cache disabled but cache memory range accessed` or crash inside FreeRTOS API

**Cause**: FastLED ISR handler calling blocking or flash-accessing code.

**Diagnosis**: Look for crash inside `xSemaphoreTake`, `vTaskDelay`, `printf`, or any non-ISR-safe function.

**Fix**:
1. Add `IRAM_ATTR` to all ISR-related functions
2. Replace blocking calls with ISR-safe variants (`xSemaphoreGiveFromISR`, `xQueueSendFromISR`)
3. Move logging out of ISR to a deferred task

### Watchdog Reset Pattern

**Symptoms**: `rst:0x7 (TG0WDT_SYS_RESET)` or `rst:0x8 (TG1WDT_SYS_RESET)`

**Cause**: Task or interrupt handler not yielding — common with:
- FastLED `show()` blocking on DMA in a task with watchdog enabled
- High LED counts causing long encoding loops without yield
- Spinning in a loop without `vTaskDelay(0)` or `taskYIELD()`

**Fix**:
1. Call `esp_task_wdt_reset()` in long loops, or
2. Move FastLED `show()` to a dedicated task exempt from watchdog, or
3. Reduce LED count to bring encoding time under watchdog timeout

### Stack Overflow Pattern

**Symptoms**: `StackOverflow` + task name, or `IllegalInstruction` with A1 near stack limit

**Diagnosis**: Check task stack size in `xTaskCreate` or `xTaskCreatePinnedToCore`. FastLED `show()` with large LED arrays can use significant stack.

**Fix**: Increase task stack size (minimum 4096 bytes for tasks calling FastLED show with encoding)

### Memory Corruption Pattern

**Symptoms**: `IllegalInstruction` at random address, corrupted register values, intermittent crashes

**Diagnosis**: Enable diagnostics:
```c
// In sdkconfig or menuconfig:
CONFIG_HEAP_POISONING_COMPREHENSIVE=y
CONFIG_HEAP_TASK_TRACKING=y
CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y
```

**Common FastLED Cause**: Writing past end of CRGB array (index out of bounds in user sketch)

### Backtrace Decoding Commands

```bash
# ESP32 (Xtensa)
xtensa-esp32-elf-addr2line -pfiaC -e build/firmware.elf 0xADDR1 0xADDR2

# ESP32-S3
xtensa-esp32s3-elf-addr2line -pfiaC -e build/firmware.elf 0xADDR1

# ESP32-C3/C6 (RISC-V)
riscv32-esp-elf-addr2line -pfiaC -e build/firmware.elf 0xADDR1

# Live decode via idf.py
idf.py -p /dev/ttyUSB0 monitor
```
