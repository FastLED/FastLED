# ESP noexcept implementation (#4773)

Issue: https://github.com/FastLED/FastLED/issues/4773
Branch: `fix/esp-noexcept-4773`; starting master `61d6ccffeb`.

## Requirements and sequence

1. Capture unchanged Blink fbuild baselines for legacy ESP32, modern ESP32,
   S3 and RISC-V C3. Preserve bloat JSON and linker maps before overwriting.
2. Record a failing ESP compile-time macro probe. Implement real ESP-only
   annotation selection, capability reporting, include-order and override
   behavior; preserve non-ESP selection.
3. Retain nonthrowing contracts and repair matching
   declarations/definitions/overrides. Preserve SDK C signatures and inferred
   defaulted special-member specs. Do not transplant the experiment wholesale.
4. Compile the complete ESP family matrix and feature consumers with fbuild,
   including legacy SDK and modern component integration. Record actual flags
   and versions, plus genuinely unsupported combinations.
5. Verify real annotations with exception-disabled compiler-feature probes
   and native sanitizers. Run required native/example and lint gates; check
   unchanged AVR/WASM/host behavior.
6. Measure allocated flash, physical static DRAM, retained unwind sections and
   relevant symbols against unchanged baseline layouts/flags. Publish exact-SHA
   validation, push a focused implementation PR, address reviews and merge only
   after the issue requirements are proven.

## Scope

PR #4736 remains closed unmerged. No changes to production unity layout,
canonical compilation units or linker scripts. The maintainer explicitly directs
FastLED assumes no throw/no catch in every mode; the system may enable
exception frames in debug for crash tracing. Scoped ESP compiler policy changes
are authorized in #4773; preserve SDK fault/panic stack traces and unwind settings.
No new GitHub Actions workflows. No release/tag publication is authorized.

## Initial evidence

Current master still defines FL_NO_EXCEPT empty everywhere. The platform header
detects ESP8266 and ESP32/S2/S3/C2/C3/C5/C6/H2/P4. Read-only audits identified
malformed platform annotations, signature mismatches and SDK C forwards.
The old experiment has a different source layout, so cannot be used as production proof.

## Maintainer correction

FastLED assumes no throw and neither throws nor catches in any mode. The system
may enable exception frames in debug for crash tracing. ESP SDK CPU
fault/panic handling sits below the HAL and remains intact. Removed the invented
callback exception propagation changes, RAII support and exception-enabled test
target. Issue #4773 updated to supersede the earlier debug opt-in interpretation.

## Validation checkpoint

- Native debug: 319/319 unit tests and 95/95 examples passed. Annotations added
  afterward remain empty on the host; final exact-SHA validation is still needed.
- Focused macro/release-policy tests: 43 passed, 25 subtests. Includes a
  system-exceptions-enabled compiler probe with a real FastLED no-throw contract.
- Full lint passed; subsequent definition-only repairs need a final rerun.
- Blink passed for ESP8266 and all ten registered ESP32 chip families. Legacy
  SDK 4.4.1 and modern SDK 5.x profiles passed. Requested IDF6 resolved SDK 5.5.5,
  so this is explicitly not proof of IDF6 support. Component profile builds with
  Arduino plus IDF; it is not standalone app_main coverage.
- Feature builds passed: S3 Json/Codec/Audio/NoisePlusPalette, C6 DriverTest,
  legacy ESP32 MultipleEsp32SpiBuses/NoisePlusPalette, ESP8266
  Esp8266Uart. ESP8266 NoisePlusPalette was filtered out by its large-memory
  requirement and is not compile evidence. Non-ESP AVR Blink and WASM Blink passed.
- Clean legacy Blink passed via documented fbuild fallback (`--clean` is absent
  from the compile wrapper). Final four-profile size measurements remain pending.
- Full Python has one failing QEMU test. The real SDK compile enables exceptions
  and exposes missing specification matches; current diagnostic log is
  `/tmp/fastled-4773-qemu-debug-green-8.log`. Repair declarations/definitions
  without adding exception propagation. This is not proof of a cache defect.
- Preliminary size savings were entirely reproduced by release compiler policy
  alone. Keep `.build/size-4773` baseline maps/reports; do not report preliminary
  numbers as final or attribute them to annotation-only optimization.

No new implementation commit, PR, push, or merge yet. Old PR #4736 remains closed.

## Explicit scope expansion: strict annotations everywhere

The maintainer now directs removal of all grandfathered baseline exceptions and
completion of the longstanding no-throw annotation backlog across the codebase.
Broad directly relevant annotation edits and strict lint/tooling changes are
authorized. Unity layout, bodies and vendor linker policies remain unchanged.

- Emptying `ci/tools/noexcept_baseline.txt` produces RED with `bash lint --cpp`;
  log `/tmp/fastled-4773-strict-noexcept-red.log`, terminal exit 1.
- The old baseline had 7,078 entries. Do not recreate a debt baseline to pass.
- Existing bulk refactor tool is incomplete: one-TU discovery, misspelled macro,
  ignored parser failures and insufficient C API protection. Repair before use.
- Agents own strict checker wiring and safe bulk-tool repairs; root runs all
  mutation tools and validation. Review and measurements must be refreshed for
  this broader candidate.
- Applied the repaired bulk tool: 6,137 annotations across 476 files, then
  removed the historical annotation baseline entirely. Initial full lint passed.
- Strengthened AST parsing to expand `FL_NO_EXCEPT` to real `noexcept` with
  compiler exceptions enabled. This exposes declaration/definition mismatches
  and misplaced annotations that parsing an empty macro could not detect.
- Real-contract lint iterations 4–8 exposed WLED declaration matches, HSV16 and
  ScreenMap specifications, and pre-existing misplaced local lock annotations.
  All reported instances are repaired. Iteration 9 then exposed 15 operator[]
  misses previously hidden by the lambda signature exemption. Twenty actual
  operator[] declarations/definitions were repaired across eleven files.
  Real-contract C++ lint iteration 10 passed, terminal exit 0.
- Hardened the source filter: same-line body annotations cannot hide an outer
  function, and annotation tokens inside callback types cannot suppress a
  finding. The AST's actual isNoThrow result decides the function contract.
  Full lint broad iteration 4 passed; focused suite iteration 5 passed 82 tests.
- QEMU strict iteration 7 exposed four ESP filesystem overrides. These and two
  constructors are repaired. Iteration 8 then exposed ESP-specific getFreeHeap
  and pinToPort definitions; matching annotations are being repaired.
  All real-platform branches in those two files are now annotated; QEMU strict
  iteration 9 exposed the ESP audio factory definition. All dispatcher branches
  and matching Teensy/WASM factory definitions are repaired. QEMU strict
  iteration 10 passed (exit 0); full Python debug suite is running.
- Focused collected debug Python suite passed 75 tests after operator[] and
  conditional-initializer placement regressions were added.
- Full native debug run was interrupted (exit 130) after concurrent edits
  invalidated its precompiled header. Restart after source repair batches finish.
  Stable-source native debug iteration 2 is running, alongside refreshed modern
  ESP32 Blink compilation. Final source review is assigned to the same reviewer.
- Review found a default-argument lambda exemption loophole; removed the source
  lambda exemption and added regression coverage. The same reviewer confirmed
  no remaining findings. Full lint broad6 passed; focused83 tests passed.
- Modern ESP32 Blink passed, then was restaged after the last platform-only
  audio repairs for final measurement. Remote fetch confirms origin/master
  remains the existing base; origin's default branch is master.
- Checkpoint001a0dc17e pushed in draft PR#4774 with ci-full. Four fresh clean
  Blink measurements reproduce prior values; full Python passes1836 tests.
- Complete CI exposes public FX/SPI header gaps; repaired named signatures and
  broadened lint-only inventory without changing production unity routers.
  Global AST location dedup fixes duplicate array baseline consumption.
  Full lint publicheaders2 and86 focused tests pass; same reviewer is clean.
- Ordinary examples remain free of FL_NO_EXCEPT. AutoResearch is explicitly
  exempt and its existing testing annotations are preserved.
- CI bloat ratchet lowered366603->350383 per run37699074987. Native test-only
  coroutine phase race repaired with acknowledgement semaphore, targeteddebug
  passes; fullnative rerun pending. ATmega8A overflow is identical on unchanged
  master and is tracked in#4775; scope clarification is pending.
- Four clean final size reports record broad checkpoint001a0dc17e and reproduce
  policy-only savings. Native debug rerun passed318 units and95 examples, with
  coroutine separately passing its targeted debug run. Full Python passed1836
  tests. Later public-header and Windows parser fixes require exact-SHA full CI.
- Windows strict AST now receives the same GNU target/sysroot/header profile as
  native builds, and tooling failures remain fatal. Corrected the installed query
  fallback name; removed misplaced tokens on both Sleep calls. The actual-header
  compiler probe and focused93 tests passed; full portable lint passed. The same
  reviewer checked these repairs. MP3 codegen compiler drift is tracked in#4779.
- Refreshed four clean fbuild size reports at96de698a89 reproduce the same image
  savings/static DRAM. Windows GNU strict AST then exposed missing errno helper
  contracts. Compiler-feature RED->GREEN confirms the repair; public STL inventory
  also covers formatting, range and file-I/O header-only entry points. Repaired80
  annotations including AVR fallback, with an actual AVR C++11 compile probe.
  Full portable lint and96 focused tests passed; review is clean.
