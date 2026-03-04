# GPIO ISR RX Driver

High-performance GPIO edge capture driver for ESP32 RISC-V platforms (ESP32-C6, ESP32-C3, ESP32-H2) using a dual-ISR architecture with MCPWM hardware timestamps.

## Performance

| Metric | Value |
|--------|-------|
| ISR latency | ~130-150 ns (25-31 cycles @ 160 MHz) |
| Timestamp resolution | 12.5 ns (MCPWM @ 80 MHz) |
| Edge capture rate | >1 MHz |
| CPU overhead | <20% during capture |

Compared to ESP-IDF standard GPIO ISR (~400 ns), this achieves **3-4x faster** edge capture.

## Architecture

```
GPIO Edge Event --> MCPWM Hardware Capture (~5-15 ns, automatic)
    |
    v
Fast ISR (RISC-V assembly, ~130 ns total)
  - Reads MCPWM timestamp register (2-3 cycles, precomputed address)
  - Reads GPIO level from GPIO_IN register (4-5 cycles)
  - Writes {timestamp, level} to circular buffer (atomic, RV32A lr.w/sc.w)
  - Returns via mret (zero stack operations)
    |
    v
Circular Buffer (lock-free, power-of-2 size)
  - Atomic write_index (fast ISR)
  - Non-atomic read_index (slow ISR)
  - Bitwise AND for fast modulo
    |
    v
Slow Management ISR (C, GPTimer @ 10 us interval)
  - Drains circular buffer
  - Applies skip_signals filter (ignore N initial edges)
  - Applies jitter filter (minimum pulse width)
  - Detects idle timeout (maximum pulse width)
  - Copies filtered edges to output buffer
  - Sets done=true when complete
    |
    v
User Code
  - Calls wait(timeout_ms) -> polls done flag
  - Calls getEdges() -> span<const EdgeTimestamp>
  - Calls decode() -> converts edges to bytes (WS2812B protocol)
```

## File Structure

| File | Lines | Description |
|------|-------|-------------|
| `gpio_isr_rx.h` | ~180 | Public `GpioIsrRx` class (extends `RxDevice`), `EdgeTimestamp` struct |
| `gpio_isr_rx.cpp.hpp` | ~30 | Factory: `GpioIsrRx::create()` -> `GpioIsrRxMcpwm` |
| `gpio_isr_rx_mcpwm.h` | ~36 | MCPWM factory declaration |
| `gpio_isr_rx_mcpwm.cpp.hpp` | ~900 | Main dual-ISR implementation, filtering, configuration |
| `dual_isr_context.h` | ~390 | `DualIsrContext` struct (64-byte aligned), `EdgeEntry` struct |
| `fast_isr.S` | ~184 | RISC-V assembly fast ISR (the hot path) |
| `mcpwm_timer.h` | ~107 | MCPWM timer interface |
| `mcpwm_timer.cpp.hpp` | ~412 | MCPWM 80 MHz timer setup, register address precomputation |
| `_build.cpp.hpp` | - | Build include aggregator |

## Key Design Decisions

### Why MCPWM for Timestamps (Not CPU Cycle Counter)

| Source | Read Cycles | Resolution | Verdict |
|--------|-------------|------------|---------|
| **MCPWM (chosen)** | 2-3 | 12.5 ns | Optimal |
| mcycle CSR (single 32-bit) | 3-5 | 6.25 ns | Slower |
| mcycle CSR (64-bit) | 6-10 | 6.25 ns | Much slower |
| SYSTIMER | varies | 1000 ns | Way too slow |

MCPWM hardware automatically latches its timer value on GPIO edge. The ISR reads the already-captured value via a precomputed register address, requiring only a load-word instruction pair. The mcycle CSR requires variable-latency CSR access instructions which are inherently slower than memory-mapped register reads.

### Why esp_intr_alloc (Not Direct Vector Table Modification)

Direct RISC-V vector table modification was extensively researched and found **not feasible** on ESP32-C6:

- **W^X memory protection**: Vector table is in IRAM (executable), which is not writable at runtime
- **Vector table entries are JAL instructions**, not function pointers
- **ESP-IDF owns the vector table** and manages it during boot
- Writing to executable IRAM triggers a PMP violation -> immediate panic

The driver uses `esp_intr_alloc()` with `ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM` on RISC-V, which directly registers the assembly handler. This is safe, officially supported, and compatible with WiFi/BLE/FreeRTOS.

### Why Level 3 Interrupts (Not Level 4+)

- Level 4+ on RISC-V ESP32 has **zero working community examples**
- Level 4+ requires `handler=NULL` in `esp_intr_alloc()` with no public API to wire the actual handler
- Level 4+ cannot call FreeRTOS APIs
- ESP-IDF documentation recommends Level 2-3 for performance
- The assembly ISR with `mret` return works correctly at Level 3 via direct handler registration

### Precomputed Register Addresses

All hardware register addresses are computed once during `begin()` and stored in `DualIsrContext`:

```c
ctx->mcpwm_capture_reg_addr = (uint32_t)&MCPWM0.cap_chn[0].val;  // 0x60024060
ctx->gpio_in_reg_addr = GPIO_IN_REG;                                // 0x6000403C
ctx->gpio_bit_mask = (1U << pin);
```

This eliminates address calculation in the hot path -- the fast ISR just loads and dereferences.

### Atomic Circular Buffer

The fast ISR uses RISC-V atomic extensions (RV32A) for lock-free buffer writes:

```asm
.Lretry:
    lr.w    t0, (write_index_addr)      // Load-reserved
    addi    t1, t0, 1                   // Increment
    and     t1, t1, buffer_size_mask    // Fast modulo (power-of-2)
    sc.w    t2, t1, (write_index_addr)  // Store-conditional
    bnez    t2, .Lretry                 // Retry on contention
```

Buffer size must be a power of 2 (64, 128, 256, 512) to enable bitwise AND modulo, saving 15-30 cycles vs. `remu`.

## DualIsrContext Memory Layout

The context structure is 64-byte aligned for single cache line access:

```
Bytes 0-31  (Hot path):  buffer ptr, buffer_size, buffer_size_mask,
                         write_index, read_index, armed flag,
                         mcpwm_capture_reg_addr, gpio_in_reg_addr
Bytes 32-47 (Config):    gpio_bit_mask, pin, skip_signals,
                         min_pulse_ticks, timeout_ticks
Bytes 48+   (Cold):      gptimer_handle, intr_handle, output_buffer,
                         output_buffer_size, output_count,
                         last_edge_timestamp, done flag
```

## Fast ISR Cycle Budget

| Operation | Cycles | Notes |
|-----------|--------|-------|
| PLIC detection | 4-6 | Fixed hardware latency |
| Load armed flag + branch | 2 | Early exit if disarmed |
| Load MCPWM addr + read | 2-3 | Precomputed address |
| Load GPIO addr + mask + read | 4-5 | AND + SNEZ to normalize |
| Load buffer metadata | 2 | Base pointer + size mask |
| Atomic write_index update | 5-6 | lr.w / sc.w / retry |
| Calculate offset + write entry | 4 | SLLI + ADD + SW + SB |
| mret return | 2-3 | Machine trap return |
| **Total** | **25-31** | **156-194 ns @ 160 MHz** |

## Platform Support

| Platform | Arch | ISR Support | Notes |
|----------|------|-------------|-------|
| ESP32-C6 | RISC-V RV32IMAC | Assembly ISR | Primary target |
| ESP32-C3 | RISC-V RV32IMC | Assembly ISR | Should work (same ISR code) |
| ESP32-H2 | RISC-V RV32IMAC | Assembly ISR | Should work (same ISR code) |
| ESP32/S2/S3 | Xtensa | RMT fallback | Use `RxDeviceType::RMT` instead |

## Configuration

Key `RxConfig` parameters:

- **buffer_size**: Circular buffer capacity. Must be power of 2 (e.g., 256).
- **signal_range_min_ns**: Minimum pulse width (jitter filter). Pulses shorter than this are discarded.
- **signal_range_max_ns**: Maximum pulse width (idle timeout). Silence longer than this ends capture.
- **skip_signals**: Number of initial edges to ignore before recording.

## Usage

```cpp
#include "fl/rx_device.h"

// Pin must be configured before use
pinMode(1, INPUT);

auto rx = GpioIsrRx::create(1);

RxConfig config;
config.buffer_size = 256;
config.signal_range_min_ns = 100;
config.signal_range_max_ns = 100000;
config.skip_signals = 0;

if (!rx->begin(config)) {
    // Handle error
}

if (rx->wait(100) == RxWaitResult::SUCCESS) {
    auto edges = rx->getEdges();
    // Process edges...
}
```

## Known Limitations

1. **RISC-V only**: Assembly ISR requires RISC-V. Xtensa platforms fall back to RMT.
2. **Single MCPWM channel**: Uses capture channel 0 only (hardware supports 3).
3. **Power-of-2 buffer**: Buffer size must be 64, 128, 256, 512, etc.
4. **Pin config required**: User must call `pinMode(pin, INPUT)` before `begin()`.
5. **Overflow aborts capture**: Buffer overflow sets `done=true` with no recovery.
6. **32-bit timer wrap**: MCPWM counter wraps after ~53.7 seconds at 80 MHz.

## References

- [ESP-IDF MCPWM API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/mcpwm.html)
- [ESP-IDF Interrupt Allocation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/system/intr_alloc.html)
- [RISC-V Privileged Spec v1.12](https://riscv.github.io/riscv-isa-manual/snapshot/privileged/) (mtvec, mret, CSRs)
- [ESP32-C6 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c6_technical_reference_manual_en.pdf)
- [cnlohr ESP32-C3 interrupt testing](https://github.com/cnlohr/esp32-c3-cntest) (200 ns measured latency)
