# Binary Size Scorecards

Per-issue provenance for the readings in
[binary-size-analysis.md](binary-size-analysis.md#current-per-board-readings):
the fbuild version, toolchain, and which change moved the number. Read the
board's own entry only; the table is for citations, not for orientation.

[Rules for quoting a number](binary-size-analysis.md#rules-for-quoting-a-number)
apply to every figure here.

### #4788 — removed 32 B physical RAM from matched Uno Blink (168 -> 136; flash
3762 -> 3730; 3.10.3 is 131 / 3784): 8 controller slots on Uno instead of 16
(16 B), `setSpiBus` virtual only on ESP32 and the fixed-white policy as a bit
next to `mEnabled` instead of a virtual (8 B over two vtables), `constexpr`
`AtomicFake` so the wait-spin budget has no startup constructor (4 B), and
no weak `timer_millis` on classic ATmega (4 B). A follow-up reached 118 B /
3592 B flash, below 3.10.3: `size`, `beginShowLeds` and `endShowLeds` are not
virtual on AVR (12 B), no list tail pointer (2 B), a 16-bit FPS timestamp
(2 B) and the refresh cap stored in Hz (2 B).

### #4764 — no-Serial scorecard: the earlier published Uno Blink had Serial init and
a banner; dropping it saves 1208 B flash / 177 B RAM, separate from library
optimization. No-Serial Blink: 3.10.3 = 3784 B flash / 131 B RAM (matched
sketch); master a439902 = 3922 / 266, measured with repository
`examples/Blink` right after #4759 dropped its Serial, not the dashboard's
matched sketch, which is why it exceeds the 3762 / 168 "before #4788" figure
above; master after #4788 and its follow-up = 3592 / 118, and 3600 / 118 on
2026-10-30 master.
3.10.6 is not measured with the no-Serial sketch; never reuse its old
Serial-enabled numbers. The regression guard is
`tests/data/uno_blink_bloat_budget.json`, run with the `bash bloat` command
above.

### #4765 — attiny85 no-Serial scorecard: attiny85 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.37, AVR GCC 7.3.0-atmel3.6.1-arduino7,
ATTinyCore 1.5.2, F_CPU=8000000L. Image flash / attributed RAM went from the
recorded 2638 B flash ceiling (#4743/#4751) to 2574 / 160 after #4788, and to
2460 / 136 after its follow-up (AVR non-virtual size/begin/endShowLeds, no
tail pointer, 16-bit FPS timestamp, Hz refresh cap). 3.10.3 and 3.10.6
attiny85 numbers are not recorded with the no-Serial sketch here; never reuse
their old Serial-enabled measurements. `total_ram` is attributed RAM, not
physical RAM (see [AVR RAM semantics](binary-size-reference.md#avr-ram-semantics)). The regression guard is
`tests/data/attiny85_bloat_budget.json`, run with
`bash bloat attiny85 --build --budget tests/data/attiny85_bloat_budget.json`.

### #4762 — esp32c6 no-Serial scorecard: esp32c6 Blink (`examples/Blink`,
Serial-free since #4759) on Arduino-ESP32 3.3.5 / IDF 5.5.1. Image flash is
402674 B. Attributed RAM for the same ELF is 29027 B on fbuild 2.5.37 and
33478 B on fbuild 2.5.39; the increase is fbuild#1663 attributing weak vtable
owners (#4813 / PR #4814), not a code regression, and the image size is
identical. The earlier published 359834 / 18460 B (#4738) are board flash/RAM
from fbuild, not bloat-report image flash / attributed RAM, so they are not
comparable. 3.10.3 and 3.10.6 esp32c6 numbers are not recorded with the
no-Serial sketch here; never reuse their old Serial-enabled measurements.
`total_ram` is attributed RAM and omits singleton storage (ParlioEngine
enforces its own 2048-byte encoding-storage ceiling at compile time). The
regression guard is `tests/data/esp32c6_bloat_budget.json`, run with
`bash bloat esp32c6 --build --budget tests/data/esp32c6_bloat_budget.json`.

### #4766 — esp32dev no-Serial scorecard: esp32dev Blink (`examples/Blink`,
Serial-free since #4759) on Arduino-ESP32 3.3.11 / IDF 5.5.5 / Xtensa GCC
14.2.0. Current master on fbuild 2.5.45 (2026-10-30) measures 287671 B image
flash / 34091 B attributed RAM, under the budget below. The budget baseline
image flash is 313231 B. Attributed RAM for that ELF is 32618 B on
fbuild 2.5.37 and 34734 B on fbuild 2.5.39; the increase is fbuild#1663
attributing weak vtable owners (#4813 / PR #4814), not a code regression, and
the image size is identical. #4738 saved 452 B image flash / 148 B attributed
RAM versus 313683 / 34637. The published 310428 / 25892 B are board flash/RAM
from fbuild, not bloat-report image flash / attributed RAM, so they are not
comparable. 3.10.3 and 3.10.6 esp32dev numbers are not recorded with the
no-Serial sketch here; never reuse their old Serial-enabled measurements.
`total_ram` is attributed RAM. The regression guard is
`tests/data/esp32dev_bloat_budget.json`, run with
`bash bloat esp32dev --build --budget tests/data/esp32dev_bloat_budget.json`.

### #4767 — esp32s3 no-Serial scorecard: esp32s3 Blink (`examples/Blink`,
Serial-free since #4759) on Arduino-ESP32 3.3.11 / IDF 5.5.5 / Xtensa GCC
14.2.0. The recorded measurement, fbuild 2.5.49 with the UART console kept out
of output-only builds (#4990), is 320439 B image flash (CI run 41310165483).
The budget ceilings are 320503 B image flash (64 B link-layout slack) and
40016 B attributed RAM; the separate ratchet
`tests/data/esp32s3_bloat_baseline.txt` independently pins 320439.
Attributed RAM is not physical board RAM. Physical board RAM for this
esp32s3 Blink build has not been measured and remains open for #4767; do not
read 39147 B as physical RAM. Board flash/RAM from fbuild are not comparable with bloat-report image flash / attributed RAM. 3.10.3
and 3.10.6 esp32s3 numbers are not recorded with the no-Serial sketch here;
never reuse their old Serial-enabled measurements. The regression guard is
`tests/data/esp32s3_bloat_budget.json`, run with
`bash bloat esp32s3 --build --budget tests/data/esp32s3_bloat_budget.json`.

### #4761 — esp32c3 no-Serial scorecard: esp32c3 Blink (`examples/Blink`,
Serial-free since #4759). Image flash is 384692 B. Attributed RAM for the
same ELF is 24607 B on fbuild 2.5.37 and 26670 B on fbuild 2.5.39; the
increase is fbuild#1663 attributing weak vtable owners (#4813 / PR #4814),
not a code regression, and the image size is identical. The earlier
published 402670 / 68388 B (#4738) are board flash/RAM from fbuild, not
bloat-report image flash / attributed RAM, so they are not comparable.
Attributed RAM is not physical board RAM. 3.10.3 and 3.10.6 esp32c3 numbers
are not recorded with the no-Serial sketch here; never reuse their old
Serial-enabled measurements. #4982: the native ESP32 console (12b85b405)
had made every `fl::print`/`println` construct EspIO, so failure diagnostics
linked the SDK UART driver into no-Serial Blink (335592 / 27018 B). With the
plain-UART backend, output-only callers now write through the ROM UART until
the console is used (`begin`, `read`, `available`, `flush`, ...), and EspIO is
garbage-collected otherwise: 324378 / 26498 B, physical DRAM 13722 B. The
regression guard is
`tests/data/esp32c3_bloat_budget.json`, run with
`bash bloat esp32c3 --build --budget tests/data/esp32c3_bloat_budget.json`.

### #4763 — stm32f103c8 no-Serial scorecard: stm32f103c8 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.37, STM32duino 2.9.0, GCC 15.2.1.
Image flash / attributed RAM is 22904 / 1789 B (re-measured by #4763, supersedes the Serial-enabled #4744 26324 / 1924 B; includes #4742
compact RGBW selectors / exact adjustment). These are bloat-report image
flash / attributed RAM; board flash/RAM totals from fbuild are measured
separately and are not comparable. Attributed RAM is not physical board RAM.
3.10.3 and 3.10.6 stm32f103c8 numbers are not recorded with the no-Serial
sketch here; never reuse their old Serial-enabled measurements. The
regression guard is `tests/data/stm32f103c8_bloat_budget.json`, run with
`bash bloat stm32f103c8 --build --budget tests/data/stm32f103c8_bloat_budget.json`.

### #4768 — teensy40 no-Serial scorecard: teensy40 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.45, Teensy framework 1.160.0, ARM GCC 11.3.1.
Image flash / attributed RAM is 46080 / 21449 B (re-measured by #4768, supersedes the Serial-enabled #4745 49152 / 22990 B). These are bloat-report image
flash / attributed RAM; board flash/RAM totals from fbuild are measured
separately and are not comparable. Attributed RAM is not physical board RAM.
3.10.3 and 3.10.6 teensy40 no-Serial numbers are not recorded here; never
reuse their old Serial-enabled measurements. The regression guard is
`tests/data/teensy40_bloat_budget.json`, run with
`bash bloat teensy40 --build --budget tests/data/teensy40_bloat_budget.json`.

### #4769 — teensy41 no-Serial scorecard: teensy41 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.45, Teensy framework 1.160.0, ARM GCC 11.3.1.
Image flash / attributed RAM is 47104 / 21738 B (re-measured by #4769, supersedes the Serial-enabled #4746 50176 / 23279 B). These are bloat-report image
flash / attributed RAM; board flash/RAM totals from fbuild are measured
separately and are not comparable. Attributed RAM is not physical board RAM.
Physical static RAM, measured with `arm-none-eabi-size -A` on the same ELF, is
21984 B (`.data` 6848 + `.bss` 2688 + `.bss.dma` 12448); this differs from the
21738 B attributed figure. `.text.itcm` (28800 B of code copied into ITCM at
boot) is excluded and is not data RAM. Physical RAM is informational and is not
enforced by the budget guard.
3.10.3 and 3.10.6 teensy41 no-Serial numbers are not recorded here; never
reuse their old Serial-enabled measurements. The regression guard is
`tests/data/teensy41_bloat_budget.json`, run with
`bash bloat teensy41 --build --budget tests/data/teensy41_bloat_budget.json`.

### #4770 — rp2040 no-Serial scorecard: rp2040 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.45, Arduino-Pico 5.7.0, ARM GCC 14.2.0,
master a96b4a301. Image flash / attributed RAM is 103212 / 8624 B (re-measured
by #4770, supersedes the Serial-enabled #4813 135744 / 9474 B). These are
bloat-report image flash / attributed RAM; board flash/RAM totals from fbuild
are measured separately and are not comparable. Attributed RAM is not physical
board RAM. Physical static RAM, measured with `arm-none-eabi-size -A` on the
same ELF, is 9844 B (`.ram_vector_table` 192 + `.data` 3292 + `.bss` 6360); the
read-only NOBITS `.heap` (252296 B), `.stack_dummy` and `.stack1_dummy`
(2048 B each) reservations are excluded. Physical RAM is informational and is
not enforced by the budget guard. Blink itself has no Serial reference, but the
ELF still links `Serial`/`SerialUSB` and TinyUSB CDC: the unresolved references
come from FastLED's platform I/O object (`platforms` translation unit) and the
Arduino-Pico core `main.cpp` (USB init), not from the sketch, so this linkage
remains in the baseline.
Matched 3.10.3 (`20667c3a64`) and 3.10.6 (`ebf8c2823c`) builds use the
same fbuild project, toolchain, flags, Blink sketch and `main.cpp` with the
released `src/` unchanged; the only compatibility edits are `#define PIN_DATA 3`
(what `FL_PIN_CLOCKLESS_1` resolves to on rp2040 at master) and `delay(0)` for
the `fl::delay(0)` yield in `main.cpp`, which those releases lack. Image flash /
attributed RAM / physical RAM:

| rp2040 Blink, no Serial | image flash | attributed RAM | physical RAM |
|---|---:|---:|---:|
| 3.10.3 | 78620 B | 7049 B | 8344 B |
| 3.10.6 | 113852 B | 47433 B | 48788 B |
| master a96b4a301 | 103212 B | 8624 B | 9844 B |
| master vs 3.10.3 | +24592 B | +1575 B | +1500 B |

All three ELFs link Serial/SerialUSB and TinyUSB CDC through the Arduino-Pico
core. 3.10.6's RAM is dominated by a 38416 B `gPioRxCaptureBuffers` that
master no longer links. Master's RAM growth over 3.10.3 includes the 1 KB
`fl::detail` registry. The regression guard is
`tests/data/rp2040_bloat_budget.json`, run with
`bash bloat rp2040 --build --budget tests/data/rp2040_bloat_budget.json`.


## ESP flash recovery after #5003

Matched no-Serial Blink measurements on 2026-11-03 use fbuild
`2.5.49+git.98affea702ff`, the same checkout path, build harness and board
configuration throughout. The historical column substitutes `src/` from
`1c0ac9724e` (before #5003); the starting master is `525c533f60`, which also
includes the later introsort change #5005. These are allocated `image_flash`
bytes, not board-summary flash or summed symbol attribution.

| Target | Pre-#5003 source | Starting master | Iteration 1: shared waits | Iteration 2: compact PARLIO sorts |
|---|---:|---:|---:|---:|
| esp32s3 | 320419 | 320427 | 320255 | 320259 |
| esp32dev | 287695 | 287675 | 287547 | 287555 |
| esp32c3 | 324378 | 324372 | 324180 | 324180 |
| esp32c6 | 354760 | 355506 | 355318 | 354042 |

Each iteration used one GPT-6.1 Sol agent at low effort. Shared wait loops
preserve READY/DRAINING acceptance, spin budgets, timeout diagnostics and
yielding. PARLIO uses the existing heapsort for its unbounded timing groups
and insertion sort only for valid groups of at most 16 hardware lanes;
oversized groups retain their existing rejection. General-purpose introsort
and its worst-case guarantee are unchanged. The encoding path, worker APIs
and `Core::Auto` behavior are unchanged, with no new object fields.

Attributed RAM stays constant in all phases: S3 22451, ESP32 18651, C3 13770
and C6 17877 bytes. Toolchains are GCC 14.2.0 / Arduino 3.3.11 / IDF 5.5.5
for ESP32 and S3; GCC 13.2.0 / Arduino 3.1.0 / IDF 5.3.2 for C3; and GCC
14.2.0 / Arduino 3.3.5 / IDF 5.5.1 for C6. Local reports, source patches,
provenance and build logs are preserved in `.build/esp-flash-study/`. After
integrating master `6698370285` (including the ESP C++ guard fix), all four
iteration-2 image-flash and attributed-RAM measurements were unchanged.

