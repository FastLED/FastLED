# Binary Size Analysis

Per-symbol flash / RAM bloat reports for FastLED builds. Use this doc when investigating "why is the firmware so big" or "what symbol regressed in this PR".

For the per-workflow `max_size` / `max_size_apa102` thresholds in `.github/workflows/check_*_size.yml` and which ones are real ceilings vs. band-aids waiting on a tracked regression, see `docs/SIZE_THRESHOLD_HISTORY.md`.

This is the entry page: the command, the current per-board readings, the rules
for quoting a number, and the don'ts. Long-form material is on two companion
pages so a bloat investigation does not have to read all of it:

| Need | Read |
|---|---|
| Where a board's number came from, per-issue provenance | [binary-size-scorecards.md](binary-size-scorecards.md) |
| Artifacts, measurement semantics, the release-build levers, lessons | [binary-size-reference.md](binary-size-reference.md) |

## Quick start

```bash
# Default — analyzes the latest Blink build, prints top-10 flash symbols
bash bloat esp32s3

# Different example
bash bloat esp32s3 --example FxFire

# Deeper top-N
bash bloat esp32s3 --top 25

# Rebuild before analyzing (chains `bash compile`)
bash bloat esp32s3 --build

# JSON + MD artifact only, no stdout table
bash bloat esp32s3 --no-summary

# Check committed image-flash / attributed-RAM ceilings
bash bloat esp32c3 --budget tests/data/esp32c3_bloat_budget.json
```

`--budget` requires matching board, example and profile in its JSON file and
rejects missing measurements or growth above either ceiling. Its `image_flash`
and `total_ram` ceilings refer to the bloat report's image flash and attributed
RAM; they are not fbuild's board flash/RAM totals. The RMT allocation-record
compile-time size assertion additionally guards the measured static RAM saving.
Check each profile separately; `--compare --budget` is rejected before building.

## Current per-board readings

Latest no-Serial Blink measurement per board, with the budget that guards it.
Provenance (fbuild version, toolchain, what changed) is in
[binary-size-scorecards.md](binary-size-scorecards.md).

| Board | Issue | Image flash | Attributed RAM | Budget |
|---|---|---:|---:|---|
| `uno` | #4788 / #4764 | 3600 B | not recorded | `tests/data/uno_blink_bloat_budget.json` |
| `attiny85` | #4765 | 2460 B | 136 B | `tests/data/attiny85_bloat_budget.json` |
| `esp32c6` | #4762 | 354042 B | 17877 B | `tests/data/esp32c6_bloat_budget.json` |
| `esp32dev` | #4766 | 287555 B | 18651 B | `tests/data/esp32dev_bloat_budget.json` |
| `esp32s3` | #4767 | 320259 B | 22451 B | `tests/data/esp32s3_bloat_budget.json` |
| `esp32c3` | #4761 / #4982 | 324180 B | 13770 B | `tests/data/esp32c3_bloat_budget.json` |
| `stm32f103c8` | #4763 | 22904 B | 1789 B | `tests/data/stm32f103c8_bloat_budget.json` |
| `teensy40` | #4768 | 46080 B | 21449 B | `tests/data/teensy40_bloat_budget.json` |
| `teensy41` | #4769 | 47104 B | 21738 B | `tests/data/teensy41_bloat_budget.json` |
| `rp2040` | #4770 | 103212 B | 8624 B | `tests/data/rp2040_bloat_budget.json` |

Image flash and attributed RAM are bloat-report metrics, **not** fbuild's board
flash/RAM totals; the two are not interchangeable for any board in this table.
Unknown readings are not replaced with budget ceilings: Uno's 118 B is
physical RAM. The ESP rows are local 2026-11-03 readings; the
[matched recovery scorecard](binary-size-scorecards.md#esp-flash-recovery-after-5003)
records the toolchains, fbuild version and historical comparisons.

## Rules for quoting a number

- `total_ram` is *attributed* RAM: a controller vtable is listed once per alias,
  so it overstates physical RAM. Physical static RAM is fbuild's
  `build succeeded (... ram: N bytes)` line.
- fbuild's board flash/RAM totals and the bloat report's `image_flash` /
  `attributed RAM` describe different things. Never mix them in one comparison.
- fbuild version moves attribution, not code: weak-vtable owners (#4813 /
  PR #4814) raise attributed RAM from 2.5.37 to 2.5.39 at identical image size.
- RP2040: the ELF's read-only `.heap` / `.stack_dummy` reservations are counted
  by GNU `size` as programmed flash. Compare allocated `image_flash`.
- Always check flash alongside RAM when taking a "RAM win" (see the rejected
  experiments in the [reference page](binary-size-reference.md#avr-ram-semantics)).

## The dominant lever

On ESP32 the log level beats everything else: `FL_LOG_LEVEL` 2 (full) links the
`FL_WARN`/`FL_INFO`/`FL_DBG` string pool, ~37-58 KB. The default `bash bloat`
image is **not** the documented release config; `bash bloat <board> --profile
slim` is. All levers, their measured savings and the `--profile slim` recipe:
[binary-size-reference.md](binary-size-reference.md#release-build-flash-savers).

## Stack inventory (AVR `-fstack-usage`)

`bash bloat` reports flash and static RAM, not stack frames. To find large
stack frames on a small-RAM target, compile the unity TUs with avr-gcc's
`-fstack-usage` and sort the `.su` files (each lands next to its `-o` object).
This is a measurement probe, not a build: `bash compile uno` cannot emit `.su`
files, so the probe calls the avr-g++ that fbuild cached directly. Write the
outputs to a scratch directory and do not commit them.

```bash
# The avr-gcc and Arduino AVR core that fbuild cached for `bash compile uno`:
AVR=$(dirname "$(ls ~/.fbuild/*/cache/toolchains/*/*/*/bin/avr-g++ | head -1)")
CORE=$(ls -d ~/.fbuild/*/cache/platforms/dl-framework-arduino-avr-[0-9]*/*/* | head -1)
SRC=$PWD/src   # FastLED checkout
OUT=$(mktemp -d)   # or a session scratch directory
for f in "$SRC"/fl/build/*.cpp; do
  "$AVR/avr-g++" -mmcu=atmega328p -Os -std=gnu++11 \
    -fno-threadsafe-statics -fno-exceptions -ffunction-sections -fdata-sections -w \
    -DF_CPU=16000000L -DARDUINO=10808 -DARDUINO_ARCH_AVR -DARDUINO_AVR_UNO \
    -I"$CORE/cores/arduino" -I"$CORE/variants/standard" -I"$SRC" \
    -fstack-usage -c "$f" -o "$OUT/$(basename "$f" .cpp).o"
done
sort -t$'\t' -k2 -n -r "$OUT"/*.su | head -40   # largest frames first
```

Each `.su` line is `file:line:col:function<TAB>bytes<TAB>static|dynamic`.
`dynamic` frames hold a VLA/`alloca`, so the number shown is only the fixed
part. Templates that are only instantiated by user code (e.g.
`SerialPort::printf`) do not appear; compile a two-line probe `.cpp` that calls
them. `ci/codec_memory/audit.py` does the same for the MP3 codecs on the host
compiler, with a call graph; it is not set up for AVR.

FastLED#5307 (ATmega328P, avr-gcc 7.3 `-Os`) used this inventory:

| Function | Before | After | Fix |
|---|---|---|---|
| `fl::readLine` | 654 B | 96 B | Platforms write the line into the caller's `fl::string` instead of a 512 B `char` buffer; the `sstream` fallback moved out of line. |
| `SerialPort::printf` caller | ~359 B each | ~52 B each | Formatting stays in the template (an `sstream`); clipping/printing moved out of line (`printFormatted`, 51 B). The 256 B buffer is gone. |
| `digitalMultiWrite16` / `8` | 1196 / 604 B | 46 / 27 B (+52 / 36 B callee) | The frame was a whole `DigitalMultiWrite16/8` (its nibble LUTs). The one-shot functions now drive the pins directly; the port histogram became a pairwise count. |
| `fill_2dnoise8/16`, `fill_noise8/16` | 2*W*H B dynamic (1-D: 2*min(N,255)) | unchanged | Kept `FASTLED_STACK_ARRAY`. Both planes are needed at once (hue is read 180-degree rotated), so the buffer cannot be chunked by rows without changing the octave math, and a runtime-sized `fl::scratch` would not help AVR (`FL_SCRATCH_STACK_BYTES` is 0 there). Callers on small-RAM targets should keep the grid small or use the `fill_raw_*` functions with their own buffer. |

`fl::scratch<T>` was not used for these: on AVR its arena is 0 bytes by
default, so it would not have moved anything, and the ESP8266 arena is mostly
reserved for `drawPolygon`.

## Don'ts

- **Don't run `xtensa-esp32s3-elf-nm` or `xtensa-esp32s3-elf-size` directly.** The wrapper subsumes both. Direct toolchain invocations have caused every prior bloat audit to wire up nm/c++filt/map by hand and miss the map-derived synthesis pass.
- **Don't shell out to `fbuild symbols` with a hardcoded ELF path.** Use the wrapper — it discovers the latest ELF, picks the right toolchain, defaults the output directory, and prints the summary table in one call.
- **Don't write a new Python aggregator under `.claude/symbolaudit/`.** Use the wrapper's report for per-symbol and per-archive attribution. Extend `ci/bloat.py` if a reusable comparison view is required.

## Related

- FastLED #2773 — ESP32-S3 binary-size meta tracker (the audit that surfaced every lesson here)
- fbuild #424 / #427 — the symbols subcommand + map-derived synthesis (merged, but post-2.2.18-wheel)
- fbuild #428 — `BuildInfo` schema migration so `--nm` becomes unnecessary
- fbuild #434 — meta to rename `symbols` → `bloat` and write reports to `.fbuild/build/<env>/bloat-report/`

- Companion pages: [scorecards](binary-size-scorecards.md) ·
  [reference](binary-size-reference.md)
