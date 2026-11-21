# Binary Size Reference

Long-form companion to [binary-size-analysis.md](binary-size-analysis.md): the
release-build levers, the artifacts each run writes, the measurement semantics
behind every number, and the lessons that should not be re-discovered. Read only
the section you need.

## AVR RAM semantics

For "Uno uses more RAM than 3.10.3" reports (dashboard, #4760/#4764), run
`bash bloat uno --build --budget tests/data/uno_blink_bloat_budget.json` and
read the `.data`/`.bss` rows in `.build/symbols/uno/report.json`. Its
`total_ram` is *attributed* RAM: a concrete controller vtable is listed once
per alias (e.g. `ClocklessController<...>` and `NEOPIXEL<3>` share one
address), so it overstates physical RAM. Physical static RAM is the
`build succeeded (... ram: N bytes)` line from fbuild. To diff two builds,
compare rows by address and size, never by summing name-only deltas.

The dashboard's matched sketch (FastLED/dashboard `benchmark/Blink.ino`,
`benchmark/config/uno.ini`) has a weak `main()` and no Serial, so its numbers
differ from repository Blink; measure both when claiming parity.

### Rejected: a RAM win that costs more flash

Rejected because flash cost outweighed RAM: replacing the `addLeds` static
guard with a placement-new flag (-7 B RAM, +116 B flash, since the compiler
loses the controller's dynamic type and divides at runtime) and a power-limit
flag instead of `mPPowerFunc` (-1 B RAM, +1.6 KB flash, since it links the
limiter unconditionally). Always check flash alongside RAM.

### RP2040 reserved heap is not programmed flash

Arduino-Pico's ELF contains a read-only `SHT_NOBITS` `.heap` reservation.
GNU `size` includes it, and the read-only `.stack_dummy` reservation, in
`text`; fbuild 2.5.37's board flash summary therefore grows when unused BSS
is removed and the reserved heap expands. Compare allocated `image_flash`
from `bash bloat`, and keep the raw board summary separate.

For #4747, the SPI routing and unused RX pool optimizations reduced Blink's
allocated image from 135,820 to 135,744 bytes and physical RAM from 59,980 to
21,564 bytes. SPI routing saves 32 image bytes; the unused 38,416-byte RX pool
and its constructor account for another 44 image bytes and the RAM saving.
The new raw flash summary is 388,476 bytes: 135,744 image bytes plus
250,684 reserved heap bytes and 2,048 reserved stack bytes. Those reservations
are not programmed into flash. The scoped RP2040 budget enforces allocated
image bytes and attributed RAM; neither metric includes the heap reservation.

### Slim ESP32-S3 profile (#4564)

```bash
# Build + analyze the documented log-off release config
bash bloat esp32s3 --build --profile slim

# Build both default and slim, report them side by side
bash bloat esp32s3 --build --compare
```

**The default `bash bloat` image is NOT the documented release config.** It is built without `-DNDEBUG`, so `src/fl/log/log.h` resolves `FL_LOG_LEVEL` to 1 (errors only, since #4712): `FL_ERROR`/`FL_PRINT` stay linked, the `FL_WARN`/`FL_INFO` string pool does not. Before #4712 the default was full logging; reference numbers from that era: Blink at `0811ca88af` measured 374,588 B attributed / 459,560 B `firmware.bin`.

`--profile slim` applies `-DFASTLED_LOG_VERBOSITY=0` plus `tools/sdkconfig_for_smallest_fastled.defaults`, and fails the run if `libespcoredump.a` or `diag_log_add` are still linked (proof the overlay actually took effect). Output goes to `.build/symbols/esp32s3-slim/` alongside a `provenance.json` recording the flags and overlay used.

Slim disables field-debug logs and coredumps, so it is opt-in only. The ordinary ratchet baseline `tests/data/esp32s3_bloat_baseline.txt` still tracks the default image and is unchanged.

`bash bloat <board>` hides every choice that was previously a per-invocation footgun — toolchain prefix, ELF path, `--nm` override, output directory. Use it instead of running `nm`/`size`/`xtensa-esp32s3-elf-nm` by hand.

The wrapper recognizes fbuild's artifact layout, `.build/fbuild/<board>/.fbuild/build/release/firmware.elf`, and lets fbuild resolve the toolchain there.

## Stack inventory (AVR `-fstack-usage`)

`bash bloat` reports flash and static RAM, not stack frames. To find large
stack frames on a small-RAM target, compile the unity TUs with avr-gcc's
`-fstack-usage` and sort the `.su` files (each lands next to its `-o` object).
This is a measurement probe, not a build: `bash compile uno` cannot emit `.su`
files, so the probe calls the avr-g++ that fbuild cached directly. Write the
outputs to a scratch directory and do not commit them.

```bash
# Point AVR and CORE at the avr-gcc and arduino-avr core that `bash compile
# uno` resolves (its "AVR resolved: avr-gcc@... arduino-avr-core@..." log
# line) and quote that pair with the results. Candidates in fbuild's cache:
#   ls -d ~/.fbuild/*/cache/toolchains/*/*/*/bin
#   ls -d ~/.fbuild/*/cache/platforms/dl-framework-arduino-avr-[0-9]*/*/*
AVR=/path/to/toolchain/bin        # directory holding avr-g++
CORE=/path/to/arduino-avr/5.4.0   # directory holding cores/ and variants/
ok=1
[ -x "$AVR/avr-g++" ] && [ -d "$CORE/cores/arduino" ] || { echo "set AVR and CORE"; ok=0; }
SRC=$PWD/src   # FastLED checkout
OUT=$(mktemp -d)   # or a session scratch directory
for f in "$SRC"/fl/build/*.cpp; do
  [ "$ok" = 1 ] || break
  "$AVR/avr-g++" -mmcu=atmega328p -Os -std=gnu++11 \
    -fno-threadsafe-statics -fno-exceptions -ffunction-sections -fdata-sections -w \
    -DF_CPU=16000000L -DARDUINO=10808 -DARDUINO_ARCH_AVR -DARDUINO_AVR_UNO \
    -I"$CORE/cores/arduino" -I"$CORE/variants/standard" -I"$SRC" \
    -fstack-usage -c "$f" -o "$OUT/$(basename "$f" .cpp).o" \
    || { echo "FAILED: $f"; ok=0; }   # a partial inventory misleads
done
[ "$ok" = 1 ] && sort -t$'\t' -k2 -n -r "$OUT"/*.su | head -40   # largest first
```

Each `.su` line is `file:line:col:function<TAB>bytes<TAB>qualifiers`. `static`
is exact. `dynamic,bounded` is an upper bound on the whole frame. Plain
`dynamic` (a VLA/`alloca`, or other unbounded adjustments) means the number is
only the fixed part. Templates that are only instantiated by user code (e.g.
`SerialPort::printf`) do not appear; compile a two-line probe `.cpp` that calls
them.

The recipe is automated as a ratchet (FastLED#5332): after `bash compile uno
--examples Blink`, `uv run python ci/codec_memory/audit.py --avr-stack` reads
the avr-gcc and core paths from the uno compile database, compiles every unity
TU with the flags above, and compares each frame of 128 B or more against
`ci/codec_memory/avr_stack_ledger.json` (keyed `<file>:<function>`; avr-gcc 7.3
truncates names containing `...`). A tracked frame that grows, or a new frame
at or above the floor, fails; frames that shrink or vanish only print a note.
`--avr-stack-update` rewrites the ledger (commit it to accept growth or tighten
the ratchet). CI runs it in `check_uno_size.yml` (the `ci-platform:uno` label,
`ci-full`, or a push to `master`). It is a per-TU inventory of every emitted
body, not a link-time call graph; the MP3 codec audit in the same script has a
call graph on the host compiler.

FastLED#5307 (ATmega328P, avr-gcc 7.3 `-Os`) used this inventory:

| Function | Before | After | Fix |
|---|---|---|---|
| `fl::readLine` | 654 B | 96 B | Platforms write the line into the caller's `fl::string` instead of a 512 B `char` buffer; the `sstream` fallback moved out of line. |
| `SerialPort::printf` caller | ~359 B each | ~52 B each | Formatting stays in the template (an `sstream`); clipping/printing moved out of line (`printFormatted`, 51 B). The 256 B buffer is gone. |
| `digitalMultiWrite16` / `8` | 1196 / 604 B | 44 / 27 B (+ `init` 40 / 6 B + `keepMajorityPort` 52 / 36 B) | The frame was a whole `DigitalMultiWrite16/8` (its nibble LUTs). The port histogram became a pairwise count (#5307). #5314 then dropped the LUTs: the classes are 34 / 17 B on AVR and the one-shot functions build one on the stack again. `DigitalMultiWrite16::init` copies the 16 pins into a flat local for the port filter, hence its 40 B. |
| `fill_noise8/16` | 2*min(N,255) B dynamic | 0 B | Each point of the 1-D raw fill depends only on its own index, so the octaves are folded per pixel with no scratch planes; output matches the buffered `fill_raw_*` formulation (#5330, pinned by `tests/noise.cpp`; AVR checked by `examples/Test/NoiseHashes.h`). The 16into8 accumulator was widened at the same time, so AVR `fill_noise16` output for octaves >= 2 changed where the old 16-bit `int` shift overflowed. |
| `fill_2dnoise8/16` | 2*W*H B dynamic | unchanged | Kept `FASTLED_STACK_ARRAY` (#5307, re-checked in #5330). Both planes are needed at once and the hue plane is read 180-degree rotated. The coarse octaves of `fill_raw_2dnoise8` step one pixel at a time but blend a skip-by-skip block, so each pixel is an order-dependent blend of up to skip^2 samples from earlier rows; an identical row-streamed form would have to recompute or cache several rows of noise per octave, costing more time than the stack it saves. A runtime-sized `fl::scratch` would not help AVR (`FL_SCRATCH_STACK_BYTES` is 0 there). Callers on small-RAM targets should keep the grid small or use the `fill_raw_*` functions with their own buffer. |

`fl::scratch<T>` was not used for these: on AVR its arena is 0 bytes by
default, so it would not have moved anything, and the ESP8266 arena is mostly
reserved for `drawPolygon`.

## Artifacts

Every run writes BOTH files side by side under `.build/symbols/<board>/`, or
`.build/symbols/<board>-slim/` for a `--profile slim` run (e.g. `esp32s3-slim/`):

| File | What's in it |
|---|---|
| `report.json` | Machine-readable: `{ symbols: [...], sections: [...], total_flash, image_flash, total_ram, ... }`. `total_flash` is the sum of attributed symbol rows (a breakdown). `image_flash` (fbuild >= 2.5.28) is the allocated, file-backed ELF section bytes, independent of `nm`; compare it across machines, and it is what the ESP32-S3 bloat gate uses (#4468). Per-symbol rows carry `archive`, `object`, `output_section`, `source`, `region`, demangled `demangled` name. Suitable for diffing two builds. |
| `report.md` | Human-readable GitHub-style tables: top FLASH symbols, top RAM symbols, per-archive flash roll-up. Renders inline on PRs. |

## Lessons baked in (do not re-discover)

These are the gotchas the wrapper handles for you. They are documented here so they survive an agent rewrite of `ci/bloat.py`:

1. **fbuild release lag.** `fbuild symbols` (the underlying subcommand) was merged to fbuild#main after the v2.2.18 wheel was tagged. The released wheel **does NOT carry it**. The wrapper's `assert_fbuild_has_symbols()` detects this and fails fast with the upgrade instructions. Until fbuild >= 2.2.19 publishes, rebuild fbuild from main (`soldr cargo build --release -p fbuild-cli` inside the dev checkout) and drop the binary into `.venv/Scripts/fbuild.exe`.

2. **Map-derived synthesis is what makes the report useful.** fbuild PR #427 parses `.rodata.<owner>.str1.<N>` input-section names and attributes those bytes to the owning function. Without it, the single biggest contributor on ESP32-S3 Blink (the NEOPIXEL chipset ctor's `FL_WARN`/`FL_LOG` string pool, ~58 KB / 15 %) appears as anonymous bytes against `main.cpp.o` and there's no way to chase it. The `source: "map-derived"` field on each symbol tags rows whose attribution came from this synthesis pass; treat them with the same trust as `source: "nm"` rows.

3. **The dominant flash lever on ESP32 is the log level.** `FL_LOG_LEVEL` (`src/fl/log/log.h`) is 0 = off, 1 = errors only (`FL_ERROR`, `FL_PRINT`, `FL_WARN_LIT`), 2 = full (`FL_WARN`/`FL_INFO`/`FL_DBG`). The `FL_WARN` string pool is ~37-58 KB. Defaults: 0 under `NDEBUG` (#2890), 1 otherwise (#4712), 2 under `FASTLED_TESTING`. The legacy `FASTLED_LOG_VERBOSITY` (#2791) still works: 0 = off, 1 = full. **When a size measurement looks off, check the effective level first — it dominates everything else.**

### Release-build flash savers

In order of impact:

| Lever | How to enable | Savings | Source |
|---|---|---:|---|
| `FL_LOG_LEVEL` 1 (errors only) | Default on non-`NDEBUG` builds; `-DFL_LOG_LEVEL=2` (or legacy `-DFASTLED_LOG_VERBOSITY=1`) restores full logs | ~37-50 KB | #4712 |
| `FL_LOG_LEVEL` 0 / `FASTLED_LOG_VERBOSITY=0` | Release default (NDEBUG); also drops `FL_ERROR`/`FL_PRINT` | a few KB more than level 1 | #2791 + #2890 |
| `tools/sdkconfig_for_smallest_fastled.defaults` | `board_build.sdkconfig_defaults` in `platformio.ini`; disables coredump, IDF log, bootloader log, panic-print + **switches newlib to nano printf (#2915 — biggest single lever)** | ~30-45 KB | #2895 + #2915 (Stage 3) |
| `-DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1` | `build_flags`; strong-overrides the Arduino-ESP32 boot-banner gate | ~3 KB | #2894 (Stage 2) |
| `-DFL_RMT_STATIC_ALLOCATION=1` | `build_flags`; exactly one fixed TX strip, no late add/remove, runtime reconfiguration, or RMT5 RX allocation | −4,395 B symbol flash and −108 B RAM versus default dynamic Blink; whole `firmware.bin` is 4,640 B smaller on the same candidate. The isolated ledger before/after results (including its 192 B whole-bin increase) are in `docs/SLIM_ESP32S3.md`. | #2846; issue #4567 |

The user-facing copy of this table — same content, written for end users rather than agents, with platformio.ini snippets — lives at [`docs/SLIM_ESP32S3.md`](../../docs/SLIM_ESP32S3.md). When a new knob lands, update both: this table for the agent reference, and `docs/SLIM_ESP32S3.md` for the user copy.

To refresh the measured numbers in the SLIM doc, run `uv run python tests/measure_esp32s3_opt_ins.py --config all --out compare.md` — the script builds the ESP32-S3 Blink under each opt-in combo, runs `bash bloat esp32s3` against each ELF, and emits a Markdown comparison table sized to drop straight into the doc. See #2905. Configs that need the Stage 3 sdkconfig overlay (`stage3`, `stack`, `max_savings`) are skipped by `all` and refused by name, because fbuild cannot apply sdkconfig overrides yet (FastLED/fbuild#1460, #4570).

4. **The ELF it analyses is the newest one, and `--build` refuses a stale one.** `find_elf` used to rank candidate ELFs under a board directory by fixed priority and ignore mtime, so `--build` could compile one and report on another. That is fixed in #4386: the newest candidate wins, with the documented order only as a tie-break, and after `--build` an ELF older than the build is refused outright rather than analysed.

   If you see

   ```
   Bloat: --build ran, but the ELF selected for analysis predates it by N h
   ```

   that guard is doing its job — a layout the build did not write is sitting where the analyser looks. Remove it, or build the layout it belongs to.

   Worth knowing what the old behaviour looked like, because it was silent: two `--build --top 200` runs across a real code change produced **byte-identical** `report.json` files, `total_flash` included, and the local total did not match CI's for the same board and example because they described different binaries. The failure mode was plausible output and an inverted conclusion, not an error.

5. **Toolchain resolution belongs to fbuild.** The current wrapper calls `fbuild symbols` without an explicit `--nm` path. fbuild resolves the installed toolchain from the board build metadata; do not reintroduce a hardcoded cross-toolchain path.

6. **Preserve both reports before comparing builds.** Each run overwrites the board's `report.json`. Save both reports and compare image/RAM totals and affected symbol rows, including their object, section, region and attribution source. Symbol aliases and map-derived rows can overlap, so do not equate a sum of name-only deltas with an image saving. The previously documented `.claude/symbolaudit/diff.py` helper is no longer present; the wrapper does not automatically produce a before/after diff.

