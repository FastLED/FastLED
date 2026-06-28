# Vendored NXP CMSIS PAL headers for LPC8xx

This directory contains NXP's CMSIS Peripheral Access Layer headers, vendored
verbatim from upstream so that FastLED's LPC drivers can use vendor-correct
register typedefs (`SCT_Type`, `DMA_Type`, `SYSCON_Type`, `SPI_Type`,
`GPIO_Type`, `SWM_Type`, `IOCON_Type`, etc.) instead of hand-rolled shim
structs.

This is the "tier 2 vendor in" path in `agents/docs/register-maps.md` and
the V1 step of the [#3437](https://github.com/FastLED/FastLED/issues/3437)
remediation plan. It exists because hand-typed offsets in
`clockless_arm_lpc_pwm_dma.h` shipped wrong twice ([#3349](https://github.com/FastLED/FastLED/pull/3349)).

## Files

| File | Purpose |
|---|---|
| `LPC845.h` | CMSIS PAL for LPC845 — all peripheral typedefs, register masks, and pointer macros |
| `system_LPC845.h` | System init function declarations (`SystemInit`, `SystemCoreClockUpdate`, `SystemCoreClock`) referenced by `LPC845.h` |

The headers in turn `#include "core_cm0plus.h"` which is part of CMSIS-Core
and is expected to be on the include path from the ARM toolchain
(`arm-none-eabi-gcc`'s CMSIS-Core package). FastLED does not vendor
CMSIS-Core itself.

## Provenance

- **Upstream repository:** `nxp-mcuxpresso/mcux-sdk`
- **Upstream path:** `devices/LPC845/`
- **Commit SHA:** `8a289764d763ad06e0c3a05c885644ed98b970af` (2025-10-31)
- **Vendor file revision:** rev. 1.2 (2017-06-08), build b201029 / b210815
- **Reference manual cited:** LPC84x User manual Rev. 1.6 (UM11029)
- **License:** BSD-3-Clause (per file header)
- **Copyright:** 1997-2016 Freescale Semiconductor, Inc.; 2016-2020 NXP

Files are byte-identical with upstream. License headers preserved
verbatim. Do not edit — re-fetch from upstream if a newer revision is
needed, and update this README's SHA + date.

To re-fetch:

```sh
SHA=8a289764d763ad06e0c3a05c885644ed98b970af
BASE="https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/${SHA}/devices/LPC845"
curl -sSL --fail "${BASE}/LPC845.h"        -o LPC845.h
curl -sSL --fail "${BASE}/system_LPC845.h" -o system_LPC845.h
```

## Verified findings

Cross-checking the vendor `SYSCON_Type` against the
`FL_LPC_SYSCON_Shim` in `clockless_arm_lpc_pwm_dma.h` confirmed:

- **`SYSAHBCLKCTRL0` is at offset `0x80`** — matching @phatpaul's
  [#3349](https://github.com/FastLED/FastLED/pull/3349) fix
  (`_resv0[32]`), **not** the pre-#3349 master value of `0x40`
  (`_resv0[16]`). Vendor truth: `LPC845.h` line where `SYSAHBCLKCTRL0`
  is declared with `/**< ..., offset: 0x80 */`.

Spot-checking the `DMA_Type.CHANNEL[]` layout against
`FL_LPC_DMA_ChannelShim` confirmed:

- `CFG` at array offset `0x0`, `CTLSTAT` at `0x4`, `XFERCFG` at `0x8`,
  reserved word at `0xC`, per-channel step `0x10`. Both layouts agree.
  However, the vendor common-register block lives at
  `DMA0->COMMON[0].INTENSET` etc., **not** `LPC_DMA->INTENSET0`. That
  member-name difference is what subsequent PRs (#3437 V6) need to
  reconcile when deleting `FL_LPC_DMA_Shim`.

Spot-checking the `SCT_Type` layout against `FL_LPC_SCT_Shim`
uncovered hidden bugs in the shim that #3349 did **not** catch:

- The shim splits `LIMIT` / `HALT` / `STOP` / `START` into pairs of
  `_L` / `_H` 32-bit registers (8 bytes each). The vendor has each as
  a single 32-bit register (4 bytes), with optional 16-bit-access
  unions (`LIMIT_ACCESS16BIT.LIMITL` + `.LIMITH`) that share the same
  4 bytes.
- Net effect: every shim member from `COUNT_U` (offset 0x040 per
  shim comment) onwards is laid out 16 bytes too high — `LPC_SCT->COUNT_U`
  actually addresses the SCT register at hardware offset `0x050`, not
  `0x040`. The driver does not (today) read `COUNT`, `STATE`, `INPUT`,
  or higher members, which is why this has not manifested as a visible
  bug.

These corroborate the central thesis of `agents/docs/register-maps.md`
that hand-typed offsets and hand-typed reserved-gap sizes are
fundamentally unsafe and that the vendor header must be the source of
truth.
