# ESP no-throw contract and release flash audit

Issue: [FastLED #4773](https://github.com/FastLED/FastLED/issues/4773).
Implementation branch: `fix/esp-noexcept-4773`, baseline `61d6ccffeb`.
This is a working evidence record; exact-SHA validation gates are pending.
The clean size measurements below predate the subsequent codebase-wide
annotation migration and require a fresh build before final reporting.

## Contract

The maintainer explicitly authorized codebase-wide annotation cleanup and
removal of the 7,078-entry grandfathered lint baseline in
[the scope directive](https://github.com/FastLED/FastLED/issues/4773#issuecomment-6048066130).
The bulk pass added 6,137 annotations across 476 files, followed by matching
declaration and inactive-platform repairs. Unity assembly and function bodies
are preserved. The semantic AST check expands the macro to real noexcept,
enables compiler exceptions to expose mismatches, and fails on parser errors;
annotation text in callback types or lambdas cannot suppress an outer function.
Full lint and 83 focused tests passed. ESP32 QEMU strict10 passed. Full native
and Python suites and refreshed measurements remain pending at this checkpoint.

FastLED assumes no throw and neither throws nor catches, including callbacks.
`FL_NO_EXCEPT` expands to `noexcept` on ESP in both release and debug modes.
ESP release example staging appends `-fno-exceptions`. Debug staging preserves
the system's compiler policy. SDK unwind, panic and CPU fault handling are
unchanged. A CPU fault/backtrace is not evidence of C++ throw/catch support.

## Retention graph

```text
Sketch: FastLED.show()
        |
        v
Required FastLED functions select unity archive members
        |
        +-- ordinary function/data sections --> linker garbage collection
        |
        +-- compiler emits C++ exception/unwind metadata when enabled
                   |
                   v
           SDK linker KEEP(.eh_frame)
                   |
                   v
           allocated flash retains FastLED frame metadata
           across selected unity members, including unused functions

Release compiler -fno-exceptions --> avoids FastLED EH emission
                                    unity layout stays intact

Debug system exception policy --> frames may remain for diagnostics
                                 FastLED still promises noexcept
```

The legacy before map allocates FastLED `.eh_frame` inputs from `fl.gfx+`,
`fl.log+`, `fl.math+`, `fl.stl+`, `fl.system+`, `fl.task+`, `platforms+`, `src`,
`fl.audio+`, `fl.channels+`, `fl.net+`, `third_party+`, `fl.fx+`, `fl.video+`,
`fl.fled+`, `fl.fs+`, `fl.fs.sd+` and `fl.codec+` archive objects.
This metadata is emitted from our compiled objects, including vendor source
compiled in our unity members. SDK and C++ runtime metadata are separate inputs.

The matching [ESP-IDF 4.4.1 linker template](https://github.com/espressif/esp-idf/blob/v4.4.1/components/esp_system/ld/esp32/sections.ld.in)
keeps `.eh_frame` in `.flash.rodata`. The before linker map confirms allocated
input sections. This proves retained metadata; proving each individual code
symbol is rooted specifically by its frame requires relocation inspection.
Do not conflate retained frames with a complete relocation graph of all code.

## What the measurements establish

Allocated flash savings for legacy ESP32, S3 and C3 were reproduced
with the release compiler policy and an empty `FL_NO_EXCEPT` override. Therefore
those savings are attributable to compiler policy, not an annotation-only gain.
The modern ESP32 baseline already disabled exceptions and showed no saving.
Clean Blink rebuilds reproduced the same allocated image measurements:

| Profile | Before image bytes | After image bytes | Saving | Before static DRAM | After static DRAM |
|---|---:|---:|---:|---:|---:|
| ESP32 SDK 4.4.1 | 651,905 | 476,089 | 175,816 | 24,572 | 24,572 |
| ESP32 SDK 5.5.5 | 298,919 | 298,919 | 0 | 25,884 | 25,884 |
| ESP32-S3 SDK 5.5.5 | 366,375 | 350,367 | 16,008 | 24,248 | 24,248 |
| ESP32-C3 SDK 5.3.2 | 368,202 | 337,294 | 30,908 | 14,108 | 14,100 |

Maps and bloat reports are preserved under `.build/size-4773`, prefixed
`before-`, `policy-only-` and `clean-after-`. These are working-tree candidate
measurements; final commit and CI provenance must be recorded before merge.
Raw board flash/RAM summaries can include IRAM and must not replace allocated
image/static DRAM measurements.

The preliminary legacy map retained 49 FastLED EH input rows totaling 171,195
bytes; those rows disappeared in the release candidate. SDK/libstdc++ symbols
including `__gxx_personality_v0`, `__cxa_throw` and `std::terminate` remained.
This change does not claim to remove all system exception support.

`fl::Singleton` can address independently rooted global initialization/storage;
it does not prevent the compiler emitting EH metadata or override linker KEEP.
Neither a singleton migration nor splitting unity groups is required here.

## Validation limits

- Debug-compatible SDK compilation is mandatory: disabling exceptions can
  suppress mismatched exception-specification diagnostics. The QEMU SDK build
  enables `-fexceptions` while retaining real FastLED annotations.
- Requested `esp32dev_idf6` resolved SDK 5.5.5, so it is not IDF6 evidence.
  Tracked in [fbuild #1662](https://github.com/FastLED/fbuild/issues/1662).
- The component profile uses Arduino plus IDF; it is not bare `app_main` proof.
- Native, release ESP family/feature, AVR and WASM checks passed as recorded in
  the plan. Full Python/debug QEMU, final lint and exact-SHA CI are pending.
  Pre-push review found no remaining correctness/scope findings; final additional
  definition matches still require validation.
- PR #4736 stays closed and unmerged. Production unity routers, compilation
  units and vendor linker scripts are unchanged.
