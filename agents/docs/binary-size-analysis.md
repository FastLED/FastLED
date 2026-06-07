# Binary Size Analysis

Per-symbol flash / RAM bloat reports for FastLED builds. Use this doc when investigating "why is the firmware so big" or "what symbol regressed in this PR".

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
```

`bash bloat <board>` hides every choice that was previously a per-invocation footgun — toolchain prefix, ELF path, `--nm` override, output directory. Use it instead of running `nm`/`size`/`xtensa-esp32s3-elf-nm` by hand.

Supported boards live in `ci/bloat.py::BOARD_CHIP_MAP` (esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2). Adding a new board is a one-line entry mapping the board id to its architecture + chip prefix; the script resolves the cross-toolchain `nm` from the PIO packages directory.

## Artifacts

Every run writes BOTH files side by side under `.build/symbols/<board>/`:

| File | What's in it |
|---|---|
| `report.json` | Machine-readable: `{ symbols: [...], sections: [...], total_flash, total_ram, ... }`. Per-symbol rows carry `archive`, `object`, `output_section`, `source`, `region`, demangled `demangled` name. Suitable for diffing two builds. |
| `report.md` | Human-readable GitHub-style tables: top FLASH symbols, top RAM symbols, per-archive flash roll-up. Renders inline on PRs. |

## Lessons baked in (do not re-discover)

These are the gotchas the wrapper handles for you. They are documented here so they survive an agent rewrite of `ci/bloat.py`:

1. **fbuild release lag.** `fbuild symbols` (the underlying subcommand) was merged to fbuild#main after the v2.2.18 wheel was tagged. The released wheel **does NOT carry it**. The wrapper's `assert_fbuild_has_symbols()` detects this and fails fast with the upgrade instructions. Until fbuild >= 2.2.19 publishes, rebuild fbuild from main (`cargo build --release -p fbuild-cli` inside the dev checkout) and drop the binary into `.venv/Scripts/fbuild.exe`.

2. **Map-derived synthesis is what makes the report useful.** fbuild PR #427 parses `.rodata.<owner>.str1.<N>` input-section names and attributes those bytes to the owning function. Without it, the single biggest contributor on ESP32-S3 Blink (the NEOPIXEL chipset ctor's `FL_WARN`/`FL_LOG` string pool, ~58 KB / 15 %) appears as anonymous bytes against `main.cpp.o` and there's no way to chase it. The `source: "map-derived"` field on each symbol tags rows whose attribution came from this synthesis pass; treat them with the same trust as `source: "nm"` rows.

3. **The dominant flash lever on ESP32-S3 is `FASTLED_LOG_VERBOSITY=0`.** FastLED PR #2791 introduced this build-time knob. Setting it (define before `#include <FastLED.h>`) collapses ~43-58 KB of FL_WARN string pool with zero behaviour change for users who don't need release-mode logging. **Always check whether this knob is set before chasing other optimisations — it's a single define that dominates everything else.** Since #2890 (Stage 1 of #2886), the default flips to `0` automatically when `NDEBUG` is set (release builds).

   ### Release-build flash savers (in order of impact)

   | Lever | How to enable | Savings | Source |
   |---|---|---:|---|
   | `FASTLED_LOG_VERBOSITY=0` | Now the release default (NDEBUG); explicit `-DFASTLED_LOG_VERBOSITY=1` to restore | ~43-58 KB | #2791 + #2890 |
   | `tools/sdkconfig_for_smallest_fastled.defaults` | `board_build.sdkconfig_defaults` in `platformio.ini`; disables coredump, IDF log, bootloader log, panic-print + **switches newlib to nano printf (#2915 — biggest single lever)** | ~30-45 KB | #2895 + #2915 (Stage 3) |
   | `-DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1` | `build_flags`; strong-overrides the Arduino-ESP32 boot-banner gate | ~3 KB | #2894 (Stage 2) |
   | `-DFASTLED_RMT_STATIC_ALLOCATION=1` | `build_flags`; for sketches that init LEDs in `setup()` and never remove | ~22-43 KB | #2846 |

   The user-facing copy of this table — same content, written for end users rather than agents, with platformio.ini snippets — lives at [`docs/SLIM_ESP32S3.md`](../../docs/SLIM_ESP32S3.md). When a new knob lands, update both: this table for the agent reference, and `docs/SLIM_ESP32S3.md` for the user copy.

   To refresh the measured numbers in the SLIM doc, run `uv run python tests/measure_esp32s3_opt_ins.py --config all --out compare.md` — the script builds the ESP32-S3 Blink under each opt-in combo, runs `bash bloat esp32s3` against each ELF, and emits a Markdown comparison table sized to drop straight into the doc. See #2905.

4. **`--nm` is still required.** fbuild's `build_info.json` does not yet carry toolchain paths (`nm_path` / `cppfilt_path`). The wrapper resolves them from PIO packages. fbuild issue #428 tracks the migration to build-info-driven resolution; when that lands, drop the explicit `--nm` path in `ci/bloat.py::run_fbuild_symbols`.

5. **Diff two builds with the existing diff script.** Save two `report.json` files and run `uv run python .claude/symbolaudit/diff.py <old.json> <new.json>` for a per-symbol delta table (added / removed / grew / shrunk). The wrapper does NOT do this automatically; ship a follow-up PR if you need it inline.

## Don'ts

- **Don't run `xtensa-esp32s3-elf-nm` or `xtensa-esp32s3-elf-size` directly.** The wrapper subsumes both. Direct toolchain invocations have caused every prior bloat audit to wire up nm/c++filt/map by hand and miss the map-derived synthesis pass.
- **Don't shell out to `fbuild symbols` with a hardcoded ELF path.** Use the wrapper — it discovers the latest ELF, picks the right toolchain, defaults the output directory, and prints the summary table in one call.
- **Don't write a new Python aggregator under `.claude/symbolaudit/`.** The wrapper's `_AggBucket` summary plus the existing `diff.py` cover the per-symbol and per-archive views needed for both "what's in this build" and "what changed between two builds". Extend `ci/bloat.py` if a new view is required.

## Related

- FastLED #2773 — ESP32-S3 binary-size meta tracker (the audit that surfaced every lesson here)
- fbuild #424 / #427 — the symbols subcommand + map-derived synthesis (merged, but post-2.2.18-wheel)
- fbuild #428 — `BuildInfo` schema migration so `--nm` becomes unnecessary
- fbuild #434 — meta to rename `symbols` → `bloat` and write reports to `.fbuild/build/<env>/bloat-report/`
