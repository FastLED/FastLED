# ESP no-throw contract and release flash audit

Issue: [FastLED #4773](https://github.com/FastLED/FastLED/issues/4773).
Implementation branch: `fix/esp-noexcept-4773`, baseline `61d6ccffeb`.
This record preserves local measurements and coverage boundaries. Exact-SHA
CI conclusions and the merge decision are published on
[PR #4774](https://github.com/FastLED/FastLED/pull/4774).
Fresh measurements at source checkpoint bea5017753 reproduce the four image
results below after the annotation and public-header repairs.

## Contract

The maintainer explicitly authorized codebase-wide annotation cleanup and
removal of the 7,078-entry grandfathered lint baseline in
[the scope directive](https://github.com/FastLED/FastLED/issues/4773#issuecomment-6048066130).
The bulk pass added 6,137 annotations across 476 files, followed by matching
declaration and inactive-platform repairs. Unity assembly is preserved; the
optional NeoPixelBus adapter's two owned catch wrappers are removed. The semantic AST check expands the macro to real noexcept,
enables compiler exceptions to expose mismatches, and fails on parser errors;
annotation text in callback types or lambdas cannot suppress an outer function.
Full lint and 86 focused tests passed. ESP32 QEMU strict10 passed. Full Python
passed 1,836 tests and 3,257 subtests (25 skips, two expected failures). Native
debug passed 318/319 units and 95/95 examples, exposing a test-only semaphore
phase-observation race; explicit acknowledgement replaced its timing assumption,
and targeted debug verification passed. Subsequent full native debug verification
passed 318 units and 95 examples.
Ordinary examples do not use the library annotation macro. AutoResearch is an
explicit maintainer-approved exception for testing requirements.

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

One concrete archive selection path in the preserved before map is:

```text
src_bbc1.o -> AudioManager::instance() -> extracts fl.audio+_8e53.o
                                          |
                         function text is discarded by section GC
                                          |
                         audio .eh_frame remains allocated: 29,556 B
                         because SDK KEEP retains frame metadata
```

Before-map lines27–28 record extraction, lines37774–37775 discarded text,
line131839 the allocated frame input, and line169062 the cross-reference.
The original before ELF/FDE relocations were not preserved, so this is an
object/section graph rather than a complete symbol-to-FDE graph.

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
`before-`, `policy-only-`, `clean-after-`, `final-`, `head-` and
`merged-candidate-`. The latter records source checkpoint bea5017753.
CI run37699074987 measured S3 image350383 against
its pinned366603 baseline (16220 B saving); this is within16 B of the local
350367 result. The CI ratchet is lowered to350383 to claim that saving.
Raw board flash/RAM summaries can include IRAM and must not replace allocated
image/static DRAM measurements.

The legacy map retained 18 FastLED `.eh_frame` inputs (170,704 bytes) and
31 `.gcc_except_table*` inputs (491 bytes), totaling 171,195 bytes. Both sets
disappeared in the release candidate. SDK/libstdc++ symbols
including `__gxx_personality_v0`, `__cxa_throw` and `std::terminate` remained.
This change does not claim to remove all system exception support.

`fl::Singleton` can address independently rooted global initialization/storage;
it does not prevent the compiler emitting EH metadata or override linker KEEP.
Neither a singleton migration nor splitting unity groups is required here.

## ESP compile inventory

The local family matrix and feature logs establish these resolved profiles.
Final exact-SHA hosted results are linked from PR #4774; earlier local feature
checks are checkpoint evidence and are not relabeled as final-SHA executions.
Every release profile's recorded C++ flags end with effective `-fno-exceptions`.
ESP32 profiles can inherit earlier `-fexceptions`; the final flag wins.

| Board profile | Architecture | Arduino framework | SDK | GCC |
|---|---|---|---|---|
| esp8266 | Xtensa LX106 | 3.1.2 | NONOSDK22x_190703 | 10.3.0 |
| esp32dev_idf44 | Xtensa LX6 | 2.0.3 (package3.20003.220626) | IDF4.4.1 | 8.4.0 |
| esp32dev | Xtensa LX6 | 3.3.11 | IDF5.5.5 | 14.2.0 |
| esp32s2 | Xtensa LX7 | 3.1.0 | IDF5.3.2 | 13.2.0 |
| esp32s3 | Xtensa LX7 | 3.3.11 | IDF5.5.5 | 14.2.0 |
| esp32c2 | RISC-V | 3.2.0 | resolved IDF5.3.2 | 14.2.0 |
| esp32c3 | RISC-V | 3.1.0 | IDF5.3.2 | 13.2.0 |
| esp32c5 | RISC-V | 3.3.5 | IDF5.5.1 | 14.2.0 |
| esp32c6 | RISC-V | 3.3.5 | IDF5.5.1 | 14.2.0 |
| esp32h2 | RISC-V | 3.1.0 | IDF5.3.2 | 13.2.0 |
| esp32p4 | RISC-V | 3.3.5 | IDF5.5.1 | 14.2.0 |

Compiler versions come from actual fbuild build logs, not the wrapper's host
Clang field or cache-selected alias paths. C2's resolved SDK5.3.2 is confirmed
by its include-farm `esp_idf_version.h`, despite the Arduino3.2.0 profile name.
`build_info_Blink.json` records flag ordering for each staged profile.

Representative feature builds passed: S3 Json/Codec/Audio/NoisePlusPalette,
C6 SpecialDrivers/ESP/DriverTest, legacy MultipleEsp32SpiBuses/NoisePlusPalette,
and ESP8266 Esp8266Uart. ESP8266 NoisePlusPalette was filtered out and is not
claimed as compiled. AVR and WASM Blink compiled with existing non-ESP behavior.

The initial legacy ESP compiler probe failed against unchanged baseline
61d6ccffeb: missing `FL_HAS_NOEXCEPT`, false `noexcept(esp_noexcept_probe())`,
and false `noexcept(fl::move(...))`. The same probe passed after enabling the
macro. Logs are `/tmp/fastled-4773-red-probe.log` and
`/tmp/fastled-4773-green-probe.log`; collected tests in
`ci/tests/test_esp_noexcept.py` preserve capability, override and include-order
assertions. SDK strict10 QEMU additionally compiles with `-fexceptions` and real
annotations to expose mismatches; it does not authorize FastLED throw/catch.
SDK diagnostics preservation is static/configuration evidence; no physical
panic/backtrace hardware execution was performed.

## Validation limits

- Complete CI caught public FX/SPI controller headers absent from canonical
  implementation routers. Added a lint-only FX header inventory and reused the
  existing root router with broader source matching. Global physical-location
  deduplication prevents repeated TU findings from falsely consuming an array
  baseline entry twice. Production unity layout is unchanged.
- Full ATmega8A CI fails RGBW/RGBWEmulated by874/530 flash bytes. A detached
  unchanged origin/master61d6ccffeb worktree reproduces identical overflows.
  This is tracked separately in FastLED#4775; no workflow thresholds are changed.

- Debug-compatible SDK compilation is mandatory: disabling exceptions can
  suppress mismatched exception-specification diagnostics. The QEMU SDK build
  enables `-fexceptions` while retaining real FastLED annotations.
- Requested `esp32dev_idf6` resolved SDK 5.5.5, so it is not IDF6 evidence.
  Tracked in [fbuild #1662](https://github.com/FastLED/fbuild/issues/1662).
- The component-named profile is another Arduino-backed SDK build. fbuild
  ignores the requested component integration and does not invoke CMake
  `idf_component_register`; neither component nor bare `app_main` integration
  is validated. Tracked in [fbuild #1664](https://github.com/FastLED/fbuild/issues/1664).
- Full Python passed: 1,836 passed, 25 skipped, 2 xfailed and 3,257 subtests.
  The latest focused annotation/compiler fixture inventory passed 96 tests.
  Full native debug passed 318 unit tests and 95 examples; the repaired coroutine
  handshake also passed its separate targeted debug run. Full portable lint and
  strict10 SDK QEMU passed. Exact-SHA complete CI remains required before merge.
- Windows CI exposed namespace friend lookup of `fl::CFastLED`; explicit
  `::CFastLED` selects the intended global class. A real Windows-mode Clang
  fixture reproduces failing lookup and passes after qualification. Final review
  found no remaining correctness or scope findings.
- Subsequent Windows strict AST validation exposed an MSVC parser profile while
  native builds use the bundled GNU Windows toolchain. Shared parser arguments
  now use that native GNU target/sysroot/headers; missing tooling fails closed.
  Standalone and combined AST checks use the same profile. Corrected the installed
  query entrypoint name and removed misplaced annotation tokens from both Windows
  Sleep calls. An actual-header compiler probe instantiates both call paths with
  real noexcept. Full portable lint and the focused suite passed.
- Windows GNU parsing then exposed the unannotated inline errno helpers behind
  the annotated socket interface. Their actual-header contract probe records
  RED in `/tmp/fastled-4773-errno-red-debug.log` and GREEN after repair. Added a
  lint-only public STL inventory covering errno, formatting, range access and
  file-I/O headers; repaired 80 annotations including inactive AVR initializer
  helpers. An actual AVR-target C++11 probe validates the fallback. Full portable
  lint and 96 focused tests passed; review is clean.
- Refreshed clean four-profile fbuild reports at96de698a89 reproduce all image
  and static DRAM measurements above. Preserved `validated-*.json`, `.map` and
  provenance in `.build/size-4773`. Wrapper provenance misidentifies the compiler
  as host Clang; actual build logs record GCC8.4/GCC14.2/GCC13.2 as appropriate.
- MP3 CPU audit compiler drift is tracked in FastLED#4779. Unchanged master and
  this PR both produce float dct32 instruction count47 with explicit GCC13.2;
  explicit GCC14.2 produces44, matching the baseline. Decoder and audit sources
  are identical before/after; no audit threshold or workflow changes are included.
- Ordinary example sketches have no annotation edits. AutoResearch is explicitly
  exempt under the maintainer's clarification and retains its testing macros.
- PR #4736 stays closed and unmerged. Production unity routers, compilation
  units and vendor linker scripts are unchanged.
- Fresh four-profile builds at79f51bfa12 reproduce the table; `head-*.json`,
  maps and provenance are preserved under `.build/size-4773`.
- Final runtime audit removed two catch wrappers from the optional NeoPixelBus
  adapter and repaired two constructors and two invalid local annotation tokens.
  A collected guard scans owned runtime code, including inactive platforms,
  for exception keywords after stripping comments and strings; vendor code and
  the nonproduction assertion framework retain their independent contracts.
  All39 targeted ESP policy tests passed in debug mode.
- Actual NeoPixelBus2.8.4 adapter RGB/RGBW template instantiations compile against
  ESP32 SDK4.4.1 with exceptions disabled when a test-only macro isolates the
  adapter's existing `ClocklessController` name collision. Ordinary inclusion
  fails that collision; this is not passing end-to-end adapter integration.
  Integration is tracked separately in [FastLED #4780](https://github.com/FastLED/FastLED/issues/4780).
