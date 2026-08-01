# Teensy 4.x LPSPI — local fork

Vendored from the Teensyduino `SPI` library so FastLED does not depend on the
Arduino `SPI` library on Teensy 4.x.

## Upstream

| | |
|---|---|
| Source | `libraries/SPI/SPI.h`, `libraries/SPI/SPI.cpp` |
| Version | Teensyduino 1.60 (`framework-arduinoteensy` 1.160.0) |
| Author / licence | Paul Stoffregen, PJRC — MIT |

Only the IMXRT1062 (Teensy 4.x) paths are taken. Every copied body carries an
`// Upstream SPI.{h,cpp}:<lines>` comment next to it; keep those accurate if
this is ever re-synced.

Same precedent as `platforms/arm/teensy/audio/pjrc_*` and
`platforms/arm/teensy/sdfat/`: fork locally, namespace it, and document
provenance next to the code. Files are named `lpspi_*` rather than `SPI.*`
deliberately — the sdfat fork kept upstream filenames and as a result its
`SdSpiDriver.h` still resolves `#include "SPI.h"` to the *framework* header.

## Why fork at all

Two independent reasons, both from FastLED#3777:

1. **It was not linking.** fbuild will not let a local library's header select
   a framework library, so `<SPI.h>` never entered the link and every
   `SpiHw2MXRT1062` / `SpiHw4MXRT1062` symbol came up undefined
   (FastLED#3775). Every documented way to declare the dependency was tried
   and none worked.
2. **We do not want it even if it linked.** The framework defines three global
   `SPIClass` objects (`SPI`, `SPI1`, `SPI2`). Each carries ~600 bytes of
   `.bss`, and once anything names one the linker can never drop it. FastLED
   named `SPI` as a template argument in `spi_output_template.h`, which is
   reached from `chipsets.h` — i.e. from essentially every sketch. That
   template parameter is now a plain bus index, so nothing references a
   global.

## What is here

| file | contents |
|---|---|
| `lpspi_hardware.h` | `LpspiHardware` struct + the three per-bus pin/mux tables, copied verbatim |
| `lpspi_bus.h` | `LpspiBus` (begin/end/setSCK/setMOSI/setMISO/beginTransaction/endTransaction/transfer) and `LpspiSettings` |

Bus indices match the legacy Arduino names the drivers already used:
`0 -> LPSPI4` (was `SPI`), `1 -> LPSPI3` (was `SPI1`), `2 -> LPSPI1` (was `SPI2`).

## Deliberate deviations from upstream

- **DMA triple dropped** from the hardware struct (`tx_dma_channel`,
  `rx_dma_channel`, `dma_rxisr`). Nothing on the FastLED path reads it, and
  keeping the `_spi_dma_rxISR*` pointers would anchor upstream's DMA and
  `EventResponder` machinery into the link.
- **NVIC mask save/restore dropped** from `beginTransaction` /
  `endTransaction`. That path is gated on `interruptMasksUsed`, which upstream
  only sets from `usingInterrupt()`. FastLED never calls it, so the masks are
  always zero and the block is dead. `endTransaction` is therefore a no-op.
- **Header-only.** Everything is `inline`; there is deliberately no
  `.cpp.hpp` and no `_build.cpp.hpp` entry. An earlier revision put the
  implementation in the unity build, which emitted `LpspiBus::get()`
  unconditionally and dragged all three hardware tables into every firmware —
  Teensy 4.1 `Blink` grew from 58,688 to 106,816 bytes of `.text`. Keeping it
  header-only restores byte-identical output for sketches that do not use SPI.
  **Do not move this into the unity build.**
- **Types** are FastLED's `fl::` fixed-width aliases.
- The `transfer*` family beyond single-byte, `setCS`, `setBitOrder`,
  `setDataMode`, the deprecated setters and the whole async/DMA block are
  simply absent — FastLED drives the data path itself with direct register
  access.

## Do not "tidy" the tables

The pin numbers, mux values and select-input registers are load-bearing and
board-specific (T4.0 and T4.1 differ in both arity and pin numbers). A wrong
mux value links cleanly and silently drives nothing.

## Validation status

`bash compile teensy41 --examples Apa102` links (it did not before), and
`--examples Blink` is byte-identical to the pre-change build, confirming the
backend is still fully strippable.

**Not yet validated on hardware.** Driving an APA102/SK9822 strip from a
Teensy 4.x is still outstanding — see FastLED#3777. Compiling proves the
symbols resolve, not that the pads are muxed correctly.
