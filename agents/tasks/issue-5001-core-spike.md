# Spike: worker configuration only in the Channels API

Removing core template arguments provides no useful memory savings in the
measured ordinary legacy sketch. Migrating a legacy caller that needs core
pinning to configured Channels increases its footprint in this experiment.
This is an experimental branch; the breaking API change is not merged.

## Matched measurements

Six WS2812B strips, 170 LEDs each, GPIO 1–6, ESP32-S3 LCD clockless
(`Bus::FLEX_IO`). All values are bytes.

| Build | Allocated image flash | Firmware binary | Static DRAM data+BSS |
|---|---:|---:|---:|
| Before: legacy, default worker | 321,915 | 322,032 | 26,076 |
| After: legacy, default worker | 321,915 | 322,032 | 26,076 |
| Before: legacy, template Core1 | 322,107 | 322,224 | 26,076 |
| Before: configured Channel, Core1/priority 23 | 341,631 | 341,744 | 26,844 |
| After: configured Channel, Core1/priority 23 | 341,627 | 341,744 | 26,844 |

- Default legacy removal: **0 flash / 0 static DRAM saved**.
- Unchanged configured-channel control: 4 fewer allocated image bytes,
  **0 firmware-binary bytes / 0 static DRAM saved**. This tiny difference
  does not support a practical footprint benefit.
- Migrating the pinned legacy sketch to the after-channel sketch:
  **+19,520 allocated flash bytes, +19,520 firmware-binary bytes,
  +768 static DRAM bytes**, before accounting for runtime heap usage.
- Before removal, requesting a legacy core costs only 192 flash bytes and
  no additional static DRAM in this sketch.

## What RAM means here

The target ELF size probes report `sizeof(WS2812B<1, GRB>) == 52` and
`sizeof(fl::Channel) == 340` before and after. Core selection added no
per-controller field. Full Channels allocate their objects dynamically;
static DRAM does not include those allocations, shared-pointer bookkeeping,
frame allocations, or FreeRTOS task stacks. No free-heap measurement or
on-device run is claimed.

The DRAM column sums `.dram0.data`, `.dram0.bss`, and `.noinit` directly
from each ELF. The 45,056-byte `.dram0.dummy` reservation is unchanged in all
five builds. It represents the shared IRAM/DRAM physical SRAM overlap, not
fictitious disposable memory; it is excluded only from the **DRAM data+BSS**
column. These figures are not total internal SRAM consumption or free heap.
The raw fbuild RAM headline includes that reservation. Symbol-attributed RAM
is another metric and is not used as physical RAM here.

Allocated image flash sums allocated, file-backed ELF sections, independently
of symbol aliases. Firmware binary size is also shown because padding absorbs
the four-byte difference in the channel control.

## Scope and provenance

Baseline: `f9beb26cee26d48a6d7213c1c1464f157a133754`.
Candidate: the production source delta in this spike branch.

- Board: `esp32s3` / ESP32-S3-DevKitC-1-N8, no PSRAM.
- fbuild: published 2.5.51, source `9b26affea5ad`.
- Platform: 55.03.311; Arduino 3.3.11; ESP-IDF 5.5.5.
- Compiler: Xtensa ESP GCC 14.2.0, 20260121 toolchain archive.
- Defines: `FL_LOG_LEVEL=0`, `CORE_SPIKE_MODE=0`, `1`, or `2`.
- Mode 0 is default legacy; mode 1 is baseline-only legacy Core1; mode 2
  is configured Channel Core1/priority 23, equal to the legacy LCD priority.
- Both APIs explicitly select `Bus::FLEX_IO`; the channel measurement uses
  `FastLED.add<Bus::FLEX_IO>(cfg)`, avoiding the all-driver runtime bundle.
- Identical sketch source and pixel arrays across before/after controls.
  Two four-byte volatile size probes appear in every build.
- SDK/build caches are preserved. No driver code, worker configuration,
  diagnostics, or scheduling behavior is removed from the shared engine.

Each case was compiled through `bash compile esp32s3 --examples AutoResearch
--defines CORE_SPIKE_MODE=N,FL_LOG_LEVEL=0`, then analyzed through
`bash bloat esp32s3 --example AutoResearch --no-summary`. The existing
AutoResearch sketch was temporarily used as the scratch target and restored.
No device was flashed. Raw ELFs, binaries, compiler logs, build metadata,
symbol reports and `metrics.json` remain in ignored
`.build/evidence/issue-5001-core-spike/`.

## Validation and publication

- [x] Capture all three baseline cases and both after-controls.
- [x] Remove core templates; preserve bus selection and custom chipset hooks.
- [x] Verify negative API regression RED then GREEN in debug mode.
- [x] Full native debug suite: 329 unit groups and 95 examples passed
  (292.50 seconds including setup).
- [x] Changed Python suite: 217 tests passed (22.94 seconds).
- [x] Restored canonical AutoResearch ESP32-S3 integration build passed.
- [x] Full lint and independent implementation/report review passed.

Publication scope is a draft spike PR for review, with native and ESP32-S3
hosted coverage selected at creation. Production merge is a separate decision.

The recommendation is to treat this as API simplification, not a memory
optimization. Requiring the full Channel path for affinity is a substantial
cost for this specific caller and should be an explicit product decision.

## Reproduction sketch

Use this in the existing AutoResearch scratch target, preserving its original
contents for restoration. Mode 1 intentionally compiles only on the baseline.

```cpp
#include <FastLED.h>
#include "fl/channels/channel.h"
#include "fl/channels/config.h"

#ifndef CORE_SPIKE_MODE
#define CORE_SPIKE_MODE 0
#endif

CRGB spikeLeds[6][170];
volatile fl::u32 spikeLegacyControllerSize = sizeof(WS2812B<1, GRB>);
volatile fl::u32 spikeChannelSize = sizeof(fl::Channel);

template<fl::u8 PIN, int LANE>
void addSpikeLegacy() {
#if CORE_SPIKE_MODE == 1
    FastLED.addLeds<WS2812B, PIN, GRB, fl::Bus::FLEX_IO,
                   fl::Core::Core1>(spikeLeds[LANE], 170);
#else
    FastLED.addLeds<WS2812B, PIN, GRB,
                   fl::Bus::FLEX_IO>(spikeLeds[LANE], 170);
#endif
}

void setup() {
#if CORE_SPIKE_MODE == 2
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    for (int lane = 0; lane < 6; ++lane) {
        fl::ChannelConfig cfg(lane + 1, timing, spikeLeds[lane], GRB);
        cfg.options.mWorker.core = fl::Core::Core1;
        cfg.options.mWorker.priority = 23;
        FastLED.add<fl::Bus::FLEX_IO>(cfg);
    }
#else
    addSpikeLegacy<1, 0>();
    addSpikeLegacy<2, 1>();
    addSpikeLegacy<3, 2>();
    addSpikeLegacy<4, 3>();
    addSpikeLegacy<5, 4>();
    addSpikeLegacy<6, 5>();
#endif
}

void loop() {
    if (spikeLegacyControllerSize == 0 || spikeChannelSize == 0) {
        return;
    }
    ++spikeLeds[0][0].r;
    FastLED.show();
    delay(20);
}
```
