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
`tests/data/uno_blink_bloat_budget.json`, run with
`bash bloat uno --build --budget tests/data/uno_blink_bloat_budget.json`.

### #4765 — attiny85 no-Serial scorecard: attiny85 Blink (`examples/Blink`,
Serial-free since #4759) on fbuild 2.5.37, AVR GCC 7.3.0-atmel3.6.1-arduino7,
ATTinyCore 1.5.2, F_CPU=8000000L. Image flash / attributed RAM went from the
recorded 2638 B flash ceiling (#4743/#4751) to 2574 / 160 after #4788, and to
2460 / 136 after its follow-up (AVR non-virtual size/begin/endShowLeds, no
tail pointer, 16-bit FPS timestamp, Hz refresh cap). 3.10.3 and 3.10.6
attiny85 numbers are not recorded with the no-Serial sketch here; never reuse
their old Serial-enabled measurements. `total_ram` is attributed RAM, not
physical RAM (see [rules for quoting a number](binary-size-analysis.md#rules-for-quoting-a-number)). The regression guard is
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
flash / 34091 B attributed RAM, under its budget below. The budget baseline
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
14.2.0. The recorded measurement, fbuild 2.5.43 with stock console
integration (#4873), is 325075 B image flash / 39147 B attributed RAM. The
budget ceilings are 325139 B image flash (64 B link-layout slack) and
40016 B attributed RAM; the separate ratchet
`tests/data/esp32s3_bloat_baseline.txt` independently pins 325075.
Attributed RAM is not physical board RAM. Physical board RAM for this
esp32s3 Blink build has not been measured and remains open for #4767; do not
read 39147 B as physical RAM. Board flash/RAM from fbuild are not comparable with bloat-report image flash / attributed RAM. 3.10.3
and 3.10.6 esp32s3 numbers are not recorded with the no-Serial sketch here;
never reuse their old Serial-enabled measurements. The regression guard is
`tests/data/esp32s3_bloat_budget.json`, run with
`bash bloat esp32s3 --build --budget tests/data/esp32s3_bloat_budget.json`.

