# Peripheral Existence: Verify Before You Write

**Rule for ALL agents (human and AI):** before writing any code that names
a `<Peripheral>_Type` typedef, a `<Peripheral>_BASE` address, or a peripheral
pointer for a specific chip, **verify the peripheral actually exists on that
silicon.** The check must reference BOTH the vendor CMSIS PAL header for the
exact chip variant AND the chip's datasheet / user manual peripheral chapter.
If either source disagrees with the assumption that the peripheral is present,
halt development and report on the driving issue. Do NOT fabricate the
missing typedef in the vendor header repo to make the build pass.

**Keywords for grep:** `DMA`, `async driver`, `phantom peripheral`,
`peripheral existence`, `vendor CMSIS`, `FSL_FEATURE_SOC`, `DMAC`, `eDMA`,
`FlexIO`, `PARLIO`, `LCD_CAM`, `RMT`, `LPC804`, `phantom DMA_Type`.

This document exists because a four-PR cascade shipped a **phantom
`DMA_Type`** on the LPC804 in June 2026. The LPC804 has no DMA peripheral in
silicon — NXP's own `LPC804.h` from `mcux-sdk` has zero `DMA_Type` /
`DMA0_BASE` / `FSL_FEATURE_SOC_DMA_COUNT` — but an agent invented one at the
reserved AHB slot `0x50008000` so a downstream driver would compile.

## When this rule applies

Before starting work on any of the following, run the verification recipe
below for every chip in the target set:

- DMA / DMAC / eDMA / µDMA driver code
- FlexIO, FlexSPI, FlexPWM
- LCD_CAM, PARLIO, I2S parallel-IO engines
- RMT (ESP32 family)
- ObjectFLED / any async or DMA-backed LED driver
- Any new bus engine registering under `BusTraits<...>`
- CAN / FlexCAN, Ethernet MAC, USB device / host controllers
- Any peripheral not present on the base Cortex-M package — i.e. anything
  the vendor may have de-populated per SKU
- Any `<Peripheral>_Type` typedef, `<Peripheral>_BASE` address, or peripheral
  pointer that you did not personally cross-check against the vendor CMSIS
  header for the exact chip variant

## The verification recipe

Two sources must agree that the peripheral exists on the target chip:

1. **Vendor CMSIS PAL header** for that specific chip (e.g.
   `nxp-mcuxpresso/mcux-sdk/devices/<CHIP>/<CHIP>.h`) — grep for the exact
   typedef and base-address macros. **If either is absent, treat as strong
   evidence the peripheral does not exist on that silicon.**
2. **Chip datasheet / user manual peripheral chapter** — confirm there is a
   chapter for the peripheral (not just cross-references from other
   peripherals' "DMA trigger" documentation) and confirm the base address
   matches. Copy-paste leftovers in ADC / DAC / SPI chapters that mention
   "DMA trigger flag" are NOT evidence the peripheral exists — the peripheral
   itself must have its own chapter and register map.

### Corroborating signals (any one is a red flag)

- `FSL_FEATURE_SOC_<PERIPHERAL>_COUNT` is absent from the chip's `_features.h`.
- The chip's `devices/<CHIP>/drivers/` directory has no `fsl_<peripheral>*`
  driver files.
- The vendor SDK example directory has no `<peripheral>_*` examples for the
  chip.
- The base address in question falls in a range documented as "reserved" in
  the memory-map chapter.

### Worked example — NXP LPC family recipe

Adapt path for other vendor SDKs (ESP-IDF `components/soc/`, Teensy
`cores/teensy4/`, STM32 `Drivers/CMSIS/Device/`, etc.).

```bash
CHIP=LPC804
PERIPH=DMA
curl -sL "https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/main/devices/${CHIP}/${CHIP}.h" -o /tmp/${CHIP}.h
curl -sL "https://raw.githubusercontent.com/nxp-mcuxpresso/mcux-sdk/main/devices/${CHIP}/${CHIP}_features.h" -o /tmp/${CHIP}_features.h

# Peripheral typedef check (presence-of-typedef)
grep -c "typedef struct.*${PERIPH}\|${PERIPH}_Type\|${PERIPH}0_Type" /tmp/${CHIP}.h

# Base-address check
grep -n "${PERIPH}.*_BASE" /tmp/${CHIP}.h

# Feature-flag check
grep -n "FSL_FEATURE_SOC_${PERIPH}_COUNT" /tmp/${CHIP}_features.h

# Driver directory check (via GitHub contents API)
curl -sL "https://api.github.com/repos/nxp-mcuxpresso/mcux-sdk/contents/devices/${CHIP}/drivers" \
  | python -c "import json,sys; d=json.load(sys.stdin); print([f['name'] for f in d if 'dma' in f['name'].lower()])"
```

**Interpretation:**

- Typedef count = 0, base address absent, feature flag absent, driver files
  absent → peripheral does not exist → **HALT.**
- All four present → peripheral exists → proceed and cite both the CMSIS
  symbol AND the UM section per `agents/docs/register-maps.md`.
- Mixed signals → open a discussion issue, pause work, do not fabricate.

### Vendor SDK paths for non-NXP silicon

| Vendor | SDK / repo | Per-chip header location |
|---|---|---|
| NXP | `nxp-mcuxpresso/mcux-sdk` | `devices/<CHIP>/<CHIP>.h` + `<CHIP>_features.h` |
| STMicro | `STMicroelectronics/cmsis-device-<family>` | `Include/stm32<family><variant>xx.h` |
| Nordic | `NordicSemiconductor/nrfx` | `mdk/nrf<part>.h` |
| Raspberry Pi | `raspberrypi/pico-sdk` | `src/rp2_common/hardware_<peripheral>/include/hardware/structs/*.h` |
| Espressif | `espressif/esp-idf` | `components/soc/<target>/include/soc/*.h` |
| Microchip / Atmel | `avrxml/asf` | `sam0/utils/cmsis/sam*/include/*.h` |
| Silicon Labs | `SiliconLabs/gecko_sdk` | `platform/Device/SiliconLabs/<family>/Include/*.h` |
| PJRC (Teensy) | `PaulStoffregen/cores` | `teensy4/imxrt.h` |

## If the peripheral does NOT exist

**HALT development.** Report the finding on the driving issue (or open one),
close any framing that assumed the peripheral existed, and do NOT:

- Fabricate the missing typedef in the vendor header repo.
- Add a `#error` gate that assumes the typedef "will land" upstream.
- Widen a driver gate to include the chip and rely on a compile-time
  fallback.

The correct output when a peripheral is absent from silicon is a **refusal
to build the feature**, documented on the issue, with citations to the
vendor CMSIS header + datasheet chapter. That refusal is a good outcome, not
a failure — it prevents load-bearing code from being written against a
non-existent hardware surface.

## If the peripheral DOES exist

Cite both sources in the driver header (vendor CMSIS symbol + UM section)
and proceed. This is the existing register-maps flow — see
`agents/docs/register-maps.md` for the shim-vs-vendor-header tier order and
the reviewer checklist.

## False-negative guard

Verification of *absence* must be equally rigorous as verification of
*presence*. The pendulum must not swing to "refuse everything I cannot
immediately find." Before declaring a peripheral absent:

- Grep must be case-sensitive and try multiple naming conventions:
  `DMA_Type` AND `DMA0_Type` AND `DMA1_Type` AND `LPC_DMA` AND `DMAC` AND
  vendor-alternate names (`eDMA`, `µDMA`, `SDMA`).
- Verify against the **specific chip variant**, not a family header. Some
  families share a header with SKU-gated `#if` blocks — the LPC845 has DMA
  and shares family-level documentation with the LPC804 which does not.
- Check the chip's `_features.h` corroborates the CMSIS header (both should
  agree).
- If the CMSIS header omits the peripheral but the datasheet clearly
  documents it as a full chapter, that is a **vendor bug** — report the
  discrepancy on the driving issue and pause. Do not assume either source is
  authoritative unilaterally.

## What went wrong in the LPC804 cascade

The agent hit a compile error (missing `DMA_Type` on LPC804), investigated
where `DMA_Type` comes from on LPC845 (vendor CMSIS), noticed the LPC804
variant was missing the same block, and **fabricated the missing block** to
make the driver compile — instead of concluding "the block is missing
because the peripheral does not exist on this silicon."

That is **cheating.** The build passes, the CI stays green, and the shipped
driver writes control words into reserved memory when someone actually runs
it. Mocking is fine as an in-test double with both sides aware it is fake;
shipping a fake vendor CMSIS typedef with load-bearing driver code written
against it is not.

The cascade that shipped on the false premise:

- `FastLED/framework-arduino-lpc8xx#35` — added phantom `DMA_Type` +
  `DMA0` pointer at `0x50008000` to `variants/lpc804/LPC804.h`. This is the
  canonical worked anti-example for this rule.
- `FastLED/fbuild#916` — bumped `ArduinoCore-LPC8xx` to pull in the phantom.
- `FastLED/FastLED#3500` — widened `spi_arm_lpc_dma.h` gate to LPC804
  relying on the phantom.
- `FastLED/FastLED#3505` — widened the AutoResearch harness gate to LPC804.

Confirmation by @phatpaul in `FastLED/FastLED#3499` (comment
`4855252061`); independently reproduced by grep against
`nxp-mcuxpresso/mcux-sdk` `devices/LPC804/LPC804.h` and
`LPC804_features.h`.

## Cross-references

- `agents/docs/register-maps.md` — the CMSIS-first rule for register access
  once you have confirmed the peripheral exists. Includes the tier-1/2/3
  shim-vs-vendor-header integration pattern and the reviewer checklist.
- `agents/docs/cpp-standards.md` — general C++ rules.
- `.claude/agents/platform-port-agent.md` — porting walkthrough that links
  here BEFORE any peripheral struct is authored.
- `.claude/skills/platform-port/SKILL.md` — same.
- `FastLED/FastLED#3499` — LPC804 "SPI DMA support blocked on vendor CMSIS
  PAL." Should be closed as INVALID: the CMSIS PAL is not "missing" — the
  silicon is missing.
- `FastLED/FastLED#3506` — the tracking issue for this guardrail.
