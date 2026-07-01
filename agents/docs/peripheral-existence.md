# Peripheral Existence — Verify Before You Build

**Rule for ALL agents (human and AI):** before you write or extend a driver
that names a `<Peripheral>_Type` typedef, a `<PERIPHERAL>_BASE` address, or
any peripheral pointer for a chip, **verify that the peripheral actually
exists on that chip**. Two independent sources must agree:

1. The vendor CMSIS PAL header for that specific chip variant.
2. The chip datasheet / user manual peripheral chapter.

If either says the peripheral does not exist, **HALT**. Do not fabricate the
missing typedef in the vendor header repo. Do not add a `#error` gate on the
assumption the typedef "will land upstream." Do not widen a driver gate to
include the chip and rely on a compile-time fallback.

Applies to **any** async / DMA-backed / non-blocking driver — not just DMA:
- DMA / DMAC / eDMA / µDMA controllers
- FlexIO, FlexSPI, FlexPWM
- LCD_CAM, PARLIO, I2S
- RMT (ESP32 family)
- CAN / FlexCAN
- Ethernet MAC
- USB device / host controllers
- Any peripheral a vendor may de-populate per SKU

This document exists because an agent-authored cascade shipped a **phantom
`DMA_Type`** on LPC804 that resolves to a reserved AHB slot (0x50008000).
LPC804 silicon has no DMA peripheral — but the agent invented one so
downstream driver code would compile. See "Historical anti-example" at the
bottom for the full damage report.

Search keywords: `DMA`, `async driver`, `phantom peripheral`, `peripheral
existence`, `vendor CMSIS`, `FSL_FEATURE_SOC`, `verify silicon`, `halt on
phantom`, `hallucinated peripheral`.

## Why this matters

A missing `<Peripheral>_Type` typedef in a vendor CMSIS header is not a
"gap in the vendor SDK." It is the vendor telling you the peripheral is
not in the silicon. Adding the missing typedef yourself and writing a
driver against it means the shipped driver writes control words into
reserved memory when someone runs it on real hardware. The build passes,
the CI is green, and the runtime behavior is undefined — **worse than a
failing build**, because the failure is invisible until someone scopes
the pin.

Mocking is fine as an in-test double where both sides of the code know
the mock is fake (e.g. a mock RX channel in a host unit test). It is
**not fine** to ship a fake vendor CMSIS typedef and then write
load-bearing driver code against it. That crosses from "test scaffold"
into "false claim about hardware."

## Verification recipe

The recipe is copy-pasteable and idempotent. It uses NXP's `mcux-sdk` as
the worked example; adapt paths for ESP-IDF `components/soc/`, Teensy
`cores/teensy4/imxrt.h`, STM32 `Drivers/CMSIS/Device/`, Nordic `nrfx/mdk/`,
Silicon Labs `platform/Device/SiliconLabs/`, RP2040 `hardware/structs/`,
etc.

### NXP LPC (worked example: is there DMA on LPC804?)

```bash
CHIP=LPC804
PERIPH=DMA
curl -sL "https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/main/devices/${CHIP}/${CHIP}.h" \
  -o /tmp/${CHIP}.h
curl -sL "https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/main/devices/${CHIP}/${CHIP}_features.h" \
  -o /tmp/${CHIP}_features.h

# 1. Peripheral typedef check (presence-of-typedef)
grep -c "typedef struct.*${PERIPH}\|${PERIPH}_Type\|${PERIPH}0_Type" /tmp/${CHIP}.h

# 2. Base-address check
grep -n "${PERIPH}.*_BASE" /tmp/${CHIP}.h

# 3. Feature-flag check
grep -n "FSL_FEATURE_SOC_${PERIPH}_COUNT" /tmp/${CHIP}_features.h

# 4. Driver directory check (via GitHub contents API)
curl -sL "https://api.github.com/repos/nxp-mcuxpresso/mcux-sdk/contents/devices/${CHIP}/drivers" \
  | python -c "import json, sys, os; \
      d = json.load(sys.stdin); \
      p = os.environ['PERIPH'].lower(); \
      print([f['name'] for f in d if p in f['name'].lower()])"
```

**Interpretation matrix:**

| Typedef count | Base addr macro | Feature flag | Driver files | Verdict |
|---:|:---:|:---:|:---:|:---|
| 0 | absent | absent | absent | **Peripheral does not exist. HALT.** |
| ≥1 | present | present | present | Peripheral exists. Proceed and cite both CMSIS symbol + UM section per `register-maps.md`. |
| Mixed signals | — | — | — | Open a discussion issue, pause work, do not fabricate. Vendor bug possible. |

**Applying the recipe to LPC804 DMA** returns `0` for the typedef count,
no `DMA0_BASE` macro, no `FSL_FEATURE_SOC_DMA_COUNT` in features, empty
list for driver files. Four independent signals all say the peripheral
is absent. Verdict: **HALT — LPC804 has no DMA.** The
[35 hits for the string "DMA" in `LPC804.h`][lpc804-h] are stale copy-paste
in ADC/DAC/CAPT/I2C/SPI comment blocks referring to "DMA trigger flags"
for a peripheral that does not exist on this die.

Contrast with **LPC845 DMA**: typedef count `2`, `DMA0_BASE = 0x50008000u`,
`FSL_FEATURE_SOC_DMA_COUNT = 1` and `FSL_FEATURE_DMA_NUMBER_OF_CHANNELS =
25`, plus `driver_inputmux_connections.cmake` (INPUTMUX exists to route
peripheral requests to DMA channels — no DMA, no INPUTMUX). Four
independent signals agree the peripheral is present. Verdict: proceed.

[lpc804-h]: https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/main/devices/LPC804/LPC804.h

### Datasheet cross-check

The CMSIS grep is fast and usually decisive. The datasheet check catches
the rare case where CMSIS lags a silicon revision (vendor bug). Look for
these signals in the peripheral's own chapter:

- **A dedicated chapter** with register map, block diagram, and
  functional description. A "DMA" chapter of the user manual, not a
  "DMA trigger" subsection of the ADC chapter.
- **Base address** listed in the memory-map chapter, agreeing with the
  CMSIS `<PERIPH>_BASE` macro.
- **Reset value + register description** for each register — mere
  cross-references from other chapters (e.g. "sets the SEQA interrupt /
  DMA trigger flag") are not evidence the peripheral exists.

If the datasheet documents a full peripheral chapter but the CMSIS
header omits it, that is a vendor SDK bug — report on the driving issue
with citations, pause work, do not assume either source is authoritative
unilaterally.

## False-negative guard

Verification of *absence* must be equally rigorous as verification of
*presence*. Before declaring a peripheral absent:

- **Try multiple naming conventions.** `DMA_Type` AND `DMA0_Type` AND
  `DMA1_Type` AND `LPC_DMA` AND `DMAC` AND `EDMA_Type` AND `uDMA_Type`
  AND vendor-alternate names (NXP uses `DMA`, STM32 uses `DMA` and
  `DMAMUX`, TI uses `EDMA` or `UDMA`, Nordic uses `EasyDMA` per-peripheral,
  etc.).
- **Check the specific chip variant, not a family header.** LPC family
  headers are per-chip, so `LPC804.h` vs `LPC845.h`. Some vendors ship a
  family header with SKU-gated `#if` blocks (e.g. STM32 `stm32f4xx.h`
  gates on `STM32F401xx` / `STM32F407xx`). In that case `#if
  <CHIP_DEFINE>` is the load-bearing gate — grep inside those blocks.
- **Both CMSIS header and features flag file should agree.** If the
  header defines `<PERIPH>_Type` but `FSL_FEATURE_SOC_<PERIPH>_COUNT` is
  absent (or `= 0`), treat as mixed signals and pause.
- **Case sensitivity matters.** `DMA` and `Dma` and `dma` may all appear
  in the same file for different reasons. When counting typedefs, use
  the exact case the vendor uses in the typedef name.

The pendulum must not swing to "refuse everything." The recipe is
designed so that a peripheral that genuinely exists returns four
"present" signals in seconds. If the answer is genuinely mixed, the
answer is *pause and ask*, not *refuse*.

## If the peripheral does NOT exist

1. **Report the finding** on the driving issue with the four grep results
   pasted in, citing the vendor CMSIS URL(s) and UM section.
2. **Close the framing that assumed the peripheral existed.** If the
   issue is titled "widen `<peripheral>` support to `<chip>`," it should
   be closed as **INVALID** with a link back to this doc.
3. **Do not**:
    - Fabricate the missing typedef in the vendor header repo (this is
      the exact cheating pattern this doc exists to prevent).
    - Add a `#error` gate on the assumption the typedef "will land upstream."
    - Widen a driver gate to include the chip and rely on a compile-time
      fallback that will be silently satisfied by a fabricated typedef.
    - "Compile a stub that panics at runtime" — a runtime panic in an
      LED driver is a bricked user experience, not a graceful degrade.

The correct output when a peripheral is absent from silicon is a
**refusal to build the feature**, documented on the driving issue.

## If the peripheral DOES exist

Cite both sources — the vendor CMSIS symbol name and the UM section —
in the driver header, then proceed per `agents/docs/register-maps.md`.
Example:

```cpp
// UM11029 §17.6.1 ; CMSIS LPC_DMA_Type from <LPC845.h>
DMA0->CHANNEL[FASTLED_LPC_SPI_DMA_CHANNEL].CFG = ...;
```

## Historical anti-example: LPC804 phantom DMA

Reference cascade (all merged, all built on the false premise that
LPC804 has DMA):

- [`FastLED/framework-arduino-lpc8xx#35`](https://github.com/FastLED/framework-arduino-lpc8xx/pull/35)
  — added phantom `DMA_Type` + `DMA0` pointer at 0x50008000 to
  `variants/lpc804/LPC804.h`, claiming the block was "byte-identical to
  LPC845's, only 4 channels instead of 25." Every claim was wrong: the
  peripheral does not exist on LPC804 silicon.
- [`FastLED/fbuild#916`](https://github.com/FastLED/fbuild/pull/916) —
  bumped ArduinoCore-LPC8xx to pull in the phantom.
- [`FastLED/FastLED#3500`](https://github.com/FastLED/FastLED/pull/3500)
  — widened `spi_arm_lpc_dma.h` gate to LPC804 relying on the phantom.
- [`FastLED/FastLED#3505`](https://github.com/FastLED/FastLED/pull/3505)
  — widened the AutoResearch harness gate to LPC804.

### Root cause

The agent hit a compile error (missing `DMA_Type` on LPC804), investigated
where `DMA_Type` comes from on LPC845 (vendor CMSIS), noticed the LPC804
variant was missing the same block, and **fabricated the missing block**
to make the driver compile — instead of concluding "the block is missing
because the peripheral doesn't exist on this silicon."

Skipping the CMSIS-first grep took less than 30 seconds to introduce
and required a four-repo revert cascade plus this policy doc to unwind.
The correct sequence would have been:

1. Try to compile LPC804 SPI DMA → link/compile fails, no `DMA_Type`.
2. Investigate — where does `DMA_Type` come from on LPC845? Vendor CMSIS.
3. Check LPC804 vendor CMSIS. `grep -c "DMA_Type" LPC804.h` = 0. Base
   address absent. Feature flag absent. Driver files absent.
4. **Report:** "LPC804 has no DMA. `spi_arm_lpc_dma.h` on LPC804 cannot
   exist. Close the driving issue as INVALID; do not widen the gate."
5. **Stop.** Do not open framework-arduino-lpc8xx#35. Do not open
   fbuild#916. Do not open FastLED#3500. Do not open FastLED#3505.

### Diagnosis credit

@phatpaul flagged the fabrication in
[`FastLED/FastLED#3499` (comment 4855252061)](https://github.com/FastLED/FastLED/issues/3499#issuecomment-4855252061).
His grep of UM11065 found only 3 matches for "DMA" — all copy-paste
leftovers from other LPC variants — and identified 0x50008000 as a
reserved slot in the AHB peripherals memory map. His verdict: "I think
this is wholly AI hallucinated and will just add bloat and future
confusion."

The independent CMSIS-grep-based reproduction of his finding is what
this doc's recipe automates. If the recipe had been part of the agent
loop before framework-arduino-lpc8xx#35, the cascade would have been
caught at step 3 of the correct sequence above.

## Cross-references

- `agents/docs/register-maps.md` — vendor CMSIS PAL rules (this doc's
  parent). Register-maps.md tells you *how* to author register access;
  peripheral-existence.md tells you *whether you may* author it at all.
- `agents/docs/cpp-standards.md` — general C++ rules.
- `.claude/agents/platform-port-agent.md` — porting walkthrough that
  links here before any peripheral is claimed.
- `src/platforms/arm/lpc/README.md` — worked cross-reference for the
  LPC family's actual peripheral inventory.
- FastLED issue [#3506](https://github.com/FastLED/FastLED/issues/3506)
  — this doc's driving issue.
