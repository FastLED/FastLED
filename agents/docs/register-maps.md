# Register Maps & Vendor CMSIS Headers

**Rule for ALL agents (human and AI):** when a MCU has an official vendor
peripheral-access-layer (CMSIS PAL) header — `LPC845.h`, `stm32f4xx.h`,
`MK20DX256.h`, `nrf52840.h`, `samd21.h`, `hardware/structs/*.h`, etc. — that
header is the **source of truth** for peripheral register layouts. Do not
hand-roll a parallel `struct FooShim { volatile u32 _resv0[16]; ... }` from
the chip's user manual when the vendor already publishes the typedef.

This document exists because two register-offset bugs slipped into
`src/platforms/arm/lpc/clockless_arm_lpc_pwm_dma.h` (issue #2990, fix PR
#3349). Both were hand-rolled shim entries whose offsets disagreed with NXP's
`LPC845.h`. The contributor who hit them put it bluntly: *"This seems like a
waste of time debugging AI hallucinated register maps, when it would be so
much easier and safer to use the vendor provided CMSIS Peripheral Access
Layer."*

## Why hand-rolled shims fail

A vendor CMSIS header is generated from the silicon designer's IP-XACT /
SVD description of the part. It encodes:

- Exact byte offsets including silicon-revision-specific reserved gaps.
- The correct width and packing of bit-fields.
- Errata-driven layout quirks that don't appear in the human-readable user
  manual.

Re-typing those struct members by hand from the user manual is *literally
the kind of work that LLMs and humans both get wrong*. Off-by-one on a
`_resv0[16]` vs `_resv0[32]` is invisible at compile time, silently
writes to a different register at runtime, and the bug doesn't show up
until someone scopes the GPIO pin and notices the LED isn't blinking.

The same point applies to every vendor: NXP, ST, Nordic, Atmel/Microchip,
Raspberry Pi, Silicon Labs, Renesas, GigaDevice. If they publish a CMSIS PAL
header, **use it**.

## Where to find the vendor header

These are the canonical upstream sources for the platforms FastLED currently
supports. All are BSD-3-Clause / Apache-2.0 / equivalent permissive — safe to
vendor in `src/platforms/<arch>/<vendor>/cmsis/` if upstream availability
through the toolchain proves unreliable.

| Vendor | Family | Upstream | License |
|---|---|---|---|
| NXP | LPC8xx (LPC845, LPC804) | https://github.com/nxp-mcuxpresso/mcux-devices-lpc/tree/main/LPC800 | BSD-3-Clause |
| NXP | LPC11xx, LPC15xx | https://github.com/nxp-mcuxpresso/mcux-devices-lpc | BSD-3-Clause |
| NXP | i.MX RT (Teensy 4.x base) | https://github.com/nxp-mcuxpresso/mcux-devices-imxrt | BSD-3-Clause |
| STMicro | STM32F/G/H/L/U | https://github.com/STMicroelectronics/cmsis-device-* (one repo per family) | Apache-2.0 |
| Nordic | nRF51 / nRF52 / nRF53 | https://github.com/NordicSemiconductor/nrfx (headers in `mdk/`) | BSD-3-Clause |
| Microchip (Atmel) | SAMD / SAM | https://github.com/avrxml/asf (CMSIS in `sam0/utils/cmsis/`) | BSD-3-Clause |
| Raspberry Pi | RP2040 / RP2350 | https://github.com/raspberrypi/pico-sdk (`src/rp2_common/hardware_*/include/hardware/structs/`) | BSD-3-Clause |
| Silicon Labs | EFM32 / EFR32 / MGM240 | https://github.com/SiliconLabs/gecko_sdk (`platform/Device/SiliconLabs/`) | Zlib |
| PJRC (Teensy) | MK20DX, MK66, MIMXRT1062 | https://github.com/PaulStoffregen/cores | MIT |

If a part is missing from this table, the rule of thumb is "search GitHub for
`<chip part number>.h`" — the official repo is almost always the first hit.
If the only available header is in a board-vendor SDK (e.g. an Arduino core)
that's also fine; the goal is *use the upstream, do not retype it*.

## How to integrate a vendor header

The order below is best to worst. Pick the highest tier that works for the
build environment you actually need to support.

### Tier 1 — Toolchain provides it (preferred)

The board's Arduino core or PlatformIO platform places the CMSIS header on
the include path automatically. Just `#include <LPC845.h>` (or the
appropriate header) from `led_sysdefs_<arch>_<vendor>.h` and use the
vendor's typedef'd peripheral pointers (`LPC_SCT->CONFIG`, `GPIOA->BSRR`,
`NRF_SPIM0->TXD.PTR`).

**Exemplars in this tree:**

- `src/platforms/arm/rp/rpcommon/clockless_rp_pio.h` — uses `hardware/pio.h`,
  `hardware/dma.h`, `hardware/structs/sio.h` from the Pico SDK directly.
- `src/platforms/arm/nrf52/spi_hw_2_nrf52.h` — uses `<nrf_spim.h>` and
  `<nrfx_timer.h>` directly.
- `src/platforms/arm/sam/fastspi_arm_sam.h` — uses Atmel CMSIS
  `(Spi*)SPI0`, `SPI_SR`, `SPI_TDR`.

### Tier 2 — Vendor the header into the tree

If the toolchain's include path is unreliable (e.g. the Arduino core only
exposes the active variant directory, like the NXP `ArduinoCore-LPC8xx`
PlatformIO binding does), copy the vendor header into
`src/platforms/<arch>/<vendor>/cmsis/<chip>.h`. License headers stay intact;
add a one-line `README.md` next to it citing the upstream URL and the
commit SHA the copy is from. Update the platform's `led_sysdefs_*.h` to
include the local copy.

### Tier 3 — Local shim, **last resort only**

A local `struct ShimName { volatile u32 ...; }` may be defined only when:

1. **Both** tiers 1 and 2 above were investigated and documented as
   infeasible for the targeted build configurations. Cite the reason in a
   comment on the shim.
2. The shim is gated behind the vendor header's own include guard so the
   real CMSIS definition wins automatically when present. The LPC8xx shims
   demonstrate this — the include guard is the typedef name from the
   vendor header (e.g. `LPC_SCT_TYPE_`):

   ```cpp
   #if !defined(LPC_SCT) && !defined(LPC_SCT_TYPE_)
   struct FL_LPC_SCT_Shim { /* ... */ };
   #define LPC_SCT ((FL_LPC_SCT_Shim*)LPC_SCT_BASE)
   #endif
   ```

3. **Every** struct member cites both the user-manual section *and* the
   corresponding member name in the vendor header. Example:

   ```cpp
   volatile u32 SYSAHBCLKCTRL0;  // UM11029 §4.6.13 ; CMSIS LPC_SYSCON_Type.SYSAHBCLKCTRL0 @ 0x080
   ```

4. The shim is reviewed against the actual vendor header before merge.
   "I read the user manual" is not sufficient. The reviewer checklist
   below is the gate.

## When in doubt, don't write the shim

If you cannot complete the four bullets above for a given peripheral,
**stop and ask** before merging. Filing an issue against the FastLED
repo with the chip part number, the missing peripheral, and the upstream
header URL is always preferable to landing speculative offsets.

## Reviewer checklist (block merge if any answer is no)

When reviewing a PR that adds or modifies peripheral register access:

- [ ] If a vendor CMSIS header exists for this chip, is the PR using it
      (tier 1 or 2)? If not, does the PR document *why* the vendor header
      cannot be used in the target build configuration?
- [ ] If a local shim is used (tier 3), is it gated behind the vendor's
      include guard (`#if !defined(VENDOR_TYPEDEF_NAME)`) so the real
      definition takes precedence when available?
- [ ] Does **every** shim member cite both the user-manual section and
      the corresponding vendor-header member name? Pulled up the vendor
      header in a second window and spot-checked at least three offsets?
- [ ] Is the shim file listed in the platform's `README.md` under
      "files (quick pass)" with a note that it is a tier-3 fallback?
- [ ] If the change touches an existing shim, does the diff include the
      vendor-header offset for any added/changed member?

## Resolved anti-example: LPC845 (FastLED #3437)

`src/platforms/arm/lpc/clockless_arm_lpc_pwm_dma.h` previously defined
three shim structs (`FL_LPC_SCT_Shim`, `FL_LPC_DMA_Shim`,
`FL_LPC_SYSCON_Shim`) written from UM11029 without cross-checking the
vendor header. Three offset bugs slipped in:

- **Miss 1** — `FL_LPC_SYSCON_Shim` had `_resv0[16]` (placing
  `SYSAHBCLKCTRL0` at offset `0x040`). The real LPC845 layout puts it at
  `0x080`. Caught and fixed by @phatpaul in #3349.
- **Miss 2** — `DMA_CHANNEL.CFG` was written with only the
  `HWTRIGEN` bit; the channel actually needs `PERIPHREQEN | HWTRIGEN`
  plus explicit `TRIGPOL=0`, `TRIGTYPE=0`. Also fixed in #3349.
- **Miss 3** — `FL_LPC_SCT_Shim` split `LIMIT`/`HALT`/`STOP`/`START`
  into `_L`/`_H` 32-bit register pairs (8 bytes each). The vendor SCT
  has them as single 32-bit registers (4 bytes, with optional inner
  16-bit access unions). Net effect: every shim member from `COUNT_U`
  onward was laid out 16 bytes too high. Uncovered during #3437 cross-
  verification; latent because the PWM+DMA driver did not dereference
  those members.

**Resolution (FastLED #3437, landed in PR #3438):** the three repos in
the LPC build chain now flow vendor PAL through to FastLED:

1. `zackees/ArduinoCore-LPC8xx#34` ships the full NXP CMSIS PAL in
   `variants/lpc845/LPC845.h` and `variants/lpc804/LPC804.h` (rev. 1.2,
   BSD-3-Clause, ~10K/7K lines respectively).
2. `zackees/platform-nxplpc-arduino#9` (v0.2.0) pins
   `framework-arduino-lpc8xx` to that SHA in `platform.json`.
3. FastLED `ci/boards.py` + `platformio.ini` bumped to the v0.2.0
   platform SHA; `led_sysdefs_arm_lpc.h` `#include <LPC845.h>` /
   `<LPC804.h>` directly; the SCT / DMA / SYSCON / SPI / PLU shims
   were deleted and every call site migrated to vendor typedefs
   (`SCT0->`, `DMA0->`, `SYSCON->`, `SPI0/1`, `PLU->`).

The tier-1 path (toolchain provides the canonical NXP CMSIS PAL) is
now operational across all five FastLED LPC8xx CI workflows.

## Cross-references

- `agents/docs/cpp-standards.md` — general C++ rules.
- `.claude/agents/platform-port-agent.md` — porting walkthrough that links
  here before any peripheral struct is authored.
- `.claude/skills/platform-port/SKILL.md` — same.
- `src/platforms/arm/lpc/README.md` — worked anti-example reference.
