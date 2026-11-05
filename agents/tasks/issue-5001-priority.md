# Named worker priorities (#5001)

This is the historical implementation and validation record for #5018
(`302a26ea9742c1a9570d5b24e5a56948761f9f8d`). It records the API and evidence
at that revision, before the channel-only core-affinity follow-up.

At #5018, `FastLED.addLeds()` accepted a trailing `fl::Priority` after
`fl::Core`, and existing default/core-only calls and core-aware rebind aliases
remained supported. Runtime channels used `config.options.mWorker.priority`.
Both accepted `Auto` (default), `High`, `Medium`, and `Low`.

The current API removes both legacy worker arguments: explicit core affinity
and priority belong to channel configuration, while legacy templates retain
only chipset, pin, bus, and bus-instance selection. See the
[current API and migration guidance](../../src/fl/channels/README.md#esp32-s3-lcdi2s-clockless-worker-scheduling).

The LCD/I2S clockless worker maps a new `Auto` worker to `High`, preserving
its old priority. On the current ESP32 SDK, High is 23
(`configMAX_PRIORITIES - 2`), Medium is 12 (half the priority count), and
Low is 1. Auto inherits an existing explicit policy. Shared-engine conflicts
and late changes remain rejected. Classic ESP32 I2S has no configurable task;
explicit worker settings on unsupported backends are rejected.

## Memory evidence

These are the historical measurements for adding named priorities in #5018,
not measurements of the subsequent core-template removal.

Matched baseline: `f9beb26cee26d48a6d7213c1c1464f157a133754`.
Both measurements used fbuild 2.5.51, default Blink profiles, GCC 14.2.0 /
Arduino-ESP32 3.3.11 / IDF 5.5.5 for ESP, and AVR GCC 7.3.0 for Uno.
No budgets or build flags were relaxed.

| Target | Allocated image bytes before / after | Static RAM bytes before / after |
|---|---:|---:|
| ESP32-S3 | 320259 / 320259 | 73384 / 73384 |
| ESP32dev | 287555 / 287555 | 23520 / 23520 |
| Uno | 3600 / 3600 | 118 / 118 |

Image sizes are `bash bloat`'s `image_flash`. Static RAM is the compiler's
physical RAM summary, not bloat's attributed/vtable-aliased RAM. Attributed
RAM is also unchanged: S3 22451, ESP32dev 18651, Uno 134 bytes.

WorkerConfig shrinks from 3 to 2 bytes. Frozen configuration-layout tests
guard both it and ChannelOptions. Priority templates add no controller
fields, and default construction does not access the driver singleton.
The LCD task stack remains 4096 bytes; scheduling adds no new allocations.
The RTOS mapping is compiled only with the supported LCD backend.

Extending template names changed S3 linker relaxation by four allocated
bytes. Removing a redundant word-count argument from the RMT allocation
failure helper recovered them: it derives the same count only when logging
is enabled, without changing allocation, fallback, or diagnostic behavior.

Reproduce with `bash bloat <board> --build --no-summary` for each target.
Local logs/reports are in `.build/evidence/issue-5001-priority/{before,after}`.

## Validation

- RED: sanitizer test confirmed old WorkerConfig is 3 bytes against a 2-byte target.
- GREEN: named-policy forwarding, numeric mapping, default inheritance,
  conflicts, invalid values, lazy construction and equal-size controllers.
- Full sanitizer suite: **330/330 unit groups and 95/95 examples passed**.
- Targeted LCD, slim-controller and RMT allocator sanitizer groups passed.
- `bash compile esp32s3 --examples AutoResearch` passed. Its existing worker
  fixture now exercises explicit High through template and runtime APIs;
  the template Auto case still omits the arguments to validate defaults.
- Full `bash lint` passed. Repository and pre-push review found no actionable issues.

Port scan and the running firmware identified two C6 USB boards plus the
ESP32dev UART board. No S3 was attached, so live worker affinity/priority and
wire captures are not claimed; coverage is native behavior and S3 SDK compilation.
