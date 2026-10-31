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
| `uno` | #4788 / #4764 | 3592 B | 118 B (physical) | `tests/data/uno_blink_bloat_budget.json` |
| `attiny85` | #4765 | 2460 B | 136 B | `tests/data/attiny85_bloat_budget.json` |
| `esp32c6` | #4762 | 402674 B | 33478 B | `tests/data/esp32c6_bloat_budget.json` |
| `esp32dev` | #4766 | 287671 B | 34091 B | `tests/data/esp32dev_bloat_budget.json` |
| `esp32s3` | #4767 | 325075 B | 39147 B | `tests/data/esp32s3_bloat_budget.json` |

Image flash and attributed RAM are bloat-report metrics, **not** fbuild's board
flash/RAM totals; the two are not interchangeable for any board in this table.
Attributed RAM moves with fbuild's attribution, so the table carries the value a
currently supported fbuild reports (the budget guard's ceiling): the same esp32c6
ELF reads 29027 B on fbuild 2.5.37 and 33478 B on 2.5.39+, at identical image size.

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
