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
| **LPC11xx** | LPC1114, LPC1115, LPC11U24, LPC11U35 | Cortex-M0 | — | None yet | ⚠️ Detection scaffold only (#2849) |
| **LPC15xx** | LPC1517…LPC1549 | Cortex-M3 | — | None yet | ⚠️ Detection scaffold only (#2859) |

## Files (quick pass)

- `fastled_arm_lpc.h` — Aggregator; selects between bit-bang / PLU / PWM+DMA per chip and opt-in macro.
- `fastpin_arm_lpc.h` — Pin helpers. `_ARMPIN` template is **scoped to LPC845 / LPC804** ([#2866](https://github.com/FastLED/FastLED/pull/2866)) because LPC11xx and LPC15xx ship different GPIO controller layouts.
- `clockless_arm_lpc.h` — Bit-banged WS2812-family driver built on `arm/common/m0clockless.h` (C++ implementation, since LPC8xx GPIO SET/CLR offsets exceed the M0+ STR-immediate encoding range).
- `clockless_arm_lpc_plu.h` — LPC804-only Programmable Logic Unit (PLU) clockless driver. Hardware pulse-shaping via 26-LUT reconfigurable fabric; CPU only writes serial data per bit. See UM11065 §12.
- `clockless_arm_lpc_pwm_dma.h` — LPC845-only SCT + DMA-to-GPIO clockless driver. CPU-free WS2812 output via three DMA channels (T0_RISE / T_MID / T_END). See UM11029 §16-17.
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
