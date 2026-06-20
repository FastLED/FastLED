# Size Threshold History — `.github/workflows/check_*_size.yml`

## Purpose

Every `.github/workflows/check_*_size.yml` workflow has `max_size` (Blink build) and `max_size_apa102` (Apa102 build) ceilings. Those values were frozen into `ci/lint/check_size_thresholds.py` by PR #3303 so an agent (or a tired human) cannot silently raise them to make CI green instead of fixing the underlying regression.

This document classifies each currently frozen value as either a **real ceiling** (genuine architectural / flash limit) or a **band-aid** (a value that was bumped to clear CI for a regression that is still un-fixed). Band-aid entries must have a tracking issue. The intent is that anyone reviewing a future raise can answer:

> "Is this number the actual physical limit of the chip + framework, or is it a known-soft target we should be tightening as soon as a tracked regression is fixed?"

This doc is paired with `ci/lint/check_size_thresholds.py` (the lockdown lint) and `.coderabbit.yaml` (the review rule on `check_*_size.yml`).

## Classification table

| Board | Workflow file | Frozen `max_size` | Frozen `max_size_apa102` | Status | Tracking issue | Notes |
|---|---|---:|---:|---|---|---|
| uno | `check_uno_size.yml` | 11000 / -1 | 9300 / -1 | real ceiling | — | AVR ATmega328P has 32 KB flash. The `-1` second value is the `build_no_forced_inline` job's "no check" sentinel. Apa102 was tightened from 12050 → 9300 in `7edaf80f0` (a real optimisation, not a bump). |
| bluepill | `check_bluepill_size.yml` | 55000 | 45000 | real ceiling | — | STM32F103C8 has 64 KB flash. Workflow created at current values in `bf76a0319` (2025-06-25). Never bumped. |
| esp32dev | `check_esp32_size.yml` | 330000 | 330000 | real ceiling (CI currently RED on #3298) | #3298 | 330 KB is the canonical ceiling that PR #3268 restored on top of the `-Os -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections` size-strip flags. The 700 KB intermediate value (PR #2790, #3295) was a band-aid; PR #3303 reverted it. CI is RED until #3298 fixes fbuild's ESP32 `build_flags` / `build_unflags` propagation — the red signal is the point: it forces the underlying fbuild bug to be addressed instead of being hidden. **Do not raise above 330000 — fix #3298 instead.** |
| teensy30 | `check_teensy30_size.yml` | 60000 | 50000 | real ceiling | — | MK20DX128 (Teensy 3.0) has 128 KB flash. Workflow created at current values in `f4317e954` (2025-06-25). Never bumped. |
| teensy31 | `check_teensy31_size.yml` | 80000 | 65000 | real ceiling | — | MK20DX256 (Teensy 3.1) has 256 KB flash. Workflow created at current values in `f4317e954` (2025-06-25). Never bumped. |
| teensy32 | `check_teensy32_size.yml` | 80000 | 65000 | real ceiling | — | MK20DX256 (Teensy 3.2) has 256 KB flash. Workflow created at current values. Never bumped. |
| teensy35 | `check_teensy35_size.yml` | 100000 | 85000 | real ceiling | — | MK64FX512 (Teensy 3.5) has 512 KB flash. Workflow created at current values. Never bumped. |
| teensy36 | `check_teensy36_size.yml` | 120000 | 100000 | real ceiling | — | MK66FX1M0 (Teensy 3.6) has 1 MB flash. Workflow created at current values. Never bumped. |
| teensy41 | `check_teensy41_size.yml` | 120000 | **165000 (BAND-AID)** | apa102 is band-aid | #2802 | Blink (`max_size: 120000`) is a real ceiling. **Apa102 (165000) is a band-aid**, bumped from 88000 in PR #2804 (closes #2802) to unblock CI after the library legitimately grew past the budget (audio API, `fl::stl` additions, channel manager, `fl::AsyncLog*`, `fl::ifstream` linkage). The real ceiling is **88000** once #2802's over-link of `fl::ifstream` / `fl::posix_filebuf` / `fl::strerror` / `fl::AsyncLog*` into the Apa102 link is fixed. Current real size ≈148476 B. |
| teensylc | `check_teensylc_size.yml` | 35000 | 30000 | real ceiling | — | MKL26Z64 (Teensy LC) has 64 KB flash. Workflow created at current values in `f4317e954` (2025-06-25). Never bumped. |

## Historical bumps catalogued

These are the events the audit found in `git log --all --follow --patch -- .github/workflows/check_*_size.yml`. Bumps to the still-current frozen value are flagged.

### esp32dev (`check_esp32_size.yml`)

| Date | Commit | Change | Rationale | Classification |
|---|---|---|---|---|
| 2024-09-03 | `a0489cd39` | created at 300000 / 300000 | initial workflow | n/a |
| 2024-12-17 | `12f4e27bb` | 300000 → 320000 | "external code update and now this binary size has gotten bigger" — arduino-esp32 framework growth | soft (external framework growth) |
| 2025-08-15 | `8ed2ea80f` (PR #2056) | 320000 → 330000 | arduino-esp32 framework grew "a little bit" | soft (external framework growth) |
| 2026-06-05 | `f00e8cf1d` (PR #2790, closes #2608) | 330000 → 700000 | band-aid: real ≈635 KB after Channel API + missing `--gc-sections` after PIO backend pin | **band-aid (reverted)** |
| 2026-06-05 | `e646657d0` | 700000 → 330000 | restore canonical ceiling after fbuild + `--gc-sections` reinstated | restore |
| 2026-06-19 | `876409988` (PR #3295) | 330000 → 700000 | band-aid: CI red on master because the `[env:esp32dev]` size-strip flags weren't reaching fbuild (#3298) | **band-aid (reverted)** |
| 2026-06-19 | `5dd070abc` (PR #3268), `30ee2eba2` (PR #3295 follow-up) | (intermediate work) | port the size-strip flags into `ci/boards.py::ESP32DEV.build_flags` | progress on #3298 |
| 2026-06-19 | `ee842e51d` (PR #3303) | 700000 → 330000 | lockdown + revert: 330000 is the real ceiling; CI stays red until #3298 is fixed | **current frozen value** |

### teensy41 (`check_teensy41_size.yml`)

| Date | Commit | Change | Rationale | Classification |
|---|---|---|---|---|
| 2024-09-03 | `44590c36d` | created at 80000 / 80000 | initial workflow | n/a |
| 2024-09-03 | `3ccbb85ae` | `max_size`: 80000 → ? | (no body) | minor |
| 2024-12-23 | `9c5eeeed3` | `max_size_apa102`: 80000 → 84000 | "update max_size_apa102 for teensy41 board" | soft |
| 2025-01-21 | `ab92dc115` | `max_size`: 80000 → 104000 | "build: increase max size limit for teensy41" | soft |
| 2025-04-25 | `e31a807ec` | `max_size`: 104000 → 107000 | "Increase max_size for Teensy 4.1 to 107000" | soft |
| 2025-05-19 | `dc25e80e0` | `max_size_apa102`: 84000 → 88000 | "Update max_size_apa102 to 88000 in Teensy41 size check" | soft |
| 2025-08-09 | `3e99118d3` | `max_size`: 107000 → 120000 | "fix teensy41 size check" — accommodating real growth | **current `max_size`** (real ceiling) |
| 2026-06-05 | `ab67e3f1f` (PR #2804, closes #2802) | `max_size_apa102`: 88000 → 165000 | band-aid for `fl::ifstream` / `fl::AsyncLog*` over-link into Apa102 (current real ≈148476 B) | **band-aid (current)** — restore to 88000 once #2802 is fixed |

### Other boards

`check_uno_size.yml`, `check_bluepill_size.yml`, `check_teensy30/31/32/35/36/lc_size.yml`: created at their current values and never bumped after creation (apart from uno's apa102 12050 → 9300 *tightening* in `7edaf80f0`). All flagged **real ceiling**.

## Band-aid follow-up: how to restore the real ceiling

### #2802 — teensy41 Apa102 over-link (band-aid: 165000, real: 88000)

The Apa102 link on teensy41 currently pulls in `fl::ifstream`, `fl::posix_filebuf`, `fl::strerror`, and `fl::AsyncLog*` even though Apa102 itself does not need filesystem I/O or async logging. Investigate the call chain from the Apa102 driver into these symbols, then either:

1. Gate the offending pulls behind a feature flag that Apa102 does not enable, or
2. Restructure so the symbols are only linked when the user explicitly opts in (e.g. linker tree-shake via `--gc-sections` + API-only opt-in — see MEMORY: "Prefer linker tree-shake over compile gates").

Once the Apa102 build measures ≤ 88000 B locally, the lockdown lint allows tightening back to 88000 in both the YAML and `FROZEN_THRESHOLDS`. Reference #2802 in the PR body.

### #3298 — fbuild ESP32 board flag propagation (ceiling 330000, currently RED)

The 330 KB ceiling for esp32dev depends on `-Os -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections` (and `build_unflags = -Og -g`) being honoured by the fbuild ESP32 toolchain. After PR #3295's Phase 4 cleanup, those flags live only in `ci/boards.py::ESP32DEV.build_flags` / `.build_unflags` — but fbuild produces a 401240-byte ELF that is identical with or without them, evidence that fbuild is dropping or overriding them.

The ceiling is correct. The fix is in fbuild's flag-propagation path; until that lands, `check_esp32_size` will stay red on master. Do not raise the ceiling. Reference #3298 in any related PR.

## How to change a value here

1. Open / link a tracking issue justifying the change (real architectural change, not "CI is red and I want it green").
2. Edit the matching `.github/workflows/check_<board>_size.yml`.
3. Edit the matching entry in `ci/lint/check_size_thresholds.py::FROZEN_THRESHOLDS`.
4. Update the row in this table — set Status, add the issue link in Notes, move historical bumps into the per-board section.
5. Reference the issue # in the PR body. CodeRabbit's `.coderabbit.yaml` rule on `check_*_size.yml` flags the diff as HIGH severity and requires maintainer sign-off.

## Refs

- PR #3303 — defensive lockdown (lint + CodeRabbit rule + this doc's predecessor comments).
- PR #3305 — this audit / doc.
- Issue #2802 — teensy41 Apa102 over-link.
- Issue #2608 — esp32dev 330 KB ceiling stale vs. 635 KB reality (closed once #3303 reverted to 330 KB; the underlying flag-port work moved to #3298).
- Issue #3298 — fbuild not propagating ESP32 board-level `build_flags` / `build_unflags`.
- PR #2790, PR #3295 — historical 700 KB band-aids (both reverted).
- PR #2804 — teensy41 Apa102 band-aid (still in effect; tracked by #2802).
