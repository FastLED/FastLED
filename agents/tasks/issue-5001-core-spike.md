# Spike: worker configuration only in the Channels API

The primary comparison keeps the sketch on legacy `addLeds<>` before and
after removing core template arguments. It saves **0 flash bytes and
0 static DRAM bytes**. No runtime Channels API calls are compiled in these
legacy cases. The earlier migration comparison changed APIs and must not be
used as the result of the requested removal spike.
This is an experimental branch; the breaking API change is not merged.

## Matched measurements

Six WS2812B strips, 170 LEDs each, GPIO 1–6, ESP32-S3 LCD clockless
(`Bus::FLEX_IO`). All values are bytes.

| Build | Allocated image flash | Firmware binary | Static DRAM data+BSS |
|---|---:|---:|---:|
| Before: legacy, default worker | 321,915 | 322,032 | 26,076 |
| After: legacy, default worker | 321,915 | 322,032 | 26,076 |
| Before: legacy, template Core1 | 322,107 | 322,224 | 26,076 |

- Default legacy removal: **0 flash / 0 static DRAM saved**.
- Formerly explicit Core1 legacy call changed to the new legacy call without
  a core argument: **192 flash bytes / 0 static DRAM saved**. This also removes
  that caller's affinity request; it is not equivalent scheduling behavior.
- The primary before/after comparison keeps the same default scheduling.

## What RAM means here

The target ELF size probe reports `sizeof(WS2812B<1, GRB>) == 52` before and
after. Core selection added no per-controller field. The legacy cases do not
construct full Channel objects. The shared harness also contains a constant
Channel-size probe; `sizeof` does not construct an object or link its runtime
API. Probe overhead is identical in both revisions. Static DRAM does not
measure runtime heap or FreeRTOS task stacks. No free-heap measurement or
on-device run is claimed.

The DRAM column sums `.dram0.data`, `.dram0.bss`, and `.noinit` directly
from each ELF. The 45,056-byte `.dram0.dummy` reservation is unchanged in all
five builds. It represents the shared IRAM/DRAM physical SRAM overlap, not
fictitious disposable memory; it is excluded only from the **DRAM data+BSS**
column. These figures are not total internal SRAM consumption or free heap.
The raw fbuild RAM headline includes that reservation. Symbol-attributed RAM
is another metric and is not used as physical RAM here.

Allocated image flash sums allocated, file-backed ELF sections, independently
of symbol aliases. Firmware binary size is also shown independently.

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
- The primary tests explicitly select `Bus::FLEX_IO` through legacy
  `FastLED.addLeds<>` on both revisions.
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

The legacy-only result supports treating removal as API simplification;
ordinary legacy callers show no measured memory saving.

## Separate migration experiment (excluded from removal result)

The earlier report also measured replacing legacy calls with configured full
Channels. That changes the controller implementation and retained runtime
machinery, so its growth is not a cost caused by removing template arguments.
It is retained here only to explain the misleading earlier comparison.
This auxiliary caller used typed `FastLED.add<Bus::FLEX_IO>(cfg)`, so the
increase did not come from enrolling every available driver.

| Auxiliary build | Allocated image flash | Firmware binary | Static DRAM data+BSS |
|---|---:|---:|---:|
| Before: configured Channel, Core1/priority 23 | 341,631 | 341,744 | 26,844 |
| After: configured Channel, Core1/priority 23 | 341,627 | 341,744 | 26,844 |

The full Channel object is 340 bytes and is allocated dynamically; it is not
constructed in the primary legacy tests. None of these migration figures
belongs in the legacy-removal savings result.

## Reproduction sketch

Use this in the existing AutoResearch scratch target, preserving its original
contents for restoration. Use **mode 0 on both revisions** for the requested
legacy-only comparison. Mode 1 intentionally compiles only on the baseline.
Mode 2 belongs solely to the separate migration experiment above; its
`FastLED.add(cfg)` calls are preprocessed out of modes 0 and 1.

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
