# FastLED Platform: NXP LPC

NXP LPC microcontroller family support. Initial port landed in #2837 ([meta #2836](https://github.com/FastLED/FastLED/issues/2836)); roadmap for completion and family expansion lives in [#2845](https://github.com/FastLED/FastLED/issues/2845).

## Platform Overview

The LPC family currently covers two ARM cores:

- **Cortex-M0+** (LPC845, LPC804) — bare-metal target via [fbuild](https://github.com/FastLED/fbuild) ≥ v2.2.18. Bit-banged clockless driver shipped; optional hardware-assisted drivers available per chip.
- **Cortex-M0** (LPC11xx) and **Cortex-M3** (LPC15xx) — family **detection** is wired (`FL_LPC11`, `FL_LPC15`), but full driver wiring (`led_sysdefs`, `fastpin`, clockless) is pending hardware bring-up. See [#2845](https://github.com/FastLED/FastLED/issues/2845) Stage 4.

### Supported Platforms

| Platform | Chip | CPU | Default Clock | Drivers Shipped | Status |
|----------|------|-----|---------------|-----------------|--------|
| **LPC845** | LPC845M301 | Cortex-M0+ @ 30 MHz | 30 MHz | Bit-bang (default) + optional SCT/PWM+DMA (#2850) | ✅ Compiles; hardware bring-up pending |
| **LPC804** | LPC804M101 | Cortex-M0+ @ 15 MHz | 15 MHz | Bit-bang (default) + optional PLU (#2848) | ✅ Compiles; hardware bring-up pending |
| **LPC11Uxx** | LPC11U24, LPC11U35 | Cortex-M0 | 12 MHz (IRC) | Shared LPC8xx fastpin + M0 C++ clockless (#2872) | ✅ Compiles; hardware bring-up pending |
| **LPC11xx legacy** | LPC1110, LPC1112, LPC1114, LPC1115 | Cortex-M0 | — | None — legacy GPIO at 0x50000000 needs its own fastpin | ⚠️ `#error` if targeted; driver wiring TBD |
| **LPC15xx** | LPC1517…LPC1549 | Cortex-M3 | 12 MHz (IRC) | Shared LPC8xx fastpin + M3-compatible C++ clockless (#2872) | ✅ Compiles; hardware bring-up pending |

## Files (quick pass)

- `fastled_arm_lpc.h` — Aggregator; selects between bit-bang / PLU / PWM+DMA per chip and opt-in macro.
- `fastpin_arm_lpc.h` — Pin helpers. `_ARMPIN` template is **scoped to LPC845 / LPC804** ([#2866](https://github.com/FastLED/FastLED/pull/2866)) because LPC11xx and LPC15xx ship different GPIO controller layouts.
- `clockless_arm_lpc.h` — Bit-banged WS2812-family driver built on `arm/common/m0clockless.h` (C++ implementation, since LPC8xx GPIO SET/CLR offsets exceed the M0+ STR-immediate encoding range).
- `clockless_arm_lpc_plu.h` — LPC804-only Programmable Logic Unit (PLU) clockless driver. Hardware pulse-shaping via 26-LUT reconfigurable fabric; CPU only writes serial data per bit. See UM11065 §12.
- `clockless_arm_lpc_pwm_dma.h` — LPC845-only SCT + DMA-to-GPIO clockless driver. CPU-free WS2812 output via three DMA channels (T0_RISE / T_MID / T_END). See UM11029 §16-17.
- `spi_arm_lpc.h` — LPC845 / LPC804 hardware SPI driver for **APA102 / SK9822 / WS2801** clocked strips. Targets the LPC8xx SPI peripheral (UM11029 §"SPI") in master / MSB-first / mode-0 / 8-bit configuration. SPI0 default; users route to SPI1 via the `pSPIX` template arg. Pin routing through the LPC Switch Matrix is the user's responsibility (matching the bit-bang clockless driver's "FastPin sees raw GPIO" convention). Closes #2845 Stage 4 item 3.
- `led_sysdefs_arm_lpc.h` — System defines (sets `FL_IS_ARM_M0_PLUS`, `F_CPU`, forces `FASTLED_M0_USE_C_IMPLEMENTATION`, includes `<LPC845.h>` / `<LPC804.h>` CMSIS device headers).
- `fastpin_arm_lpc.h` — see above.
- `is_lpc.h` — Detection macros (`FL_LPC845`, `FL_LPC804`, `FL_LPC11`, `FL_LPC15`, `FL_IS_ARM_LPC`).
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

For LPC11xx and LPC15xx, no fbuild board entry exists yet; community contributors targeting those families with their own PlatformIO platform should expect to wire `led_sysdefs`, `fastpin`, and clockless headers per family before FastLED can compile against them. The hold is documented in `fastled_arm_lpc.h` — attempting to build with only `FL_LPC11` or `FL_LPC15` set emits a clear `#error` pointing here.

## Detection scaffolds

- **LPC11xx (Cortex-M0)** — `is_lpc.h` recognises `__LPC11xx__`, `CPU_LPC1114*`, `CPU_LPC1115*`, `CPU_LPC11U24*`, `CPU_LPC11U35*` and similar. LPC11xx is **Cortex-M0**, not M0+ — do not gate any code path on `FL_IS_ARM_M0_PLUS` when adding the driver. Driver wiring will reuse `arm/common/m0clockless_asm.h` (the same path nRF51 uses); the GPIO controller is the legacy "masked-access" peripheral at `0x50000000` (UM10398 §9 / UM10462 §9), so a new `fastpin_arm_lpc11.h` is required — the LPC8xx 0xA0000000 SET/CLR template does not apply.
- **LPC15xx (Cortex-M3)** — `is_lpc.h` recognises `__LPC15xx__` and `CPU_LPC15{17,18,19,47,48,49}*`. LPC15xx is **Cortex-M3** with the full Thumb-2 instruction set; do not gate on `FL_IS_ARM_M0` or `FL_IS_ARM_M0_PLUS`. Driver wiring should target the M3 clockless template; the GPIO controller layout per UM11074 differs again from both LPC8xx and LPC11xx.

## Implementation map — which repo owns which #2845 item

The LPC roadmap meta [#2845](https://github.com/FastLED/FastLED/issues/2845) spans two repositories. This is the table to consult before opening a PR against either side:

| Stage / Item | Owner repo | Status | Notes |
|---|---|---|---|
| **Stage 3.1** — Real `SystemInit` for LPC845 (FRO 30 MHz + PLL + flash wait states) | **`FastLED/fbuild`** | Not started | Lives in `fbuild/crates/fbuild-build/src/nxplpc/assets/startup_lpc845.S`. Needs UM11029 §4. **Hardware verification required.** |
| **Stage 3.2** — Real `SystemInit` for LPC804 | **`FastLED/fbuild`** | Not started | Same crate; UM11065 §4. **Hardware verification required.** |
| **Stage 3.3** — Vector table expansion (DMA-end IRQ, SCT match IRQ, USART RX) | **`FastLED/fbuild`** | Partial | Vector table in `fbuild` startup assets. Add IRQs as drivers reach a state where they need them. |
| **Stage 3.4** — Per-chip `mcu_config` split in `get_nxplpc_config` | **`FastLED/fbuild`** | Not started | Crate: `fbuild-build`. |
| **Stage 3.5** — Linker memory-layout verification on hardware | Hardware | Not started | Cannot be done without a real LPC845 board + flash + run. |
| **Stage 3.6** — `examples/AutoResearch/AutoResearch.ino` UART integration | **FastLED** | Not started | Wire LPC845 USART0 into the existing RPC harness. Code-only is possible, but ship gated on Stage 3.1 (so cycle counts derive from a real 30 MHz `F_CPU`) and Stage 3.5 (so the link succeeds on real silicon). |
| **Stage 3.7** — fbuild CI `continue-on-error` flip | **`FastLED/fbuild`** | Blocked on 3.1 + 3.5 | Workflows at `.github/workflows/build-lpc{804,845}.yml` in fbuild. |
| **Stage 3.8** — `validate_boards.py` reconciliation | **`FastLED/fbuild`** | Filed: [fbuild#421/#422](https://github.com/FastLED/fbuild/issues/421) | Script lives in fbuild; the FastLED `ci/` tree does not have a `validate_boards.py`. |
| **Stage 4.1** — LPC11Uxx clockless driver | **FastLED** | ✅ Shipped in #2872 | Reuses LPC8xx fastpin + M0 C++ clockless. |
| **Stage 4.1 (legacy)** — LPC1110/1112/1114/1115 clockless | **FastLED** | Not started | Needs new `fastpin_arm_lpc11_legacy.h` for the 0x50000000 GPIO. `#error` makes the unsupported case loud. |
| **Stage 4.2** — LPC15xx clockless driver | **FastLED** | ✅ Shipped in #2872 | Reuses LPC8xx fastpin + M3-compatible C++ clockless. |
| **Stage 4.3** — APA102 / SK9822 / WS2801 hardware SPI | **FastLED** | ✅ Shipped in #2872 | New `spi_arm_lpc.h` per UM11029. |
| **Stage 4.4** — Multi-strip parallel output | **FastLED** | Blocked on Stage 2c hardware validation | The meta gates this on PWM+DMA driver hardware validation. |
| **Stage 4.5** — PlatformIO upstream donation | **PlatformIO** (3rd party) | Not started | Board JSON + linker scripts donation to `platformio/platform-nxplpc` (does not exist yet — would be a new platform). |
| **Stage 4.6** — `fl::set_*` settings for LPC clock-speed / DMA-channel overrides | **FastLED** | No concrete user requirement | The existing build-time `F_CPU` override (define before `#include <FastLED.h>`) already covers the clock-speed case for users on stable boot clocks. Runtime override on `CFastLED` would need machinery to recompute per-chipset cycle counts when clock changes — out of scope until a user reports needing it. |

**Of the 15 items, 6 live in `FastLED/fbuild`, 1 lives in `platformio/*`, 1 is pure hardware bring-up (3.5), and 7 live here.** Of those 7 FastLED items: 3 shipped in #2872 (4.1 LPC11Uxx, 4.2 LPC15xx, 4.3 SPI), 2 are explicitly hardware-gated (3.6 AutoResearch UART, 4.4 multi-strip), 1 is the legacy-LPC11 fastpin follow-on, and 1 (4.6) has no concrete user requirement.

The meta should be **split into per-repo issues** once #2872 lands, since the single-issue tracking matrix obscures the cross-repo distribution of the work.

## References

- [#2836](https://github.com/FastLED/FastLED/issues/2836) — Meta: original LPC8xx port plan (closed)
- [#2837](https://github.com/FastLED/FastLED/pull/2837) — Stage 2a: bit-banged M0+ clockless driver
- [#2841](https://github.com/FastLED/FastLED/issues/2841) / [#2848](https://github.com/FastLED/FastLED/pull/2848) — Stage 2b: LPC804 PLU clockless driver
- [#2842](https://github.com/FastLED/FastLED/issues/2842) / [#2850](https://github.com/FastLED/FastLED/pull/2850) — Stage 2c: LPC845 SCT/PWM+DMA clockless driver
- [#2845](https://github.com/FastLED/FastLED/issues/2845) — Meta: LPC dev roadmap, Stages 3 + 4
- [#2849](https://github.com/FastLED/FastLED/pull/2849) — LPC11xx detection scaffold
- [#2859](https://github.com/FastLED/FastLED/pull/2859) — LPC15xx detection scaffold
- [#2866](https://github.com/FastLED/FastLED/pull/2866) — Scope LPC8xx driver code to LPC8xx-only
- [fbuild#419](https://github.com/FastLED/fbuild/pull/419) / [fbuild#420](https://github.com/FastLED/fbuild/pull/420) — Stage 1: bare-metal target + real SystemInit
- NXP UM11029 (LPC84x User Manual) — LPC845/LPC804 SCT, DMA, SYSCON, GPIO
- NXP UM11065 (LPC80x User Manual) — LPC804 PLU
- NXP UM10398 (LPC111x/LPC11Cxx User Manual) — LPC11xx legacy GPIO
- NXP UM10462 (LPC11U2x/3x User Manual) — LPC11U-family legacy GPIO
- NXP UM11074 (LPC15xx User Manual) — LPC15xx GPIO + SCT
