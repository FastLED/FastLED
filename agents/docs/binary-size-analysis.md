# Binary Size Analysis

Per-symbol flash / RAM bloat reports for FastLED builds. Use this doc when investigating "why is the firmware so big" or "what symbol regressed in this PR".

For the per-workflow `max_size` / `max_size_apa102` thresholds in `.github/workflows/check_*_size.yml` and which ones are real ceilings vs. band-aids waiting on a tracked regression, see `docs/SIZE_THRESHOLD_HISTORY.md`.

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

### RP2040 reserved heap is not programmed flash

Arduino-Pico's ELF contains a read-only `SHT_NOBITS` `.heap` reservation.
GNU `size` includes it, and the read-only `.stack_dummy` reservation, in
`text`; fbuild 2.5.37's board flash summary therefore grows when unused BSS
is removed and the reserved heap expands. Compare allocated `image_flash`
from `bash bloat`, and keep the raw board summary separate.

For #4747, removing an unused 38,416-byte RX pool reduced Blink's allocated
image from 135,788 to 135,744 bytes and physical RAM from 59,980 to 21,564
bytes. The new raw flash summary is 388,476 bytes: 135,744 image bytes plus
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

## Artifacts

Every run writes BOTH files side by side under `.build/symbols/<board>/`:

| File | What's in it |
|---|---|
| `report.json` | Machine-readable: `{ symbols: [...], sections: [...], total_flash, image_flash, total_ram, ... }`. `total_flash` is the sum of attributed symbol rows (a breakdown). `image_flash` (fbuild >= 2.5.28) is the allocated, file-backed ELF section bytes, independent of `nm`; compare it across machines, and it is what the ESP32-S3 bloat gate uses (#4468). Per-symbol rows carry `archive`, `object`, `output_section`, `source`, `region`, demangled `demangled` name. Suitable for diffing two builds. |
| `report.md` | Human-readable GitHub-style tables: top FLASH symbols, top RAM symbols, per-archive flash roll-up. Renders inline on PRs. |

## Lessons baked in (do not re-discover)

These are the gotchas the wrapper handles for you. They are documented here so they survive an agent rewrite of `ci/bloat.py`:

1. **fbuild release lag.** `fbuild symbols` (the underlying subcommand) was merged to fbuild#main after the v2.2.18 wheel was tagged. The released wheel **does NOT carry it**. The wrapper's `assert_fbuild_has_symbols()` detects this and fails fast with the upgrade instructions. Until fbuild >= 2.2.19 publishes, rebuild fbuild from main (`cargo build --release -p fbuild-cli` inside the dev checkout) and drop the binary into `.venv/Scripts/fbuild.exe`.

2. **Map-derived synthesis is what makes the report useful.** fbuild PR #427 parses `.rodata.<owner>.str1.<N>` input-section names and attributes those bytes to the owning function. Without it, the single biggest contributor on ESP32-S3 Blink (the NEOPIXEL chipset ctor's `FL_WARN`/`FL_LOG` string pool, ~58 KB / 15 %) appears as anonymous bytes against `main.cpp.o` and there's no way to chase it. The `source: "map-derived"` field on each symbol tags rows whose attribution came from this synthesis pass; treat them with the same trust as `source: "nm"` rows.

3. **The dominant flash lever on ESP32 is the log level.** `FL_LOG_LEVEL` (`src/fl/log/log.h`) is 0 = off, 1 = errors only (`FL_ERROR`, `FL_PRINT`, `FL_WARN_LIT`), 2 = full (`FL_WARN`/`FL_INFO`/`FL_DBG`). The `FL_WARN` string pool is ~37-58 KB. Defaults: 0 under `NDEBUG` (#2890), 1 otherwise (#4712), 2 under `FASTLED_TESTING`. The legacy `FASTLED_LOG_VERBOSITY` (#2791) still works: 0 = off, 1 = full. **When a size measurement looks off, check the effective level first — it dominates everything else.**

   ### Release-build flash savers (in order of impact)

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

## Don'ts

- **Don't run `xtensa-esp32s3-elf-nm` or `xtensa-esp32s3-elf-size` directly.** The wrapper subsumes both. Direct toolchain invocations have caused every prior bloat audit to wire up nm/c++filt/map by hand and miss the map-derived synthesis pass.
- **Don't shell out to `fbuild symbols` with a hardcoded ELF path.** Use the wrapper — it discovers the latest ELF, picks the right toolchain, defaults the output directory, and prints the summary table in one call.
- **Don't write a new Python aggregator under `.claude/symbolaudit/`.** Use the wrapper's report for per-symbol and per-archive attribution. Extend `ci/bloat.py` if a reusable comparison view is required.

## Related

- FastLED #2773 — ESP32-S3 binary-size meta tracker (the audit that surfaced every lesson here)
- fbuild #424 / #427 — the symbols subcommand + map-derived synthesis (merged, but post-2.2.18-wheel)
- fbuild #428 — `BuildInfo` schema migration so `--nm` becomes unnecessary
- fbuild #434 — meta to rename `symbols` → `bloat` and write reports to `.fbuild/build/<env>/bloat-report/`
