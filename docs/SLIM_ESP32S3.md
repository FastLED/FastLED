# Slimming the FastLED ESP32-S3 binary

A reference page for users who want to shrink their FastLED release-build flash footprint on ESP32-S3 (and to a large extent ESP32-S2, ESP32-C3, ESP32-C6, ESP32-H2, ESP32-P4 — anywhere the Arduino-ESP32 + ESP-IDF stack is in play).

Every knob on this page is **opt-in via a build flag, sdkconfig override, or a single-include shim**. There are no source-level changes a user has to make.

## TL;DR

```ini
; platformio.ini — drop-in starter for the smallest possible FastLED esp32-s3 build
[env:esp32s3-slim]
platform = espressif32
board = esp32-s3-devkitm-1
framework = arduino
build_flags =
    -DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1
    ; FASTLED_LOG_VERBOSITY defaults to 0 in release builds (NDEBUG)
    ; since FastLED #2890 — no explicit flag needed.
board_build.sdkconfig_defaults =
    sdkconfig.defaults
    ${platformio.packages_dir}/framework-arduinoespressif32/tools/sdkconfig.defaults.esp32s3
    ${PROJECT_LIBDEPS_DIR}/${PIOENV}/FastLED/tools/sdkconfig_for_smallest_fastled.defaults
```

That combination drops the NEOPIXEL Blink baseline from ~388 KB to ~320-325 KB on a typical release build, and another ~15 KB on top if the project doesn't use Bluetooth (uncomment `CONFIG_BT_ENABLED=n` in the overlay).

## Baseline & how to measure

| Quantity | Value | Source |
|---|---:|---|
| ESP32-S3 NEOPIXEL Blink baseline (pre-#2886) | 388,380 B flash | `bash bloat esp32s3 --top 25` against pre-Stage-1 master |
| Stage 6 target | ≤ 280,000 B (-28 %) | #2886 goal |

To measure your own build: `bash bloat esp32s3 --build` then `jq '.total_flash' .build/symbols/esp32s3/report.json`. Tool details live in [`agents/docs/binary-size-analysis.md`](../agents/docs/binary-size-analysis.md).

## Release-build flash savers (in priority order)

| # | Lever | How to enable | Savings | Status | PR |
|---:|---|---|---:|:---:|---|
| 1 | `FASTLED_LOG_VERBOSITY=0` | **Default on release builds** (NDEBUG); `-DFASTLED_LOG_VERBOSITY=1` to restore | ~43-58 KB | 📊 | #2890 |
| 2 | `tools/sdkconfig_for_smallest_fastled.defaults` | `board_build.sdkconfig_defaults` in `platformio.ini` | ~10-15 KB | 📊 | #2896 |
| 3 | `-DFASTLED_RMT_STATIC_ALLOCATION=1` | `build_flags`; for sketches that init LEDs in `setup()` and never `removeLeds()` | ~22-43 KB | 📊 | #2846 |
| 4 | `-DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1` | `build_flags`; strong-overrides the Arduino-ESP32 boot-banner gate | ~3 KB | 📊 | #2894 |
| 5 | `CONFIG_BT_ENABLED=n` | uncomment the situational block in `tools/sdkconfig_for_smallest_fastled.defaults` | ~15 KB (if currently on) | 📊 | — |

**Legend:** ✅ measured against a recorded baseline · 📊 projected from the top-25 symbol attribution in #2886.

All five entries are projections today; the column flips to ✅ once Stage 6 of #2886 wires the regression-gate run that records measured deltas. See the [Roadmap](#roadmap) below.

## Per-stage detail

### 1. `FASTLED_LOG_VERBOSITY=0` (the big one)

**Mechanism:** `src/fl/log/log.h:66-92`. The unset default resolves as `FASTLED_TESTING` → 1, `NDEBUG` → 0, otherwise → 1. At level 0 the `FL_WARN` / `FL_INFO` / `FL_ERROR` / `FL_PRINT` / `FL_DBG` macros expand to `do {} while(0)`, so the optimizer drops the formatted-stream operator chain and the linker drops every literal carried with it.

**Where the savings come from (per #2886 top-25):**

- ~58 KB from the `ClocklessIdf5` ctor's transitive `FL_WARN` rodata pool (75 sites in `channel_driver_rmt.cpp.hpp`, 59 in `rmt_memory_manager.cpp.hpp`, 6 in `manager.cpp.hpp`, 5 in `channel.cpp.hpp`).
- Additional ~5-10 KB scattered across smaller call sites (`createChannel`, `reconfigureForNetwork`, `handleAllocateTxFailure`, `Rmt5EncoderImpl::initialize`).

**To restore on a release build** (e.g. for field debugging): add `-DFASTLED_LOG_VERBOSITY=1` to `build_flags`.

**To force off on a non-release build** (e.g. measuring savings without an NDEBUG rebuild): add `-DFASTLED_LOG_VERBOSITY=0` to `build_flags`.

### 2. `tools/sdkconfig_for_smallest_fastled.defaults`

**Mechanism:** ESP-IDF sdkconfig overlay applied via `board_build.sdkconfig_defaults`. The overlay flips four knobs:

| Knob | Disables | Savings |
|---|---|---:|
| `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=n` | libespcoredump (top-25 rows #18 + #25) | ~8 KB |
| `CONFIG_LOG_DEFAULT_LEVEL=ESP_LOG_NONE` | libesp_diagnostics + IDF log strings (row #19) | ~3 KB |
| `CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y` | bootloader-stage UART chatter | ~1 KB |
| `CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y` | panic-time backtrace formatter | ~1-2 KB |

The overlay file at [`../tools/sdkconfig_for_smallest_fastled.defaults`](../tools/sdkconfig_for_smallest_fastled.defaults) carries the full per-knob commentary inline.

**`CONFIG_BT_ENABLED=n`** is shipped as a commented-out situational block — uncomment only if your sketch doesn't use BLE/BT (another ~15 KB).

### 3. `FASTLED_RMT_STATIC_ALLOCATION=1`

**Mechanism:** Selects the `ChannelEngineRMTImpl<kStatic=true>` specialization at boot. Sketches that init their LEDs in `setup()` and never call `removeLeds()` or late `addLeds()` get a smaller `allocateTx` + `createChannel` path. Pre-existing knob from FastLED #2846; included here so it's discoverable from the same reference.

**Safety:** flips a thread-local `kIsStaticInit` after `setup()` returns. Dynamic add/remove after that point is unsafe under the static path. If your sketch reconfigures LEDs at runtime, don't set this flag.

### 4. `FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1`

**Mechanism:** `src/extras/suppress_arduino_chip_debug_report.cpp.hpp` defines a strong override of the Arduino-ESP32 weak symbol `shouldPrintChipDebugReport()` that returns `false` unconditionally. The optimizer folds both `if (shouldPrintChipDebugReport())` branches at vendor `main.cpp:55` and `:63` to dead code, and `--gc-sections` drops `printBeforeSetupInfo()` (~1,464 B at top-25 row #21) plus its rodata carrier strings.

**Why not on by default?** Some users intentionally rely on the boot banner for debugging brownouts / wake-up sources / PSRAM detection. Opt-in keeps the default behavior friendly.

## Situational notes

- **Stages 1+2 stack with `FASTLED_RMT_STATIC_ALLOCATION`** — they target different parts of the binary (the FL_WARN string pool vs. the dynamic RMT scaffolding). Combining all three is the smallest-build path.
- **The sdkconfig overlay only fires if your project's `sdkconfig_defaults` references it.** Adding the overlay file to your library install is not enough; you have to list it under `board_build.sdkconfig_defaults`.
- **`PROJECT_LIBDEPS_DIR` resolution varies by PIO project layout.** If the substitution above doesn't resolve, fall back to the absolute path to the installed FastLED package.

## Measuring your build

The canonical measurement command:

```bash
bash bloat esp32s3 --top 25
```

This emits:
- `.build/symbols/esp32s3/report.json` — per-symbol attribution.
- `.build/symbols/esp32s3/report.md` — human-readable top-N table.

Compare two builds with `uv run python .claude/symbolaudit/diff.py <old.json> <new.json>` (added / removed / grew / shrunk symbols per archive). The full tooling reference is in [`agents/docs/binary-size-analysis.md`](../agents/docs/binary-size-analysis.md).

## Roadmap

Stages still in progress on the multi-stage plan tracked in [#2886](https://github.com/FastLED/FastLED/issues/2886):

- **Stage 4** — Cold-helper audit of the remaining FastLED hot paths (`Channel::showPixels`, `ChannelEngineRMTImpl::reconfigureForNetwork`, `ChannelManager::addDriver`, `RmtMemoryManager::handleAllocateTxFailure`). ~3-5 KB projected.
- **Stage 5** — `fl::basic_string::write` simplification. ~0.5-1 KB projected.
- **Stage 6** — CI verification gate (`tests/test_esp32s3_bloat_regression.sh`) that asserts `bash bloat esp32s3` total ≤ 280,000 B. Flips the per-knob ✅/📊 status flags on this page to measured values.

## Related

- **[FastLED #2886](https://github.com/FastLED/FastLED/issues/2886)** — multi-stage bloat-reduction meta with per-symbol attribution.
- **[`agents/docs/binary-size-analysis.md`](../agents/docs/binary-size-analysis.md)** — agent-facing reference for the `bash bloat` tooling.
- **[FastLED #2890](https://github.com/FastLED/FastLED/pull/2890)** — Stage 1 (FASTLED_LOG_VERBOSITY default flip).
- **[FastLED #2894](https://github.com/FastLED/FastLED/pull/2894)** — Stage 2 (Arduino-ESP32 boot-banner shim).
- **[FastLED #2896](https://github.com/FastLED/FastLED/pull/2896)** — Stage 3 (sdkconfig overlay).
- **[FastLED #2846](https://github.com/FastLED/FastLED/pull/2846)** — `FASTLED_RMT_STATIC_ALLOCATION` opt-in (pre-existing).
- **[FastLED #2773](https://github.com/FastLED/FastLED/issues/2773)** — original binary-size meta (closed, predecessor to #2886).
