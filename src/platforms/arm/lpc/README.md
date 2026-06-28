# FastLED Platform: NXP LPC

[![Build LPC845](https://github.com/FastLED/fbuild/actions/workflows/build-lpc845.yml/badge.svg)](https://github.com/FastLED/fbuild/actions/workflows/build-lpc845.yml)
[![Build LPC804](https://github.com/FastLED/fbuild/actions/workflows/build-lpc804.yml/badge.svg)](https://github.com/FastLED/fbuild/actions/workflows/build-lpc804.yml)

> **LPC11Uxx** and **LPC15xx** compile against the same `clockless_arm_lpc.h` / `fastpin_arm_lpc.h` driver headers as LPC845/LPC804 (sharing the modern 0xA0000000 GPIO controller layout). Dedicated CI workflows for those families are tracked under FastLED/fbuild [#456](https://github.com/FastLED/fbuild/issues/456); until then their build status mirrors LPC845's.

NXP LPC microcontroller family support. Initial port landed in #2837 ([meta #2836](https://github.com/FastLED/FastLED/issues/2836)); the second-batch roadmap meta [#2845](https://github.com/FastLED/FastLED/issues/2845) — Stages 3 + 4 — has been closed with the bulk of Stage 4 shipped in #2872 + #2876; the remaining work is split across per-repo follow-ups (see the implementation map below).

## Platform Overview

The LPC family currently covers two ARM cores:

- **Cortex-M0+** (LPC845, LPC804) — bare-metal target via [fbuild](https://github.com/FastLED/fbuild) ≥ v2.2.18. Bit-banged clockless driver shipped; optional hardware-assisted drivers available per chip.
- **Cortex-M0** (LPC11xx) and **Cortex-M3** (LPC15xx) — family **detection** is wired (`FL_IS_ARM_LPC_11`, `FL_IS_ARM_LPC_15`), but full driver wiring (`led_sysdefs`, `fastpin`, clockless) is pending hardware bring-up. See [#2845](https://github.com/FastLED/FastLED/issues/2845) Stage 4.

### Supported Platforms

| Platform | Chip | CPU | Default Clock | Drivers Shipped | Status |
|----------|------|-----|---------------|-----------------|--------|
| **LPC845** | LPC845M301 | Cortex-M0+ @ 30 MHz | 30 MHz | Bit-bang (default) + optional SCT/PWM+DMA (#2850) | ✅ Compiles + AutoResearch RPC bring-up sketch ([#3041](https://github.com/FastLED/FastLED/pull/3041)); AutoResearch loopback verification pending [#2880](https://github.com/FastLED/FastLED/issues/2880) |
| **LPC804** | LPC804M101 | Cortex-M0+ @ 15 MHz | 15 MHz | Bit-bang (default) + optional PLU (#2848) | ✅ Compiles + AutoResearch RPC bring-up sketch ([#3041](https://github.com/FastLED/FastLED/pull/3041)); AutoResearch loopback verification pending [#2880](https://github.com/FastLED/FastLED/issues/2880) |
| **LPC11Uxx** | LPC11U24, LPC11U35 | Cortex-M0 | 12 MHz (IRC) | Shared LPC8xx fastpin + M0 C++ clockless (#2872) | ✅ Compiles; hardware bring-up pending |
| **LPC11xx legacy** | LPC1110, LPC1112, LPC1114, LPC1115 | Cortex-M0 | — | Dedicated `fastpin_arm_lpc11_legacy.h` ([#2878](https://github.com/FastLED/FastLED/pull/2878)) + shared M0 C++ clockless | ✅ Compiles; hardware bring-up pending |
| **LPC15xx** | LPC1517…LPC1549 | Cortex-M3 | 12 MHz (IRC) | Shared LPC8xx fastpin + M3-compatible C++ clockless (#2872) | ✅ Compiles; hardware bring-up pending |

## Hardware verification via AutoResearch loopback ([#2880](https://github.com/FastLED/FastLED/issues/2880))

**No maintainer sign-off required.** Verification is fully automated: wire a TX↔RX jumper on the LPC845-BRK and run AutoResearch. The Python orchestrator asserts pass/fail from the JSON-RPC results that come back over the same serial link.

Loopback setup:

1. `fbuild build lpc845 --examples AutoResearch` flashes the consolidated `examples/AutoResearch/AutoResearch.ino` (its low-memory mode auto-engages on LPC8xx via `FL_PLATFORM_HAS_LARGE_MEMORY == 0`).
2. Jumper TX pin → RX pin externally on the LPC845-BRK header (default: `P0_10` → `P0_11`).

> **Pin discovery is on by default.** `bash autoresearch <board>` auto-sweeps the GPIO matrix for wired loopback pairs before running any driver test — no `--tx-pin`/`--rx-pin` flags required for the canonical fixture. Pass `--no-auto-discover-pins` to opt out (e.g. for known-bad wiring debug or CI determinism). Explicit `--tx-pin <N>`/`--rx-pin <N>` overrides keep working. See [#3296](https://github.com/FastLED/FastLED/issues/3296).

What the harness asserts:

- **`bash autoresearch lpc845brk --bring-up`** — `echo` RPC round-trips; FL_WARN literal reaches the host; proves Serial + JSON-RPC + log pipeline intact.
- **`bash autoresearch lpc845brk --pin-toggle-rx`** — SCT input-capture latches a bit-banged square wave; orchestrator asserts mean ±2 % and σ thresholds across 1/10/100 kHz rates ([#3035](https://github.com/FastLED/FastLED/issues/3035) Phase 1).
- **`bash autoresearch lpc845brk --ws2812-loopback`** — WS2812 byte-match: `FastLED.show()` drives 1/3/100 LEDs through the bit-bang clockless path, SCT-RX latches the wire, decoder asserts `mismatched == 0` ([#3035](https://github.com/FastLED/FastLED/issues/3035) Phase 2b).
- **Link-symbol check** — `arm-none-eabi-nm -C firmware.elf | grep -E '(aeabi_d|aeabi_f|f2iz|d2iz|__l2f|__floatdisf)'` stays empty after [#3038](https://github.com/FastLED/FastLED/pull/3038). The harness can fold this into its reporting path so the no-soft-FP invariant is asserted on every run.

Once all four run green against an LPC845-BRK with the loopback jumper, [#2880](https://github.com/FastLED/FastLED/issues/2880) closes and the table above flips its LPC845 / LPC804 rows to "✅ hardware verified". No human-eyeball scope trace required.

## Register-map authoring note (issue #2990)

`clockless_arm_lpc_pwm_dma.h` defines tier-3 fallback shim structs
(`FL_LPC_SCT_Shim`, `FL_LPC_DMA_Shim`, `FL_LPC_SYSCON_Shim`) for build
configurations where NXP's `<LPC845.h>` is not on the include path.
Those shims have shipped wrong offsets twice (#3349 fixed `SYSCON`
`_resv0[16]→[32]` and `DMA_CHANNEL.CFG` bit composition). Any further
edits to those structs — or to any new LPC register access — MUST follow
`agents/docs/register-maps.md`: cite both the UM section and the
vendor CMSIS member name on every line, gate the shim behind the
vendor typedef's include guard, and spot-check three offsets against
the real header before merging.

The proper long-term fix is for the **`zackees/ArduinoCore-LPC8xx`
platform package to ship NXP's full `LPC845.h` CMSIS PAL** (typedefs
for `SCT_Type`, `DMA_Type`, `SYSCON_Type`, etc.) in
`variants/lpc845/LPC845.h`. The version currently shipped is a 54-line
stub containing only IRQ enums + CMSIS-Core configuration constants —
no peripheral typedefs — which is why the FastLED shims exist in the
first place. Once the upstream package provides the full vendor
header, FastLED's `led_sysdefs_arm_lpc.h` can `#include <LPC845.h>`
directly (tier 1 per `agents/docs/register-maps.md`) and all eight
shim/raw-offset sites listed below can be migrated to vendor typedefs
without FastLED vendoring anything.

### Hand-rolled register-map remediation list (PR #3349 follow-up)

> **Tracking issue:** [#3437](https://github.com/FastLED/FastLED/issues/3437) — meta for replacing every site below with vendor CMSIS PAL headers.

@phatpaul (PR [#3349](https://github.com/FastLED/FastLED/pull/3349#issuecomment-4811566535)):
*"I want to remove all the hand-rolled register maps and require the
use of the vendor provided CMSIS PAL files to build."* Full audit of
what needs to move to vendor headers before that PR can finish.

**Tier-1 path (preferred per `agents/docs/register-maps.md`):** wait
for `zackees/ArduinoCore-LPC8xx` to ship NXP's full `LPC845.h` (and
`LPC804.h`, `LPC11xx.h`) under `variants/<chip>/`. Then FastLED
includes the vendor header directly from `led_sysdefs_arm_lpc.h` and
deletes every shim/raw-offset row below. **No vendoring inside
FastLED.**

**Tier-2 fallback (FastLED-side vendoring):** only if the upstream
package declines to ship the full header — copy NXP's
[mcux-sdk](https://github.com/nxp-mcuxpresso/mcux-sdk) `devices/<chip>/<chip>.h`
into `src/platforms/arm/lpc/third_party/cmsis/` (BSD-3-Clause; the
`third_party/` path component is required so the FastLED linter
exempts it from house-style rules), include from
`led_sysdefs_arm_lpc.h`, then delete every row below.

#### A. Hand-rolled struct shims (CMSIS-style typedef + `LPC_*` pointer)

These are the dangerous pattern called out in `agents/docs/register-maps.md` —
re-typed from the user manual, prone to silent offset bugs.

| # | Symbol | Location | Peripheral | Replace with |
|---|--------|----------|------------|--------------|
| 1 | `FL_LPC_SCT_Shim` + `#define LPC_SCT` | `clockless_arm_lpc_pwm_dma.h:225,269` | LPC845 SCT (UM11029 §16) | `LPC_SCT_Type` from `LPC845.h` |
| 2 | `FL_LPC_DMA_Shim` + `FL_LPC_DMA_ChannelShim` + `#define LPC_DMA` | `clockless_arm_lpc_pwm_dma.h:273,279,310` | LPC845 DMA (UM11029 §17) | `LPC_DMA_Type` from `LPC845.h` |
| 3 | `FL_LPC_SYSCON_Shim` + `#define LPC_SYSCON` | `clockless_arm_lpc_pwm_dma.h:314,321` | LPC845 SYSCON (UM11029 §4.6) | `LPC_SYSCON_Type` from `LPC845.h` |
| 4 | `FL_LPC_SPI_Type` (no `LPC_SPI*` define — driver casts pSPIX itself) | `spi_arm_lpc.h:60-72` | LPC8xx SPI0/SPI1 (UM11029 §"SPI") | `LPC_SPI_Type` from `LPC845.h` / `LPC804.h` |
| 5 | `FL_LPC11_LEGACY_GPIO_Type` + `#define LPC_GPIO0..3` | `fastpin_arm_lpc11_legacy.h:57-82` | LPC11xx legacy GPIO @ 0x50000000 (UM10398 §9) | `LPC_GPIO_Type` from `LPC11xx.h` / `LPC1100.h` |

#### B. Raw `base + offset` MMIO with embedded register-map knowledge

Not literal structs, but the same problem in flatter form: numeric base
addresses + hand-typed offset constants + `reg(base, off)` helpers. Every
offset is a hand-typed source of truth that a vendor header would
generate.

| # | Location | Peripheral(s) | Replace with |
|---|----------|---------------|--------------|
| 6 | `FL_LPC_GPIO_*_OFFSET` constants + `lpc_gpio_*()` fallback helpers, `fastpin_arm_lpc.h:55-86` | Modern LPC GPIO @ 0xA0000000 (LPC8xx / LPC11Uxx / LPC15xx) | File already prefers vendor `LPC_GPIO` when on the include path — once CMSIS is required, delete the `#else` fallback path |
| 7 | `kPluBase`, `kSysconBase`, `kOff*` offsets + `reg32(base, off)` helper, `clockless_arm_lpc_plu.h:82-180` | LPC804 PLU (UM11065 §12) + SYSCON | `LPC_PLU_Type`, `LPC_SYSCON_Type` from `LPC804.h` |
| 8 | `kSctBase`, `kDmaBase`, `kSwmBase`, `kSysconBase`, dozens of `kOff*` constants + `reg(base, off)` helper, `rx_sct_capture.cpp.hpp:80-220` | LPC845 SCT + DMA + SWM + SYSCON | `LPC_SCT_Type`, `LPC_DMA_Type`, `LPC_SWM_Type`, `LPC_SYSCON_Type` from `LPC845.h` |

**Total: 8 hand-rolled register-access sites across 6 files** —
`clockless_arm_lpc_pwm_dma.h`, `spi_arm_lpc.h`,
`fastpin_arm_lpc11_legacy.h`, `fastpin_arm_lpc.h`,
`clockless_arm_lpc_plu.h`, `rx_sct_capture.cpp.hpp`.

Closing this list = deleting all eight, replacing every access site
with the vendor typedef'd pointer, and bumping
`agents/docs/register-maps.md` to retire LPC8xx as its
worked anti-example.

## Files (quick pass)

- `fastled_arm_lpc.h` — Aggregator; selects between bit-bang / PLU / PWM+DMA per chip and opt-in macro.
- `fastpin_arm_lpc.h` — Pin helpers. `_ARMPIN` template is **scoped to LPC845 / LPC804** ([#2866](https://github.com/FastLED/FastLED/pull/2866)) because LPC11xx and LPC15xx ship different GPIO controller layouts.
- `clockless_arm_lpc.h` — Bit-banged WS2812-family driver built on `arm/common/m0clockless.h` (C++ implementation, since LPC8xx GPIO SET/CLR offsets exceed the M0+ STR-immediate encoding range).
- `clockless_arm_lpc_plu.h` — LPC804-only Programmable Logic Unit (PLU) clockless driver. Hardware pulse-shaping via 26-LUT reconfigurable fabric; CPU only writes serial data per bit. See UM11065 §12.
- `clockless_arm_lpc_pwm_dma.h` — LPC845-only SCT + DMA-to-GPIO clockless driver. CPU-free WS2812 output via three DMA channels (T0_RISE / T_MID / T_END). See UM11029 §16-17.
- `spi_arm_lpc.h` — LPC845 / LPC804 hardware SPI driver for **APA102 / SK9822 / WS2801** clocked strips. Targets the LPC8xx SPI peripheral (UM11029 §"SPI") in master / MSB-first / mode-0 / 8-bit configuration. SPI0 default; users route to SPI1 via the `pSPIX` template arg. Pin routing through the LPC Switch Matrix is the user's responsibility (matching the bit-bang clockless driver's "FastPin sees raw GPIO" convention). Closes #2845 Stage 4 item 3.
- `led_sysdefs_arm_lpc.h` — System defines (sets `FL_IS_ARM_M0_PLUS`, `F_CPU`, forces `FASTLED_M0_USE_C_IMPLEMENTATION`, includes `<LPC845.h>` / `<LPC804.h>` CMSIS device headers).
- `fastpin_arm_lpc.h` — see above.
- `is_lpc.h` — Detection macros (`FL_IS_ARM_LPC_845`, `FL_IS_ARM_LPC_804`, `FL_IS_ARM_LPC_11`, `FL_IS_ARM_LPC_15`, `FL_IS_ARM_LPC`).
- `int.h` — Integer type definitions.

## Optional feature defines

Build-time opt-ins (default off). Define before including `FastLED.h` or via build flags.

- **`FASTLED_LPC_PLU`** — LPC804 only. Activates the PLU clockless driver in place of the bit-banged default. See `clockless_arm_lpc_plu.h` for the LUT graph + register cites (UM11065 §12.6.1–12.6.4).
- **`FASTLED_LPC_PWM_DMA`** — LPC845 only. Activates the SCT + DMA-to-GPIO clockless driver in place of the bit-banged default. Claims 3 DMA channels and the SCT for the lifetime of the FastLED controller.
- **`FASTLED_LPC_PWM_DMA_BASECH`** — LPC845 + `FASTLED_LPC_PWM_DMA`. Base index of the 3 contiguous DMA channels (default `0`, i.e. channels 0/1/2).
- **`FASTLED_LPC_PWM_DMA_CHUNK_BITS`** — LPC845 + `FASTLED_LPC_PWM_DMA`. SRAM-budgeted streaming chunk size in bits (default `64`); a 144-LED encode fits in 16 KB SRAM with the default.
- **`FASTLED_M0_USE_C_IMPLEMENTATION`** — Already set unconditionally by `led_sysdefs_arm_lpc.h`. The LPC8xx GPIO controller exposes SET[port] and CLR[port] at byte offsets `0x2200 / 0x2280` from the controller base, beyond the 5-bit imm5*4 encoding the M0/M0+ STR-immediate-offset instruction supports. The shared inline-assembly clockless driver assumes both offsets fit a single str-with-immediate; LPC routes through the portable C++ implementation, which performs an indexed store instead.
- **`FASTLED_ALLOW_INTERRUPTS`** — Default `1`.
- **`FASTLED_USE_PROGMEM`** — Default `0`.

## Build system

LPC8xx targets are built with [fbuild](https://github.com/FastLED/fbuild) (no PlatformIO upstream for `lpc845` / `lpc804`):

```bash
fbuild build lpc845 --examples Blink
fbuild build lpc804 --examples Blink
```

The fbuild assets (board JSON, linker script, SystemInit, vector table) live in `crates/fbuild-config/assets/boards/json/lpc8{04,45}.json` and `crates/fbuild-build/src/nxplpc/assets/` in the fbuild repo. The board-validation script ([fbuild#421/#422](https://github.com/FastLED/fbuild/issues/421)) explicitly tallies these as fbuild-native (no PlatformIO upstream).

For LPC11xx and LPC15xx, no fbuild board entry exists yet; community contributors targeting those families with their own PlatformIO platform should expect to wire `led_sysdefs`, `fastpin`, and clockless headers per family before FastLED can compile against them. The hold is documented in `fastled_arm_lpc.h` — attempting to build with only `FL_IS_ARM_LPC_11` or `FL_IS_ARM_LPC_15` set emits a clear `#error` pointing here.

## Detection scaffolds

- **LPC11xx (Cortex-M0)** — `is_lpc.h` recognises `__LPC11xx__`, `CPU_LPC1114*`, `CPU_LPC1115*`, `CPU_LPC11U24*`, `CPU_LPC11U35*` and similar. LPC11xx is **Cortex-M0**, not M0+ — do not gate any code path on `FL_IS_ARM_M0_PLUS` when adding the driver. Driver wiring will reuse `arm/common/m0clockless_asm.h` (the same path nRF51 uses); the GPIO controller is the legacy "masked-access" peripheral at `0x50000000` (UM10398 §9 / UM10462 §9), so a new `fastpin_arm_lpc11.h` is required — the LPC8xx 0xA0000000 SET/CLR template does not apply.
- **LPC15xx (Cortex-M3)** — `is_lpc.h` recognises `__LPC15xx__` and `CPU_LPC15{17,18,19,47,48,49}*`. LPC15xx is **Cortex-M3** with the full Thumb-2 instruction set; do not gate on `FL_IS_ARM_M0` or `FL_IS_ARM_M0_PLUS`. Driver wiring should target the M3 clockless template; the GPIO controller layout per UM11074 differs again from both LPC8xx and LPC11xx.

## Implementation map — where each #2845 item landed

The LPC roadmap meta [#2845](https://github.com/FastLED/FastLED/issues/2845) **closed** alongside this PR after Stage 4 items 1, 2, and 3 shipped. Remaining items are tracked as per-repo follow-up issues so progress on each one is not gated on the others. This table is the canonical map.

| Stage / Item | Owner repo | Status | Where it lives now |
|---|---|---|---|
| **Stage 3.1** — Real `SystemInit` for LPC845 (FRO 30 MHz + PLL + flash wait states) | **`FastLED/fbuild`** | Open | [fbuild #456](https://github.com/FastLED/fbuild/issues/456) (Stage 3 fbuild meta) |
| **Stage 3.2** — Real `SystemInit` for LPC804 | **`FastLED/fbuild`** | Open | [fbuild #456](https://github.com/FastLED/fbuild/issues/456) |
| **Stage 3.3** — Vector table expansion (DMA-end IRQ, SCT match IRQ, USART RX) | **`FastLED/fbuild`** | Open | [fbuild #456](https://github.com/FastLED/fbuild/issues/456) |
| **Stage 3.4** — Per-chip `mcu_config` split in `get_nxplpc_config` | **`FastLED/fbuild`** | Open | [fbuild #456](https://github.com/FastLED/fbuild/issues/456) |
| **Stage 3.5** — Linker memory-layout verification on hardware | Hardware | Field sign-off pending | [#2880](https://github.com/FastLED/FastLED/issues/2880) (LPC845 bring-up) |
| **Stage 3.6** — `examples/AutoResearch/AutoResearch.ino` UART integration | **FastLED** | ✅ Shipped (consolidated via [#3041](https://github.com/FastLED/FastLED/pull/3041) / closes [#3030](https://github.com/FastLED/FastLED/issues/3030)) | `AutoResearch.ino` low-memory mode auto-engages on LPC8xx (`FL_PLATFORM_HAS_LARGE_MEMORY == 0`); same `echo` / `pinToggleRx` / `ws2812SctTest` RPC contract as the retired `AutoResearchLpc.ino`. |
| **Stage 3.7** — fbuild CI `continue-on-error` flip | **`FastLED/fbuild`** | Blocked on 3.1 + 3.5 | [fbuild #456](https://github.com/FastLED/fbuild/issues/456) |
| **Stage 3.8** — `validate_boards.py` reconciliation | **`FastLED/fbuild`** | Open | [fbuild #421/#422](https://github.com/FastLED/fbuild/issues/421) + [fbuild #456](https://github.com/FastLED/fbuild/issues/456) |
| **Stage 4.1** — LPC11Uxx clockless driver | **FastLED** | ✅ Shipped in [#2872](https://github.com/FastLED/FastLED/pull/2872) | Reuses LPC8xx fastpin + M0 C++ clockless |
| **Stage 4.1 (legacy)** — LPC1110/1112/1114/1115 clockless | **FastLED** | Open | [#2878](https://github.com/FastLED/FastLED/issues/2878) (legacy 0x50000000 GPIO fastpin follow-on) |
| **Stage 4.2** — LPC15xx clockless driver | **FastLED** | ✅ Shipped in [#2872](https://github.com/FastLED/FastLED/pull/2872) | Reuses LPC8xx fastpin + M3-compatible C++ clockless |
| **Stage 4.3** — APA102 / SK9822 / WS2801 hardware SPI | **FastLED** | ✅ Shipped in [#2872](https://github.com/FastLED/FastLED/pull/2872) + CR fixes in [#2876](https://github.com/FastLED/FastLED/pull/2876) | `spi_arm_lpc.h` per UM11029 |
| **Stage 4.4** — Multi-strip parallel output | **FastLED** | Blocked on Stage 2c hardware validation | [#2879](https://github.com/FastLED/FastLED/issues/2879) |
| **Stage 4.5** — PlatformIO upstream donation | **PlatformIO** (3rd party) | Deferred (no user-facing PIO platform exists for nxplpc) | — |
| **Stage 4.6** — `fl::set_*` settings for LPC clock-speed / DMA-channel overrides | **FastLED** | Deferred (no concrete user requirement) | Reopen with a concrete user report; the build-time `F_CPU` override already covers the stable-clock case |

**Distribution:** of the 15 items, **3 shipped** (Stage 4.1, 4.2, 4.3 — all in #2872 + #2876), **6 live in `FastLED/fbuild`** (folded into fbuild #456), **3 are hardware-gated** (FastLED #2880, FastLED #2879), **1 is a code-only FastLED follow-on** (#2878, the legacy LPC11xx fastpin), and **2 are deferred** (4.5 PIO donation, 4.6 settings — neither has a concrete requirement today).

Closing the meta does not stall any open work — every still-open item is now in a smaller, single-owner issue that can be picked up independently.

## References

### Meta + status

- [#2836](https://github.com/FastLED/FastLED/issues/2836) — Meta: original LPC8xx port plan (closed)
- [#2845](https://github.com/FastLED/FastLED/issues/2845) — Meta: LPC dev roadmap, Stages 3 + 4 (closed by the PR that ships this README)

### Shipped

- [#2837](https://github.com/FastLED/FastLED/pull/2837) — Stage 2a: bit-banged M0+ clockless driver
- [#2848](https://github.com/FastLED/FastLED/pull/2848) — Stage 2b: LPC804 PLU clockless driver
- [#2850](https://github.com/FastLED/FastLED/pull/2850) — Stage 2c: LPC845 SCT/PWM+DMA clockless driver
- [#2849](https://github.com/FastLED/FastLED/pull/2849) — LPC11xx detection scaffold
- [#2859](https://github.com/FastLED/FastLED/pull/2859) — LPC15xx detection scaffold
- [#2866](https://github.com/FastLED/FastLED/pull/2866) — Scope LPC8xx driver code to LPC8xx-only
- [#2872](https://github.com/FastLED/FastLED/pull/2872) — Stage 4 wiring: LPC11Uxx + LPC15xx + APA102 SPI
- [#2876](https://github.com/FastLED/FastLED/pull/2876) — CodeRabbit follow-up fixes on #2872
- [fbuild#419](https://github.com/FastLED/fbuild/pull/419) / [fbuild#420](https://github.com/FastLED/fbuild/pull/420) — Stage 1: bare-metal target + real SystemInit

### Open follow-ups (carved out of #2845)

- [#2878](https://github.com/FastLED/FastLED/issues/2878) — Stage 4.1 legacy: LPC1110/1112/1114/1115 clockless (0x50000000 GPIO fastpin)
- [#2879](https://github.com/FastLED/FastLED/issues/2879) — Stage 4.4: LPC845 multi-strip parallel output (blocked on Stage 2c hardware)
- [#2880](https://github.com/FastLED/FastLED/issues/2880) — Stages 3.5 + 3.6: LPC845 hardware bring-up + AutoResearch UART
- [fbuild #456](https://github.com/FastLED/fbuild/issues/456) — Meta: Stage 3 fbuild-side items (3.1, 3.2, 3.3, 3.4, 3.7, 3.8)

### NXP user manuals

- UM11029 (LPC84x User Manual) — LPC845/LPC804 SCT, DMA, SYSCON, GPIO
- UM11065 (LPC80x User Manual) — LPC804 PLU
- UM10398 (LPC111x/LPC11Cxx User Manual) — LPC11xx legacy GPIO
- UM10462 (LPC11U2x/3x User Manual) — LPC11U-family legacy GPIO
- UM11074 (LPC15xx User Manual) — LPC15xx GPIO + SCT
