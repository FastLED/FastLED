---
name: esp-perf-counters
description: Count retired instructions (and cycles) for a code path on ESP32-C3/C6/H2 RISC-V cores using Espressif's custom performance counter CSRs (PCER 0x7E0, PCMR 0x7E1, PCCR 0x7E2). Use when judging a per-change optimisation of FastLED.show() or a driver hot path on a RISC-V ESP32, when wall-time results are noisy between builds, when computing IPC, or when attributing instructions to phases (encode, rmt_transmit, poll). Covers the save/restore rule (ESP-IDF shares the counter), the minstret/mcycle trap, and running it on the bench via bash autoresearch.
argument-hint: [function or code path to measure, optional]
---

# Counting instructions/cycles on ESP32 RISC-V

$ARGUMENTS

## When to use this

Wall time cannot judge a single optimisation on these chips. Code-layout changes
alone move `FastLED.show()` wall time on the ESP32-C6 by 10-20 µs between builds,
which is more than a typical 5-10% optimisation. #5362 was a wall-time "win" that
turned out to be layout noise and was closed.

The retired-instruction count does not depend on layout and stays stable between
builds. For example, #5364 took `show()` from 14,439 to 12,953 instructions.

**Rule:** accept or reject a per-change optimisation on **instruction count**. Use
wall time only to track cumulative progress across many changes.

Caveat: with IPC around 0.4, a change can retire fewer instructions and still
add stalls, for example extra loads. If a change alters the memory-access pattern,
also check that wall time does not regress beyond the 10-20 µs layout-noise band.
Use the median of several runs for that check.

## Hardware facts

| Chip | Counter | Detect with (`src/platforms/esp/is_esp.h`) |
|------|---------|--------------------------------------------|
| ESP32-C3 / C6 / H2 (RISC-V) | Espressif custom M-mode counter | `FL_IS_ESP_32C3` / `FL_IS_ESP_32C6` / `FL_IS_ESP_32H2` |
| ESP32 / S2 / S3 (Xtensa) | Different hardware, **not covered here** | |

CSRs (one counter, one event at a time):

- `PCER` **0x7E0**: event select bitmask
- `PCMR` **0x7E1**: mode register. Bit 0 enables counting.
- `PCCR` **0x7E2**: the count

PCER event bits, from the C3/C6 TRM:

| Bit | Event | Status |
|-----|-------|--------|
| 0 | cycles | **verified on hardware** |
| 1 | instructions retired | **verified on hardware** |
| 2 | load hazards | TRM only, unverified |
| 3 | jump hazards | TRM only, unverified |
| 4 | idle | TRM only, unverified |
| 5 | loads | TRM only, unverified |
| 6 | stores | TRM only, unverified |
| 7 | unconditional jumps | TRM only, unverified |
| 8 | branches | TRM only, unverified |
| 9 | taken branches | TRM only, unverified |
| 10 | compressed instructions | TRM only, unverified |

Be careful with the unverified events. One scratch firmware rotated events 0-3,
one per `show()`, and the board crashed and re-enumerated. The cause is unknown.
Try one new event at a time and confirm that the board survives.

## Pitfalls

1. **Do not read `minstret` / `mcycle`.** The standard RISC-V counter CSRs trap
   on these cores. A firmware that read them crashed: the RPC returned `None` and
   the board rebooted and re-enumerated. Use only 0x7E0-0x7E2.
2. **ESP-IDF shares the counter.** `esp_cpu_get_cycle_count()` reads the same
   PCCR, configured to count cycles. Code that switches the event must save
   PCER/PCMR/PCCR and restore them afterwards. When restoring PCCR, add the
   cycles that elapsed so that IDF's cycle count keeps moving forward.
   Between `begin()` and `end()`, `esp_cpu_get_cycle_count()` (and anything
   built on it) returns instructions, not cycles. Do not take cycle-based
   timestamps or deltas inside that window. The restore cannot fix those reads
   after the fact.
3. **Only one event at a time.** Count instructions with PCCR. Derive cycles as
   wall-time µs × `getCpuFrequencyMhz()`. Report `IPC = instructions / cycles`.
4. **Probe overhead.** Each `csrr` and its bookkeeping adds a few instructions.
   This matters when you attribute instructions to small phases. Subtract an
   empty-probe baseline if the phase is tiny.

## Pattern

```cpp
#if defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2)

struct InstrCounter {
    uint32_t pcer, pcmr, pccr;   // saved IDF state
    uint32_t t0_us;

    void begin() {
        asm volatile("csrr %0, 0x7E0" : "=r"(pcer));
        asm volatile("csrr %0, 0x7E1" : "=r"(pcmr));
        asm volatile("csrr %0, 0x7E2" : "=r"(pccr));
        t0_us = micros();
        asm volatile("csrw 0x7E0, %0" :: "r"(1u << 1));  // event: instructions retired
        asm volatile("csrw 0x7E2, zero");                // reset count
        asm volatile("csrw 0x7E1, %0" :: "r"(1u));       // enable counting
    }

    // Returns instructions retired since begin(), then restores IDF's cycle counter.
    uint32_t end() {
        uint32_t instr;
        asm volatile("csrr %0, 0x7E2" : "=r"(instr));
        uint32_t elapsed_us = micros() - t0_us;
        uint32_t restored = pccr + elapsed_us * getCpuFrequencyMhz();
        asm volatile("csrw 0x7E0, %0" :: "r"(pcer));
        asm volatile("csrw 0x7E2, %0" :: "r"(restored));
        asm volatile("csrw 0x7E1, %0" :: "r"(pcmr));
        return instr;
    }
};
#endif
```

Usage: `c.begin(); FastLED.show(); uint32_t n = c.end();`. Use `micros()` for
`cycles = elapsed_us * getCpuFrequencyMhz()`. Skip the sample if `cycles` is 0.
Otherwise report `(float)n / cycles` as IPC, using floating-point division.

**Per-phase profiling:** inside the code under test, read 0x7E2 at phase
boundaries (for example encode, `rmt_transmit`, poll) and take the differences.
Do not reset it at the boundaries, because `end()` would then return only the last
phase. Remember the probe overhead from pitfall 4.

## Reference numbers (ESP32-C6, `show()` via RMT)

- About 12.4k instructions over about 34k cycles: **IPC about 0.4**, so about 60%
  of cycles are stalls.
- Moving the hot path to IRAM made no difference, so the stalls do **not** come
  from instruction fetch from flash.

## Running it on the bench

- AutoResearch's `timingDriftTest` (`examples/AutoResearch/AutoResearchTimingDrift.h`)
  is gaining an `InstrCounter` under **FastLED#5376**. It reports
  `show_instr_total` / `show_instr_count` in the RPC result. Until #5376 merges,
  check whether the struct exists before you rely on it.
- Host-side reporting is also pending under #5376.
  `ci/autoresearch/show_latency.py` on master prints only timing fields. The
  #5376 change makes it print `instr/show` and IPC when the firmware supplies
  those fields.
- Find the port with `fbuild port scan`, then run:
  `bash autoresearch esp32c6 --legacy --rmt --upload-port <port>`
- Take an fbuild lease when you share the board with other agents.
- Flash only through `fbuild deploy`, which `bash autoresearch` calls. Never run
  ad-hoc flash tools (see CLAUDE.md, "Deployment is fbuild's job").

## Xtensa (ESP32 / S2 / S3)

This skill does not cover these chips. They use the Xtensa `CCOUNT` special
register for cycles, and the performance monitor (PM) registers accessed via ERI
for other events. Nothing here has been verified on Xtensa. Check the Xtensa ISA
and the chip TRM before writing code for these chips, and do not reuse the
RISC-V CSR numbers.
