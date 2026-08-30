# Task Tracker

<!-- Add tasks here as checkable items -->

## Preserve FastLED imports for external Meson consumers

- [x] Reproduce the foreign-working-directory `ModuleNotFoundError` in CI.
- [x] Add a RED regression for the host Python command environment.
- [x] Prepend the FastLED project root to `PYTHONPATH` for Meson helpers.
- [x] Pass focused tests, full lint, and a cross-checkout Blink WASM compile.

### Review

- RED: the new environment regression failed and fastled-wasm DWARF smoke could
  not import `ci.meson.cache_utils` from its checkout working directory.
- GREEN: 3 focused tests pass, full lint passes, and fastled-wasm compiles Blink
  successfully against this checkout from outside the FastLED project root.

## Normalize Meson host Python selection

- [x] Reproduce the macOS failure caused by Meson invoking bare `python`.
- [x] Add a RED source-contract test for Windows versus macOS/Linux selection.
- [x] Define one build-machine Python program and reuse it in Meson helpers.
- [x] Run the focused regression test and Meson lint checks.
- [x] Validate the WASM Blink compile through fastled-wasm's Tauri test mode.

### Review

- RED: `ci/tests/test_meson_python_selection.py` failed both assertions before
  the normalized build-machine program was introduced.
- GREEN: the focused regression test passes (2 tests), and the dependent
  macOS Tauri run finds `python3`, compiles Blink, starts the pthread worker,
  and captures a non-black WebGL frame.
- Review: selection is intentionally based on `build_machine`, because these
  Python helpers execute during configuration even for an Emscripten cross-build.

## minimp3 Phase 0 (#4051)

- [x] Capture the missing minimp3 backend as a focused RED test.
- [x] Vendor pinned CC0 minimp3 with provenance and a caller-owned scratch patch.
- [x] Add the Helix-default dual-backend adapter without changing the public API.
- [x] Add deterministic host golden coverage for both backends.
- [x] Run focused debug/quick tests, lint, broader tests, and compile gates.
- [x] Run the pre-push code-review gate.
- [x] Push, merge the closing PR, and verify issue #4051/G0 state.

### Review

- RED: the focused codec test failed because
  `third_party/minimp3/minimp3.h` did not exist; the required debug rerun
  reproduced the missing-backend failure.
- GREEN: the golden corpus and scratch API pass; the full native gate passed
  283 unit tests and 84 host examples; standard lint and IWYU pass; explicit
  minimp3 builds pass on AVR and WASM, with the WASM compile database proving
  the selector define reached every library TU; default-Helix AVR and WASM
  also pass. A selected-minimp3 debug host run covers the public stream path.
- The broad Python gate passed 1044 tests and skipped 34; its lone ESP32 QEMU
  smoke failure was an orphaned fbuild daemon. After scoped daemon recovery,
  the focused QEMU test built and emulated successfully in 481 seconds, and
  the complete Python gate then passed cleanly.
- The pre-push review is clean after correcting bitrate units/free-format
  metadata, corrupt-tail stream progress, and full WASM selector cache
  isolation (including stale-build recovery).
- Strict-only Pyright remains non-green on current master with 916 unrelated
  baseline errors; all diagnostics introduced by this diff were corrected.
- PR #4057 merged as `30e763437`; issue #4051 closed automatically and the
  parent tracker reports G0 complete (1/6 children).

## minimp3 Phase 1 (#4052)

- [x] Capture RED evidence for the missing codec memory ledger/instrumentation.
- [x] Add tagged allocation accounting for decoder state, scratch, and stream buffer.
- [x] Cross-check Linux heap measurements with heaptrack or Valgrind Massif.
- [x] Add decode stack watermarking plus `-fstack-usage`, frame-size, and call-graph analysis.
- [x] Enforce the 2 KiB decode-stack and 24 KiB working-RAM budgets.
- [x] Measure and itemize codec static/flash symbols with `nm` and `size`.
- [x] Add a machine-parsed `codec_memory_ledger.md` with a 2% regression gate.
- [x] Run focused/broad validation and the pre-push code-review gate.
- [x] Push, merge the closing PR, and verify issue #4052/G1 state.

## MP3 Phase 2 CPU profiling audit (#4053)

- [x] Capture the focused RED signal for missing CPU audit/trend/codegen ledgers.
- [x] Instrument exact multiply/MAC counts per decoder stage for both backends.
- [x] Add N=30 host counters, attribution, and per-stage median timing reports.
- [x] Enforce `codec_cpu_trend.json` with a 5% regression gate.
- [x] Record `-Os` inner-loop codegen for Xtensa, RISC-V, M0+, and M4.
- [x] Run focused and broad validation plus the pre-push review gate.
- [ ] Push, merge the closing PR, and verify issue #4053/G2 state.

### Review

- RED: the focused test failed at collection because `ci/codec_cpu/audit.py`
  and `codec_cpu_trend.json` did not exist.
- Tier 1 now instruments optimized decoder LLVM IR and records exact totals
  plus mechanically derived per-frame multiply/MAC counts for all eight
  stages across the same 892-frame, five-file corpus for both backends.
- Tier 3 cross-compiles the complete codec build translation units at `-Os`
  and records whole-kernel and real loop-body instruction counts
  for Xtensa ESP32, RISC-V ESP32, Cortex-M0+, and Cortex-M4.
- Stage coverage is fail-closed: every dedicated stage requires static
  instrumentation and a positive timing, while the two codec-specific fused
  stages are declared explicitly. Multiply/MAC accounting uses one MAC per
  accumulation, including Helix sums of `MULSHIFT32` products.
- Host trends are keyed by CPU model, compiler, and governor so measurements
  from unlike hosted runners cannot be compared; Callgrind requires at least
  eight attributed codec functions and enforces normalized function shares.
- Focused validation passes 24 tests, operation and four-target codegen audits
  pass locally, lint is clean, and the repeated pre-push review is clean after
  resolving all five initial findings plus two follow-up accounting findings.
- The first native runs exposed compiler-dependent direct-use MAC attribution
  and unavailable PMU hardware counters. The audit follows multiply results
  through LLVM casts and spill slots to their consuming accumulations; Clang
  14 Linux and Clang 21 Windows produce identical operation sites and exact
  ledgers across the full corpus.
- CodeRabbit's first pass is addressed: Callgrind event/value cardinality is
  strict, fixture allocation/read failures clean up, unattributed operations
  abort, the audit-only macro follows the `FL_` rule, and target codegen now
  uses production inlining/contraction flags. Narrow callers retain minimp3's
  otherwise fully inlined kernels without changing optimization inside them.
- The N=30 stage timer and Callgrind attribution tiers are locally green.
  GitHub's hosted Azure VM reports all `perf stat` PMU events as unsupported,
  so the fail-closed host ledger records N=30 pinned Clang cycle-counter
  medians plus deterministic Callgrind instructions and simulated branch
  misses over the same decode regions, with explicit provenance and IPC
  derived from those two sources. Schema validation recomputes every identity.
- Native Ubuntu run 32927438193 captured the authoritative AMD EPYC 9V74 /
  Clang 18 baseline: Helix median 147,513,314 cycles, 461,845,281 Callgrind
  instructions, and 453,948 simulated branch misses; minimp3-float median
  54,515,955 cycles, 261,532,799 instructions, and 555,920 branch misses.
- Follow-up run 32927721702 proved the same `ubuntu-24.04` label rotates among
  CPU models and captured the AMD EPYC 7763 profile. The trend selects a
  distinct checked-in host baseline by CPU/compiler/governor key.
- Run 32928148824 captured the third observed pool profile, Intel Xeon Platinum
  8573C with the `performance` governor exposed.
- Run 32928388694 captured Intel Xeon 6973P-C, the fourth observed pool model.
  GitHub documents only the standard runner's core count and architecture, not
  its CPU model. Every environment remains fail-closed: matching profiles gate
  all trends at 5%, while unknown models upload evidence and fail for explicit
  baseline onboarding.
- Broad Python validation passed 1,070 tests, skipped 35, and passed 40
  subtests; its sole cold-build QEMU timeout passed on the required immediate
  focused rerun in 12m56s.

### Review

- RED baseline: issue #4052 is open and the repository has no codec memory
  ledger, tagged codec accounting, stack-usage parser, or MP3 budget gate.
- GREEN: the production profile reports 27,952-byte Helix and 23,180-byte
  minimp3-float working RAM (persistent state, scratch, and stream staging).
  Minimp3 is 1,396 bytes below the explicit 24 KiB target; pipeline peaks with
  caller PCM are 32,560 and 27,788 bytes. The upstream 2,304-byte free-format
  cap and 4,096-byte stream lookahead remain intact by reusing enlarged
  persistent QMF state as synthesis and reorder workspace.
- Optimized LLVM IR plus Clang `.su` files derive 552-byte and 2,032-byte
  worst decoder paths and fail closed for missing roots or reachable frames.
  Live stack-pointer watermarks conservatively observe 1,736 and 2,056 bytes,
  including the measurement boundary and its 256-byte safety guard.
- ELF `llvm-nm` inventories every named table: Helix totals 12,744 bytes and
  minimp3-float 7,878 bytes. Separate Valgrind Massif processes dynamically
  attribute 32,560-byte Helix and 27,788-byte minimp3 peaks, each exactly
  matching its production allocation hook.
- The audit gate passes on managed Ubuntu 24.04 and requires the real profile
  binary. Allocation shape is exact, footprint growth is capped at 2%, and
  parser tests cover deeper callgraphs, missing frames, Massif aggregation,
  working-RAM enforcement, and unledgered tables. Default quick and selected-
  minimp3 sanitizer codec tests pass, including 1,200-byte free-format public
  streaming and allocation-failure cleanup; the broad Python suite and full
  lint are green.
- The final pre-push review is clean after preserving Phase 0 free-format
  lookahead, isolating Massif by backend, making selector tests portable,
  covering every partial OOM path, and adding Layer I synthesis parity.
- The CodeRabbit follow-up is locally resolved: fork PRs run the gate,
  stack-symbol parsing handles drive letters and C++ names, Valgrind fails
  closed, persistent Helix state is tagged accurately, and memory-tag hooks
  are bounds-checked and balance every bucket. Focused Python/C++ tests, the
  release profile, lint, WASM, Uno, and the repeated pre-push review are green.
- PR #4058 merged as `b0969cbec`; issue #4052 closed automatically and the
  Phase 2 branch was rebased onto the verified merge.

## Frame-task lifecycle (#3896)

- [x] Reproduce the missing production dispatch and contradictory one-shot semantics.
- [x] Compare repair, removal, and contract-narrowing strategies.
- [x] Add failing lifecycle tests for automatic before/after dispatch and recurrence.
- [x] Implement the selected engine-event integration and recurring semantics.
- [x] Correct the frame-task and executor documentation.
- [x] Run focused tests, lint, the C++ suite, and the pre-push review gate.
- [ ] Push a PR, drive checks/reviews to green, merge it, and verify master.

### Review

- Selected automatic recurring frame tasks over removal (source-breaking) and
  one-shot contract narrowing (inconsistent with the documented per-frame API).
- Frame callbacks now use a lazy low-memory-safe engine hook, stable per-phase
  snapshots, same-phase reentrancy guards, cancellation cleanup, and deferred
  registration semantics.
- RED reproduced missing dispatch in quick and sanitizer builds. GREEN evidence:
  focused quick + sanitizer tests pass; all 279 C++ tests and 84 host examples
  pass; full lint passes; the pre-push review is clean after fixing its scheduler
  mutation finding.

## QEMU build badges

- [x] Inventory the ESP32-DEV, ESP32-C3, and ESP32-S3 badge failures from current GitHub Actions logs.
- [x] Reproduce every distinct failure from current logs and the legacy local entrypoint.
- [x] Fix the underlying workflow/build causes without weakening validation.
- [x] Run focused QEMU validation, lint, C++ tests, and code review.
- [ ] Push one PR, drive its checks/reviews to green, merge it, and verify all three badges on current master.

### Review

- Root cause: the retired `ci-compile --merged-bin` step expected fbuild
  artifacts in a legacy environment-nested directory and failed before QEMU.
- Replaced the reusable job with source-only staging plus native
  `fbuild test-emu`; removed Docker QEMU and manual flash-image plumbing.
- All five legs now require explicit runtime assertions: BlinkParallel proves
  four channels registered, Test proves loop execution, and the S3 LCD leg
  proves the real LCD_CLOCKLESS driver linked and registered. Transmission is
  intentionally left to HIL because Espressif QEMU lacks those interrupts.
- Local evidence: native ESP32-DEV fbuild/QEMU exit 0; the latest focused run
  passed 8 tests with one daemon-owned smoke test deselected; all 362 C++
  tests/examples passed; actionlint, full lint, staged-manifest verification,
  and the final two-cycle pre-push review are green. The broad Python run
  passed 766 tests, skipped 34, and passed 39 subtests; its sole failure is an
  unchanged current-master AutoResearch board-list expectation. The unrelated
  MinGW Renesas guard remains excluded, and another worktree owns the shared
  fbuild daemon, so the isolated PR jobs are the acceptance evidence.

## WASM gfx electrical-group update

- [x] Verify the `gfx-v0.1.1` release tarball is available.
- [x] Update the compiler's strict gfx tarball pin and lockfile.
- [x] Run compiler typecheck and production build.
- [x] Review and push the FastLED PR.

## HydroPack LED audio prototype

- [x] Replace the EL geometry preview with a normal LED < | > screenmap.
- [x] Add independent sensitive and loud adaptive-audio indicators.
- [x] Compile the WASM example and launch its local preview.
- [x] Review and push the FastLED PR.
- [x] Gate HydroPack launches on calibrated SPL and stable musical tempo.

## SAMD51 unused Arduino I2S compile regression (#4030)

- [x] Capture an unmasked SAMD51 RED build from current master.
- [x] Add focused source-selection regression coverage.
- [x] Exclude incompatible generic Arduino I2S on SAMD51 in source.
- [x] Remove SAMD51-only CI `I2S` masks.
- [x] Run focused tests, lint, the C++ suite, and all three SAMD51 board builds.
- [x] Run the pre-push review gate.
- [ ] Push a PR, drive checks/reviews green, merge, and verify master/issue state.

### Review

- RED: unmasked Metro M4 Blink/Apa102 both failed while preprocessing
  `fl.audio+.cpp`, with SAMD51's `I2S` register macro expanded as a header name.
- GREEN: the poisoned-header guard test passes; Metro M4, Feather M4, and
  Grand Central M4 compile Blink/Apa102 without masks; SAMD21, Uno, and ESP32
  Blink compile; full lint passes; all 284 native unit tests and 84 host
  examples pass.
- The full Python suite improved from 1037 to 1038 passes after the source fix;
  its two remaining failures are unchanged WASM-path and QEMU-fixture failures
  unrelated to this diff. The focused I2S guard selection passes independently.
- The one-agent pre-push review is clean after improving failed-preprocessor
  diagnostics with platform and exit-code context.
