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
| Current default-flag build (post-Stage-1 + recent micro-optimizations) | **339,962 B flash** (−48,418 B / −12.5 %) | measured 2026-06-07 against `master @ f43f76701a` |
| **Stage 3 overlay alone (`tools/sdkconfig_for_smallest_fastled.defaults`)** | **280,738 B flash** (−107,642 B / −27.7 %) | measured 2026-06-07 — **hits the #2886 end goal at row 2 with one knob** |
| Stage 6 target | ≤ 280,000 B (−28 %) | #2886 goal |

To measure your own build: `bash bloat esp32s3 --build` then `jq '.total_flash' .build/symbols/esp32s3/report.json`. Tool details live in [`agents/docs/binary-size-analysis.md`](../agents/docs/binary-size-analysis.md).

## Release-build flash savers (in priority order)

| # | Lever | How to enable | Savings | Status | PR |
|---:|---|---|---:|:---:|---|
| 1 | `FASTLED_LOG_VERBOSITY=0` | **Default on release builds** (NDEBUG); `-DFASTLED_LOG_VERBOSITY=1` to restore | **−37,812 B from this flag alone** + post-Stage-1 cascade (see below) totalling **≈ −50,391 B** (388,380 → 337,989 B on master post-#2957) | ✅ | #2890 + cascade |
| 2 | `tools/sdkconfig_for_smallest_fastled.defaults` | `board_build.sdkconfig_defaults` in `platformio.ini`. **Includes `CONFIG_NEWLIB_NANO_FORMAT=y`** which drops the standard newlib printf cluster — see Stage 3 detail below. | **−59,224 B measured** (339,962 → 280,738 B on `f43f76701a`) | ✅ | #2896 + #2915 |
| 3 | `-DFASTLED_RMT_STATIC_ALLOCATION=1` | `build_flags`; for sketches that init LEDs in `setup()` and never `removeLeds()` | **−908 B measured** (339,962 → 339,054 B; well below projection) | ✅ | #2846 |
| 4 | `-DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1` | `build_flags`; strong-overrides the Arduino-ESP32 boot-banner gate | ~3 KB | 📊 | #2894 |
| 5 | `CONFIG_BT_ENABLED=n` | uncomment the situational block in `tools/sdkconfig_for_smallest_fastled.defaults` | ~15 KB (if currently on) | 📊 | — |
| 6 | `-DFASTLED_DISABLE_SPI_CHIPSETS=1` | `build_flags`; drops the SPI dispatch branch in `Channel::showPixels`. **Constraint:** clockless-only sketches; calling `FastLED.addLeds<APA102, ...>` (or any SPI chipset) under this flag silently emits nothing. | ~1.0-1.2 KB | 📊 | #2913 |
| 7 | `-DFASTLED_DISABLE_UCS7604=1` | `build_flags`; drops the UCS7604 cases in `Channel::showPixels`'s clockless switch. **Constraint:** WS2812-only sketches; calling `FastLED.addLeds<UCS7604, ...>` under this flag silently emits nothing. | **−3,804 B measured** (339,962 → 336,158 B; 6-9× projection) | ✅ | #2920 |
| 8 | `-DFASTLED_DISABLE_DYNAMIC_DRIVER=1` | `build_flags`; gates out `Channel::resolveDynamicDriver()` and its `ChannelManager::findDriverByName` / `selectDriverForChannel` lookup chain. **Constraint:** legacy `addLeds<>` flow only (every `addLeds<>` flavor pre-binds in its ctor). Channels created via manager-based `Channel::create(cfg)` without pre-binding silently emit nothing. | **−937 B measured** (339,962 → 339,025 B) | ✅ | #2926 |
| 9 | `-DFASTLED_DISABLE_CHANNEL_EVENTS=1` | `build_flags`; replaces the 7 `fl::function_list` event slots in `fl::ChannelEvents` with no-op fallbacks. **Constraint:** user-registered listeners via `events.onChannelXxx.add(...)` are silently dropped — the registration compiles (returns -1) but the callback never fires. | **−8,034 B measured** (339,962 → 331,928 B; 4-8× projection) | ✅ | #2931 |

**Legend:** ✅ measured against a recorded baseline · 📊 projected from the top-25 symbol attribution in #2886.

Row 1 is empirically confirmed by the 2026-06-06 audit (see the [#2886 audit comment](https://github.com/FastLED/FastLED/issues/2886#issuecomment-4641413123) and the Stage 6 gate baseline at `tests/data/esp32s3_bloat_baseline.txt`). Rows 2-5 stay 📊 until a measured build with their flag / overlay enabled is recorded.

> **Row 1 cascade.** PRs #2908/#2911/#2918/#2925/#2929/#2943/#2951/#2953 stacked
> diagnostic-only cold-helper gates on top of #2890's Stage 1. Each individually
> projected 100-800 B; several exceeded projection 4-10× thanks to dead-strip
> cascades on supporting template machinery (notably #2918 −2,235 B, #2925
> −3,937 B, #2929 −870 B, #2943 −1,845 B). Cumulative measured saving on top of
> Stage 1: **−12,603 B**, bringing the total from the pre-Stage-1 baseline to
> **≈ −50 KB / −13%**.

## Per-stage detail

### 1. `FASTLED_LOG_VERBOSITY=0` (the big one)

**Mechanism:** `src/fl/log/log.h:66-92`. The unset default resolves as `FASTLED_TESTING` → 1, `NDEBUG` → 0, otherwise → 1. At level 0 the `FL_WARN` / `FL_INFO` / `FL_ERROR` / `FL_PRINT` / `FL_DBG` macros expand to `do {} while(0)`, so the optimizer drops the formatted-stream operator chain and the linker drops every literal carried with it.

**Where the savings come from (per #2886 top-25):**

- ~58 KB from the `ClocklessIdf5` ctor's transitive `FL_WARN` rodata pool (75 sites in `channel_driver_rmt.cpp.hpp`, 59 in `rmt_memory_manager.cpp.hpp`, 6 in `manager.cpp.hpp`, 5 in `channel.cpp.hpp`).
- Additional ~5-10 KB scattered across smaller call sites (`createChannel`, `reconfigureForNetwork`, `handleAllocateTxFailure`, `Rmt5EncoderImpl::initialize`).

**To restore on a release build** (e.g. for field debugging): add `-DFASTLED_LOG_VERBOSITY=1` to `build_flags`.

**To force off on a non-release build** (e.g. measuring savings without an NDEBUG rebuild): add `-DFASTLED_LOG_VERBOSITY=0` to `build_flags`.

### 2. `tools/sdkconfig_for_smallest_fastled.defaults`

**Mechanism:** ESP-IDF sdkconfig overlay applied via `board_build.sdkconfig_defaults`. The overlay flips five knobs:

| Knob | Disables | Savings |
|---|---|---:|
| `CONFIG_NEWLIB_NANO_FORMAT=y` | standard newlib printf cluster (`_vfprintf_r`, `_svfprintf_r`, `_svfiprintf_r`, `_vfiprintf_r`, `_dtoa_r`, `get_arg$isra$0`) | **~20-30 KB** |
| `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=n` | libespcoredump (top-25 rows #18 + #25) | ~8 KB |
| `CONFIG_LOG_DEFAULT_LEVEL=ESP_LOG_NONE` | libesp_diagnostics + IDF log strings (row #19) | ~3 KB |
| `CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y` | bootloader-stage UART chatter | ~1 KB |
| `CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y` | panic-time backtrace formatter | ~1-2 KB |

The overlay file at [`../tools/sdkconfig_for_smallest_fastled.defaults`](../tools/sdkconfig_for_smallest_fastled.defaults) carries the full per-knob commentary inline.

**`CONFIG_NEWLIB_NANO_FORMAT=y` constraint:** nano printf has no `%f` / `%lf` / `%lld` / positional-arg support. Sketches that print floating-point values via printf will see literal format specifiers in the output instead of the formatted number. Most LED installations don't print floats; if yours does, either drop the overlay entirely or layer your own sdkconfig overlay AFTER FastLED's with `CONFIG_NEWLIB_NANO_FORMAT=n` to override.

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

Status of every Stage on the multi-stage plan tracked in [#2886](https://github.com/FastLED/FastLED/issues/2886):

- **Stage 0–3, 6** — ✅ shipped (see the [Per-stage detail](#per-stage-detail) section above and the [Related](#related) links below).
- **Stage 4** — ✅ audited / no action (#2901). The four targets were authored before Stage 1 collapsed the FL_WARN string pool; re-audited against post-Stage-1 master, each is either already correctly gated or has nothing further to recover:
  - `Channel::showPixels` — the `has_no_runtime_scaling_v<>` trait the #2886 description named does not exist in the source. The actual dispatch in `src/fl/chipsets/encoders/pixel_iterator.h:202-216` is a three-way runtime branch against compact 5-line encoders (`encodeWS2812_RGB` / `_RGBW` / `_RGBWW` in `src/fl/chipsets/encoders/ws2812.h:35-96`). The encoder bodies are too small to be worth a build-flag-gated removal.
  - `ChannelEngineRMTImpl::reconfigureForNetwork` — body at `src/platforms/esp/32/drivers/rmt/rmt_5/channel_driver_rmt.cpp.hpp:1128-1236` is the channel destroy / recreate sequence around a Network state transition. Of 10 diagnostic emissions, 8 are FL_DBG (already gated by `FASTLED_HAS_DBG`) and 2 are FL_WARN (no-op'd by Stage 1 in release builds). The remaining bytes are essential peripheral teardown.
  - `ChannelManager::addDriver` — the entire `capStr` builder block at `src/fl/channels/manager.cpp.hpp:117-132` is already wrapped in `#if FASTLED_HAS_DBG`. Verification only; no fix needed.
  - `RmtMemoryManager::handleAllocateTxFailure` — at `src/platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.cpp.hpp:417-478`, already a `FL_NO_INLINE` cold helper. The 5 FL_LOG_RMT + 10 FL_WARN sites are no-op'd by `FASTLED_LOG_VERBOSITY=0`, and the size_t computations at lines 451-454 that fed those FL_WARNs are DCE'd by the optimizer.
- **Stage 5** — ✅ audited / no action (#2901). The body at `src/fl/stl/basic_string.cpp.hpp:193-283` is the COW + inline-vs-heap storage state machine: four branches (non-owning, unique-heap, COW-heap, inline + inline→heap transition). No formatted-stream machinery in the body to drop; the projected ~0.5-1 KB overestimated.

### Next round of structural savings — see [#2856](https://github.com/FastLED/FastLED/issues/2856)

The #2886 stages have squeezed every release-build flash-saver knob and audit target available from the current architecture. The next round of *real* (multi-KB) savings is structural and tracked separately in [#2856 "Meta: ESP32-S3 binary-size reduction — Batch 3"](https://github.com/FastLED/FastLED/issues/2856), covering items 3.1–3.6 (template-trait static dispatch for `Channel::showPixels`, printf-style log backend, `ChannelManager` static-bind path, RMT5 encoder dedup, `fl::result<T,E>` → status code for internal APIs, fixed-size allocation ledger under `FASTLED_RMT_STATIC_ALLOCATION`). Several of those items are already shipped or in flight on `master`.

## Related

- **[FastLED #2886](https://github.com/FastLED/FastLED/issues/2886)** — multi-stage bloat-reduction meta with per-symbol attribution.
- **[`agents/docs/binary-size-analysis.md`](../agents/docs/binary-size-analysis.md)** — agent-facing reference for the `bash bloat` tooling.
- **[FastLED #2890](https://github.com/FastLED/FastLED/pull/2890)** — Stage 1 (FASTLED_LOG_VERBOSITY default flip).
- **[FastLED #2894](https://github.com/FastLED/FastLED/pull/2894)** — Stage 2 (Arduino-ESP32 boot-banner shim).
- **[FastLED #2896](https://github.com/FastLED/FastLED/pull/2896)** — Stage 3 (sdkconfig overlay).
- **[FastLED #2846](https://github.com/FastLED/FastLED/pull/2846)** — `FASTLED_RMT_STATIC_ALLOCATION` opt-in (pre-existing).
- **[FastLED #2856](https://github.com/FastLED/FastLED/issues/2856)** — Batch 3 structural meta (the next round of multi-KB savings, beyond the knob/audit work of #2886).
- **[FastLED #2773](https://github.com/FastLED/FastLED/issues/2773)** — original binary-size meta (closed, predecessor to #2886).
