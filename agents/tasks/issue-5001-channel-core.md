# Channel-only LED worker core affinity

This is the historical implementation and validation record for #5022
(`e5016e7c09f082bb2e9f4ed332532a99b43e9ea7`). Its accepted behavior retained
legacy priority templates while removing core templates. The subsequent
channel-only scheduling change removes legacy priority templates as well;
see the [current API and migration guidance](../../src/fl/channels/README.md#esp32-s3-lcdi2s-clockless-worker-scheduling).
The measurements and validation below belong to #5022 and are preserved as
historical evidence, not new measurements of the scheduling follow-up.

Implement the design accepted at that time on master
`302a26ea9742c1a9570d5b24e5a56948761f9f8d`.

- [x] Verify failing tests for legacy core-template removal, then debug GREEN.
- [x] Remove legacy core selection while preserving named priority and bus
  selection, custom RGBW/16-bit encoders, and default scheduling.
- [x] Retain core affinity exclusively in ChannelConfig worker settings;
  reject explicit legacy affinity in AutoResearch callers.
- [x] Update migration documentation and current examples.
- [x] Validate targeted/full native tests, Python tests, and ESP32-S3 builds.
- [x] Run repository code review.

Publication gate: create the production PR with `ci-full` and
`ci-platforms-unit-test` already applied. Completion requires review clearance,
successful exact-head full CI, and normal merge; the PR's current checks and
merge state provide that final evidence.

The earlier spike remains a historical, pinned-baseline experiment in #5020.
Its memory results must not be described as measurements of this newer base.
Only legacy `addLeds<>` comparisons measure removal cost; migration to the
full Channel API is a separate experiment. No local release or device flash
is part of this change.

## Accepted API behavior

Legacy clockless calls accept `Priority` directly after `Bus`; SPI calls
accept it after `Bus` and `B_WHICH`. They no longer accept a `Core` argument
or need a `Core::Auto` placeholder. Existing default scheduling, named
priorities, custom encoder hooks, and bus rebinding remain supported.

Core selection uses `config.options.mWorker.core` when adding a Channel.
Worker configuration is shared by the engine. Priority-only legacy requests
leave the core automatic and inherit an existing channel-selected affinity;
the real LCD driver regression verifies this before and after worker creation.
Generic task affinity and channel-driver backend plumbing are unchanged.

## Current-base legacy-only measurement

ESP32-S3, six WS2812B strips of 170 LEDs on GPIO 1–6, LCD clockless
`Bus::FLEX_IO`, `FL_LOG_LEVEL=0`. The sketch includes only FastLED and uses
legacy `addLeds<>` in every measured case. No Channel construction, migration,
or Channel-size probe is part of these measurements.

| Case | Allocated image flash | Firmware binary | Static DRAM data+BSS |
|---|---:|---:|---:|
| Before: default legacy | 321,703 | 321,824 | 26,060 |
| Before: legacy Core1 | 321,871 | 321,984 | 26,060 |
| After: default legacy | 321,703 | 321,824 | 26,060 |

Unchanged default legacy use saves zero bytes. Removing the previous explicit
Core1 request saves 168 allocated image bytes and 160 padded binary bytes,
with zero static DRAM change; it also removes that affinity request.
The earlier spike's 192-byte result belongs to its older baseline/harness,
not this priority-enabled base.

The shared IRAM/DRAM reservation is 45,056 bytes in all three ELFs. Static
DRAM here sums `.dram0.data`, `.dram0.bss`, and `.noinit`; it is not total
internal SRAM or free heap. Runtime heap/task stacks and hardware behavior
were not measured. Image flash sums allocated, file-backed ELF sections.

All builds use the published fbuild 2.5.51 wheel, platform 55.03.311,
Arduino-ESP32 3.3.11, IDF 5.5.5, and Xtensa GCC 14.2.0. Original AutoResearch
was temporarily used as the scratch target and restored. Raw logs, ELFs,
binaries, build metadata, symbol reports, and metrics are retained under
ignored `.build/evidence/issue-5001-channel-core/`.

Local evidence: current-base negative API test failed both legacy-core
acceptance probes with 23 other cases passing, then passed in debug mode after
implementation. All 217 changed Python tests passed. Full lint and independent
production review are clean. Full native debug validation passed 330 unit
groups and 95 examples (252.86 seconds including setup). The restored
canonical AutoResearch S3 build passed as well. Hosted checks and final merge
state are the completion evidence.
