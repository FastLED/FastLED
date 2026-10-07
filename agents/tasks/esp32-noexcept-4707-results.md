# Real noexcept and unity restoration experiment (#4707)

Date: 2026-10-07. Branch: `experiment/esp32-noexcept-4707`.
Parent checkpoint: `f29056fc189b42a297558106fc6f0c88b37469f9`.
Follow-up: https://github.com/FastLED/FastLED/issues/4771

## Executive summary

`FL_NO_EXCEPT` was globally a noop. Enabling real `noexcept` produces a
measurable flash reduction on two ESP32 toolchain profiles. It also allows
restoring the optional string implementation group to the existing STL unity
build at a much smaller size cost. This is experimental compiler and binary-size
evidence, not permission to enable the macro globally in production.

## Controlled legacy ESP32 comparison

Default Blink, `esp32dev_idf44`, unchanged sketch and build flags. Measurements
use allocated `image_flash` from `bash bloat`; RAM is physical `.dram0.data` plus
`.dram0.bss` from the linker map.

| String code layout | Macro disabled | Real noexcept | Saving |
|---|---:|---:|---:|
| Separate optional archive unit | 273,173 B | 256,613 B | 16,560 B |
| Restored to STL unity group | 276,133 B | 256,745 B | 19,388 B |

Static DRAM is 17,496 B in all four cases. The restoration penalty falls from
2,960 B to 132 B. Restoring unity with real noexcept saves 16,428 B against the
previous split/noop checkpoint. Noop/split was rebuilt with the signature repairs
present and exactly reproduces the checkpoint's flash/RAM measurements.

After C++11/native compatibility repairs to callback pointer aliases, the final
restored legacy build is **256,801 B / 17,496 B DRAM**: 56 B above the earlier
four-way snapshot, still 16,372 B below the checkpoint. The four-way table
records its own controlled snapshot; do not substitute the final value into it.

## Other profiles, original split layout

| Profile | Noop flash | Real noexcept flash | Saving | Static DRAM (both) |
|---|---:|---:|---:|---:|
| Modern `esp32dev` | 289,927 B | 289,927 B | 0 B | 25,036 B |
| `esp32s3` | 353,571 B | 344,691 B | 8,880 B | 23,416 B |

Legacy and S3 flags enable exceptions. Modern ESP32's final effective flag is
`-fno-exceptions`, consistent with no measured benefit from the annotations.
These are default Blink measurements; do not compare them directly with older
Serial Blink or the public gist to claim complete recovery to 3.10.3.

With the string group restored and real noexcept, modern ESP32 compiled at
289,975 B (+48 B versus split) and S3 at 344,763 B (+72 B versus split).
Static DRAM remains unchanged on both. All three restored SDK builds passed.

## Why unity creates the size pressure

```text
Blink -> live STL operation -> archive member fl.stl+.cpp.o extracted
                               |
                               +-- live function sections -> flash
                               |
                               +-- optional string function sections
                               |       normally candidates for --gc-sections
                               |
                               +-- compiler-generated unwind/exception metadata
                                       |
ESP32 SDK sections.ld: KEEP(*(.eh_frame)) --+
                                       |
                                       +-- retained metadata, relocation edges

real noexcept -> changed code generation and smaller allocated metadata
              -> lower retained metadata cost within the same unity group
```

One demonstrated retention root is the ESP32 SDK linker policy retaining
`.eh_frame` from
extracted FastLED archive members. FastLED generates the metadata; ESP/HAL and
SDK dependencies can also be genuinely live through normal driver calls.
A relocation or map cross-reference alone does not prove the target function
body survives: `.eh_frame` has special linker GC handling. Measure the allocated
output and inspect retained input sections before claiming a function is pinned.

Examples from the legacy map before/after real noexcept (split layout):

| FastLED owner | Retained `.eh_frame` before | After |
|---|---:|---:|
| STL | 4,344 B | 844 B |
| Channels | 3,188 B | 728 B |
| Platforms | 4,888 B | 1,652 B |
| System | 644 B | 40 B |
| Root | 2,356 B | 488 B |

These metadata savings are part of the total binary reduction; they are not
counts of function bodies elided. `fl::Singleton` can avoid an unused eager
initializer, but cannot remove SDK `KEEP` policy, live calls, or live tables.
EngineEvents already uses Singleton and is called from live frame/strip paths.

## Compatibility and semantic limits

Enabling the macro required approximately 180 files of repairs: declarations,
definitions and virtual overrides had inconsistent specs, some annotations
were placed on expressions or variables, and C++11 cannot put exception specs
in the affected function-pointer aliases. C vendor APIs retain their declared
signatures. Three default constructors keep compiler-inferred exception specs;
forcing noexcept caused the old GCC toolchain to delete them.

A read-only review verified signature repair bodies and initializers against
the checkpoint after normalizing macro tokens and whitespace. Runtime behavior
can nevertheless change: `fl::function`, variant visitors, allocators and
Singleton constructors can call arbitrary user code. An escaping exception from
a newly noexcept function terminates. Passing nonthrowing examples does not
validate these generic contracts.

## Recommendation

Keep this branch as a measured experiment. Audit generic/user callback contracts
and use real or conditional noexcept only where valid, with matching declarations
and definitions. Keep native compilation and lint compatibility as required
gates. Re-measure after narrowing the annotations; do not assume the full
global-enable saving survives a safe subset. Restore unity groups one at a time
using the same four-way comparison. No SDK linker-script patch is needed for
the demonstrated string result.

Machine-readable measurements: `esp32-noexcept-4707-measurements.json`.
Local full maps/reports: `.build/size-4707/noexcept-experiment/`.

## Validation and remaining gates

- SDK Blink: legacy, modern and S3 all compile with restored string unity and
  real noexcept. Final legacy image was re-measured after callback alias fixes.
- Native `bash test move --debug`: passed, including the `FL_HAS_NOEXCEPT`
  compile-time assertions. Clean setup was used to regenerate the stale native
  inventory. Initial failures exposed malformed annotations; repaired before
  the passing run. A transient zccache exit 113 was retried by the wrapper.
- Native `bash test function --debug`: passed.
- Native `bash test tests/fl/stl/string_search_a.cpp --debug`: passed against
  the restored string unity group. An earlier ambiguous `string` selector
  performed no build/test; the explicit path above is the actual validation.
- Final read-only review: bodies/initializers match the checkpoint after
  normalizing macro tokens and whitespace; `git diff --check` passes.
- Lint is not green: both initial and final `bash lint --cpp` reported 25 violations:
  two array-parameter warnings in transposition definitions, twenty annotation
  warnings across audio/C vendor definitions, and three default constructors.
  Audio/C vendor declarations and inferred default-ctor specs were deliberately
  preserved to get valid compiler contracts. Do not force incorrect noexcept
  promises to silence the checker. Reconcile these contracts and the lint rules
  during the production follow-up.
- No full native/example suite, hardware execution or hosted exact-SHA matrix
  has been completed for this experiment. No production merge or release.

Reproduce board results with `bash compile <profile> --examples Blink` then
`bash bloat <profile> --no-summary`. Toggle only the macro for the control;
keep signature repairs, sketch and compiler flags identical. Restore/remove the
string group include, canonical wrapper and Rust canonical inventory together.
