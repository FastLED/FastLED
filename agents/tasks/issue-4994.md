# Issue #4994: ESP32 local-static initialization under LTO

- [x] Add an AutoResearch two-task local-static contention probe.
- [x] Reproduce double construction with LTO before the fix.
- [x] Remove ESP32's unnecessary C++ ABI fallback.
- [x] Repeat AutoResearch with LTO and without LTO; inspect linked guard code.
- [x] Push and land both fbuild fixes and the 2.5.50 version bump.
- [x] Publish fbuild through GitHub CI and advance FastLED's dependency minimum
  with a local sync to that release.
- [x] Repeat the hardware checks using the published fbuild release.
- [x] Run lint, native tests, and code review.

Hosted delivery gate: push the dependency/evidence commit, create the #4994 PR
with `ci-platforms-unit-test` and `ci-full` already applied, observe the complete
required validation and review, and merge through repository policy.

Acceptance: both tasks finish, exactly one constructor runs, and ESP-IDF's
thread-safe guard implementation owns initialization with LTO enabled. No LEDs
or jumper wiring are required. Record board, port, commands, and output below.

## Evidence

- Final review tidy-up: reject the ignored TX-remux workload option, clarify
  the board reboot requirement, and encapsulate semaphore ownership while
  retaining resources on timeout. Both deferred LTO/control staging paths are
  covered. All **501** AutoResearch Python checks, lint, WASM compilation,
  and peer review passed. The updated C6 LTO probe passed with one
  construction, two completed callers, and suspended workers.

- Final scheduling robustness: an injected 400 ms RPC scheduling delay made the
  fixed 200 ms sample fail with one construction; a bounded two-second wait
  for the real blocked contender passed under that same delay. The temporary
  delay was removed. With the final probe, the original fallback under LTO
  still failed with two constructions; the fixed LTO and subsequent non-LTO
  control both passed with one construction, a blocked contender, two completed
  callers, and suspended workers. Artifacts: `c6-schedule-{before,after}.*` and
  `c6-bounded-{before,after,control}.*` in the evidence directory. All **482**
  AutoResearch Python checks, final lint, WASM AutoResearch, and the final local
  code review passed. The positive control has explicit parser/staging coverage.
- Review-tightened acceptance: use the contender's real FreeRTOS blocked state
  while the first constructor is active. Workers retain their TCBs until both
  completions and explicit suspension, then the RPC deletes them and its SDK
  semaphores. With the original fallback temporarily restored, LTO produced
  two constructions and failed; restoring the fix produced one construction,
  two completed callers, an observed blocked contender, and safe teardown.
  The non-LTO control passed without cleaning after LTO. Logs and ELFs:
  `.build/evidence/issue-4994/c6-review-{lto-before,lto-after,control-after}.*`.
  New incompatible-mode tests reproduced 18 failures before parser validation;
  all **481 AutoResearch Python checks**, full lint, WASM AutoResearch, and the
  follow-up code review passed. The first hosted full sweep passed 99 workflows
  on `dd2293368`; the final review commit requires its own full CI sweep.
- Published-package hardware acceptance: installed CLI
  `2.5.50+git.9c0dd1bcaa38`, no executable override. Rebuilt and fully flashed
  LTO firmware with `--clean`; both tasks completed and overlapped, exactly one
  construction. The probe object contains LTO IR and no native function code;
  its linked worker calls strong ESP-IDF guard acquire/release functions.
  After another reset, the non-LTO control passed without `--clean`; its probe
  object contains native code and no LTO IR. Both ELFs and logs are retained
  as `.build/evidence/issue-4994/c6-{lto,control}-published.*`.
  Repeated Python validation with the published package: **456 passed**.
- Integrated current `master` (`1c0ac9724`) and preserved both static-init and
  memory-benchmark RPC bindings. Added parser rejection for combining the
  static-init probe with memory benchmarks: four focused tests reproduced the
  silent skip before the fix. Integrated AutoResearch Python validation:
  **456 passed**; full `bash lint` and WASM AutoResearch compilation passed.
  Follow-up repository code review found no blockers. Integration committed
  as `e30276bd3`. Logs:
  `/tmp/fastled-4994-python-merged.log`,
  `/tmp/fastled-4994-lint-merged.log`, and
  `/tmp/fastled-4994-wasm-autoresearch.log`.
- Exact-SHA release dry-run
  <https://github.com/FastLED/fbuild/actions/runs/41556139471> failed both
  Windows signing lanes with Azure `AADSTS7002131` for
  `repo:FastLED/fbuild:ref:refs/heads/main`. Version 2.5.50 was merged but not
  yet published at the time of that failure.
  After user-authorized Azure login, inspected the existing fbuild federated
  credential: Azure already expected the correct immutable owner/repository
  identity. GitHub's repository configuration still had
  `use_immutable_subject=false`. Enabled that repository opt-in and verified
  `use_immutable_subject=true` with prefix
  `repo:FastLED@5899270/fbuild@1010805115`. No Azure settings or secrets changed.
  Retry attempt 2: Windows ARM64 (`125714763129`) and x64 (`125714763290`)
  release jobs completed successfully, including Azure login and Authenticode
  signing. The complete exact-SHA dry-run passed, including signed-binary and
  wheel smoke tests. CI-only publication for that same candidate succeeded at
  <https://github.com/FastLED/fbuild/actions/runs/41577477949>, dispatched from
  the already-trusted non-version tag `release-candidate-2.5.50-9c0dd1bc`.
  All five PyPI wheels and the release tag's exact candidate SHA were verified.
  FastLED's minimum is now `fbuild>=2.5.50`; local sync installed that release
  and the CLI reports `2.5.50+git.9c0dd1bcaa38`.
- Dependency fixes and version 2.5.50 merged through the required queue in
  <https://github.com/FastLED/fbuild/pull/1865>, commit
  `9c0dd1bcaa38de3ac6d67c86cca202a0b4677e6c`. Local unit, formatting, guard,
  independent review, canonical workspace Clippy, and hosted checks passed.
- C6 hardware LTO RED: original FastLED fallback, patched local fbuild backend,
  GCC 14.2 / Arduino 3.3.5 / IDF 5.5.1. Both callers completed and overlapped,
  but `constructions=2, success=false`. Log and ELF saved as
  `.build/evidence/issue-4994/c6-lto-before.*`. Actual linker process used
  `liblto_plugin.so`, `lto-wrapper`, and `-flto=auto` (recorded process args).
- C6 hardware LTO GREEN: same toolchain, probe, and board after removing the
  ESP32 macro. `success=true, completed=2, contenders=2, overlapped=true,
  constructions=1, constructorEntered=true`. Log/ELF/symbols/worker disassembly:
  `.build/evidence/issue-4994/c6-lto-after.*`.
- Fixed LTO ELF has strong `__cxa_guard_acquire/release` symbols and actual
  calls from `staticInitWorker`; pre-fix ELF has only `__cxa_guard_dummy` and
  the worker's two plain guard-byte loads.
- Post-fix non-LTO control passed without `--clean` after the LTO run. Both
  tasks completed and overlapped, with one construction. This also exercises
  the corrected local framework-cache identity on the LTO -> non-LTO switch.
- `bash compile wasm --examples Blink` passed.
- `bash test --cpp` reached its 600-second compilation deadline while linking
  target 578/915; no test case had run or failed. Immediately reran
  `bash test --cpp --debug` (sanitizers and 1800-second build deadline).
- Full native debug validation completed successfully after retaining build
  artifacts across the compilation deadline/cache transport interruption:
  **329/329 unit-test groups and 95/95 examples passed**, zero failures.
  All 424 groups executed under the sanitizer-enabled debug configuration.
  Log: `.build/evidence/issue-4994/cpp-debug-passed.log`.
- `bash lint` passed. Final repository code review found no blocking issues;
  the duplicate staging-parameter documentation it noted was removed.
- Port scan and firmware readiness identified ESP32-C6 at `/dev/ttyACM0`.
- `PYTEST_ADDOPTS='-k autoresearch --junitxml=/tmp/fastled-4994-python-results.xml' bash test --py --debug`:
  368 passed, including 15 static-init checks; zero errors, failures, or skips.
- Pre-fix LTO build exposed fbuild's failure to remove SDK `-fno-lto` before
  linking: tracked at <https://github.com/FastLED/fbuild/issues/1862>.
  This build failure is not hardware RED evidence.
- Switching to the non-LTO control exposed stale local framework archives;
  tracked at <https://github.com/FastLED/fbuild/issues/1863>. The supported
  `--clean` recovery produced the passing control below.
- Dependency unit RED: SDK flag regression 1 failed / 1 passed; local archive
  cache regressions 3 failed / 1 passed. Logs retained under
  `.build/evidence/issue-4994/fbuild-186{2,3}-unit-red.log`.
- Dependency unit GREEN: 209 ESP32/ESP8266 tests passed, zero failed, one
  ignored; both new regressions included. Log:
  `.build/evidence/issue-4994/fbuild-1862-1863-green.log`.
- Pre-fix non-LTO control: `bash autoresearch esp32c6 --static-init --static-init-no-lto --upload-port /dev/ttyACM0 --timeout 120s --skip-lint --clean`
  passed on the C6: `success=true, completed=2, contenders=2, overlapped=true,
  constructions=1, constructorEntered=true`. ELF has strong (`T`)
  `__cxa_guard_acquire` and `__cxa_guard_release` symbols. Log and ELF retained
  under `.build/evidence/issue-4994/c6-control-before.*`.
