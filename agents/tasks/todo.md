# Task Tracker

<!-- Add tasks here as checkable items -->

## Memory meta #4737: sequential local optimization

- [x] Complete all ten native platform children: C3 #4739, C6 #4740, STM32 #4744, Uno #4742, ATtiny85 #4743, ESP32 #4738, S3 #4741, Teensy40 #4745, Teensy41 #4746, RP2040 #4747.
- [x] Land reductions and existing-budget ratchets through PRs #4748–#4757, with matched 3.10.3/master fbuild bloat, published examples and controlled RGB evidence.
- [x] Post a cumulative parent performance diff on each child closure; all ten comments verified.
- [x] Run the final eleven-row local matrix sequentially at `41a6c284fcbbc8c6cb0a379bd994b98d017f78d1`. All budgets pass; every row verifies 2,394 tracked staged source files and its staged sketch against that commit.
- [x] Publish the final three-version scorecard, matched savings, remaining gaps, source pins and metric limits: https://github.com/FastLED/FastLED/issues/4737#issuecomment-6041460246.
- [x] Publish the parent acceptance audit and reconcile this tracker; keep #4737 open. Audit: https://github.com/FastLED/FastLED/issues/4737#issuecomment-6041492984.

### Results and execution

Eleven published rows save 2,868 B flash and 39,422 B physical RAM versus
matched implementation anchors. RP2040 uses allocated image flash; other rows
retain their board flash metric. This sum spans separate targets. Historical
Teensy gains and frozen-to-matched baseline differences are excluded. Remaining
3.10.3 gaps are explicit in the scorecard; exact parity is not claimed.

RP2040 saves 76 B allocated image and 38,416 B physical RAM while preserving
explicit RX capacity and SPI pin routing. GNU/fbuild raw flash includes reserved
NOBITS heap/stack: final 388,476 raw versus 135,744 allocated image bytes. The
scoped budget guards the allocated image and attributed RAM.

Work used this checkout without git worktrees. Platform builds/tests ran locally
and sequentially, never CI Full. Caches were retained; stale artifacts were
rejected with source/flags checks and focused rebuilds. Logging, RMT/PARLIO,
controller/settings storage and unused initialization were audited and trimmed.
No fl::printf calls remain in ESP32/RP/Teensy4 platform trees; failure warnings
and some legacy FL_WARN diagnostics remain. Features were retained. Strict native
validation passed 417/417 unit/example jobs after the final source changes.
Host/SDK/QEMU evidence does not claim physical waveforms; SPI SDK initialization
timed out on both versions while direct ownership accounting passed.

## ESP32 flash and RAM regression (#4707)

- [x] Reopen the issue and record controlled 3.10.3 versus master evidence.
- [x] Remove unnecessary subsystem linkage from LED-only execution paths.
- [x] Reduce RMT queue/state and channel dispatch overhead without limiting strips or runtime configuration.
- [x] Measure identical Blink and CD77 gist builds with one toolchain before and after.
- [x] Run focused behavior tests, C++/lint gates, and code review.
- [x] Publish a feature PR and update the issue with measured savings and remaining costs.

### Review

- Controlled ESP32 image flash: Blink 313,767 -> 307,031 B (-6,736 B);
  CD77 gist 306,275 -> 299,943 B (-6,332 B). Static RAM falls 392 B and
  368 B respectively. ESP32-S3 ratchet falls 367,407 -> 359,003 B (-8,404 B).
- The controlled 3.10.3 flash baselines are 287,689 B and 274,921 B;
  the remaining gaps are 19,342 B and 25,022 B. Keep #4707 open.
- Full native gate passes 319 units and 95 examples; focused RMT and executor
  sanitizer runs, lint, and ESP32/ESP32-S3 board builds pass. Physical hardware
  behavior and hosted full CI remain unverified.

### Further reductions after PR #4736

- [x] Measure lazy diagnostic cache linkage: Blink image flash 307,031 ->
  306,683 B, static RAM 25,692 -> 25,628 B with the same configuration.
- [x] Remove unused iterator state and trial hardware-bounded RMT state storage.
- [x] Check encoder allocation failures and remove redundant encoder state.
- [x] Measure the combined default configuration, run behavior gates and review,
  then publish the additional evidence without closing the remaining regression.

- Second-pass flash: Blink 306,119 B, gist 299,015 B, ESP32-S3 357,663 B
  locally. Total savings versus original master are 7,648 B / 7,260 B;
  S3 is 9,744 B below its original ratchet. Blink/gist static RAM is
  25,604 B / 26,388 B. Remaining 3.10.3 flash gaps are 18,430 B / 24,094 B.
- Full native run passes 414/414; the final isolated production allocator TU
  passes its sanitizer run. Its native tests now execute real TX/RX allocation,
  fallback, exhaustion and error paths; earlier native cases were excluded.
- Hosted first-pass image is 32 B above local. Pin the final ratchet to hosted
  evidence when available. Second-pass hosted run 37581805753 measures
  357,679 B, 16 B above local; the ratchet now uses that exact result.
  ATmega8 RGBW overflows reproduce unchanged at base.
- Rejected native TLS guard: approximately 150 B flash savings would add
  16 B of task-stack use to every task in the current zero-TLS image.

### Plan

Third pass:
- [x] Trial unified TX/RX accounting without changing global/shared pool limits.
- [x] Exercise real global-pool allocation, reservations, rollback and reset natively.
- [x] Validate retained test coverage and publish the measurements (corrected below).

Unified accounting saved 8 B static RAM and default image flash:
Blink 306,119 -> 306,055 B (-64 B), S3 357,663 -> 357,591 B (-72 B).
Initially rejected by mistakenly comparing the CLI Firmware flash line with
the saved JSON image_flash baseline; reintegrating the measured reduction.
Corrected hosted gate 37582254539 passed completely at cb41f69ebd.
Retained test passes against the unchanged production allocator with sanitizers;
`bash test --cpp` reruns its changed unit successfully (other units/examples
use their prior passing fingerprints). C++ lint and code review pass.

Fourth pass:
- [x] Trial production constant-clock encoder specialization:
  Blink 306,091 B (-28 B), S3 357,639 B (-24 B). Reintegrate after correcting
  the same mixed-metric error.
- [x] Remove unnecessary locale initialization roots from scoped locks and
  isolate unused ESP32 condition-variable code without changing synchronization.
- [x] Measure matching default images, validate behavior/build routing, review,
  and publish the retained changes and remaining regression.

Fourth-pass local image_flash: Blink 305,127 B, gist 298,023 B, S3 356,675 B.
Blink/gist total master savings 8,640 B / 8,252 B; remaining 3.10.3 gaps
17,438 B / 23,102 B. Static RAM drops another 328 B to 25,276 B / 26,060 B.
Four locale initializers (816 B attributed) disappear after all scoped interner/UI
guards use lock_guard and ESP32 condition-variable code has its own unity entry.
An explicit ESP32 condition-variable wait sketch still links; the implementation
move is byte-for-byte unchanged. Global interner, UI and allocator sanitizer
tests pass. Hosted ratchet tightening follows the measured final hosted image.
Full forced native run passes 319/319 units and 95/95 examples in 147.33 s;
current-tree C++ lint and the 18-file code review pass.
Hosted run 37584376133 measures image_flash 356,691 B, 16 B above local,
and static RAM 77,392 B. Pin this exact value; its first gate failed only
because the additional 988 B saving had not yet been claimed in the ratchet.
Full Python rerun passes 1,750 tests after official WASM clean regeneration
repairs a stale include-path cache; WASM Blink also builds. Corrected hosted
gate 37584819636 passed completely at 0c3a21114e. Both prior review threads
are resolved. Full platform CI remains a separate gate before merging.

Next candidate: private EngineEvents listener inline capacity 16 -> 4 would
save 96 B singleton RAM and 96 B per snapshot stack frame on ESP32 while
preserving unlimited spillover. Investigate allocation/per-frame costs and
cover more than 16 listeners, priority and snapshot mutation before retaining.

Use the default dynamic driver configuration. Keep scheduler pumping, network
yielding, multiple strips, and reconfiguration available. Optimize symbol reachability
and redundant state first, then use actual board images to decide further changes.
The original reporter's build and the controlled fbuild comparison are separate
measurements; only compare matching examples, flags, and frameworks.

## ESP32-S3 binary-size Batch 3 (#2856)

- [x] Audit every tracking item against current master and merged PR history.
- [ ] Resolve item 3.6 with a focused fixed-capacity ledger PR and regression tests.
- [x] Include same-configuration before/after firmware and per-symbol bloat evidence.
- [ ] Run focused tests, full lint/C++ gates, and the pre-push review gate.
- [ ] Push, wait for required CI/review, merge, and rebase onto `origin/master`.
- [ ] Resolve item 3.1 in a separately validated, measured PR.
- [ ] Close #2856 only after all remaining acceptance criteria are proven.

### Plan

- Start with 3.6 because it is the bounded remaining change. Static mode's
  contract permits exactly one FastLED TX strip and no RMT5 RX allocation, so
  the fixed ledger holds one record; dynamic mode keeps the full TX/RX ledger.
  Check `vector_fixed::insert` capacity before accounting changes and add
  static-mode regressions. Compare ESP32-S3 Blink with
  `FL_RMT_STATIC_ALLOCATION=1` at identical revisions/configuration; retain
  the legacy `FASTLED_RMT_STATIC_ALLOCATION` alias in compatibility coverage.
- Treat 3.1 as a separate architectural slice after 3.6 is merged and the
  checkout has been rebased onto `origin/master`.

## Meta issue #4528: ten verified bug fixes

- [x] Audio slice: resolve #4518-#4521 with focused RED→GREEN regressions.
- [x] STL slice: resolve #4522-#4524 with focused RED→GREEN regressions.
- [x] CI/WASM slice: resolve #4525-#4527 with focused RED→GREEN regressions.
- [x] Integrate and inspect all worker diffs in deterministic slice order.
- [x] Run focused checks, full lint, and full Python/C++ test gates.
- [x] Run the pre-push review gate and address findings.
- [ ] Push a closing PR, wait for GitHub Actions and review threads, then merge.
- [ ] Confirm #4518-#4528 closed, remove owned worktrees/branches, and sync clean master.

### Plan

- Each slice owns disjoint production and test files and starts from the same
  `origin/master` commit (`5cce0fb6f9`). Workers run lint only under the
  orchestrated carve-out; the orchestrator records focused RED→GREEN evidence
  and runs the combined gates.
- Integration order is audio, STL, then CI/WASM. Any shared-file conflict is
  resolved in this integration worktree and revalidated before publishing.

### Review

- RED: audio produced non-finite/stale analysis and dropped `INT16_MIN`; flat
  containers skipped erase successors; the span constructor dereferenced an
  empty iterator; vector allocation failure hung on an assertion/underflow;
  all three CI/WASM checks falsely reported success.
- GREEN: focused sanitizer tests pass for audio, flat-map, flat-set, span, and
  vector; the full Python gate passes; lint passes; and a clean combined gate
  passes 311/311 C++ unit tests plus 95/95 host examples.
- The vector fix preserves the successfully appended prefix on allocation
  failure and keeps allocation failure recoverable in debug and release modes.
- The review follow-ups preserve empty pointer identity across cv conversion,
  keep generic empty iterators non-dereferencing, and report reversed pointer
  ranges through `FL_ERROR_F` before returning a safe zero-length span.
- Pre-push review is clean after three cycles; the final combined native gate
  passes 311/311 unit tests and 95/95 host examples after rebuilding 867
  targets affected by the public span header.
- Upstream fingerprint false-green behavior discovered during validation is
  tracked as zackees/zccache#1650.

## Profiled color pipeline (#4032 / #4034)

- [x] Revalidate current master, phase issues, and #4156 review decisions.
- [x] Assign P1, P2, and P4 to Terra agents in isolated checkouts.
- [ ] Merge the [cross-phase contract addendum](../../docs/color-pipeline-contracts.md) and link it from the tracker.
- [ ] P1 #4035: complete research/schema artifacts and merge its PR.
- [ ] P2 #4036: complete profile/binding API and merge its PR.
- [ ] P3 #4037: complete generator/ingestion/freshness tooling and merge its PR.
- [ ] P4 #4038: complete media contract and playback integration and merge its PR.
- [ ] P5 #4039: complete float64 reference/goldens/RED baseline and merge its PR.
- [ ] P6 #4040: complete streaming core/brightness/power and merge its PR.
- [ ] P7 #4041: complete gamut/device solve and merge its PR.
- [ ] P8 #4042: complete native encoders/dithering and merge its PR.
- [ ] P9 #4043: prove fixed-point/TINY/bloat/throughput gates and merge its PR.
- [ ] P10 #4044: complete measured hardware/compatibility gates and merge its PR.
- [ ] Audit every criterion, clean owned worktrees/artifacts, return to origin/master.

### Evidence and constraints

- Initial checkout and remote default are master at 9de01d3547; origin/main
  does not exist. No pre-existing worktree changes were present.
- P4's carry/validate layer landed in #4045; that does not satisfy playback.
- Current port scan reports one healthy Espressif USB device at /dev/ttyACM0.
  Calibration instrument and strip/measurement provenance still need validation.
- Full requirement ownership remains #4034; implementation contracts are in
  docs/color-pipeline-contracts.md. No phase is complete merely because an API
  or test stub exists.

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

## Native example CI split and speed (#4544)

- [x] Investigate current example discovery, workflow timing, badge routing, and related issues; file and read back #4544.
- [x] Add an exhaustive six-group path manifest with RED -> GREEN membership tests.
- [x] Add one highly amalgamated live Compile Tests target with a focused failing contract repro.
- [x] Route PR example workflows to the live gate and add the nightly six-group sweep and badge.
- [x] Remove redundant cold Blink setup and 114-sketch Meson discovery from the live gate; measure local cold/warm behavior.
- [x] Remove unused OIDC grants from the affected reusable workflows and callers.
- [x] Address the two security review comments after pushing the fix.
- [ ] Run focused tests, full repository gates, review, and both live/nightly CI paths; merge only after validated.

### Review

- Baseline Linux run 35839651828: 4m43s total; Install 27s, cold Blink 3m00s, all examples 52s.
- RED: focused group test failed at collection without the six-group manifest; compile-gate contract failed without the target. GREEN: focused Python and workflow-routing tests pass.
- Live `debug-thin` gate configured just four Meson targets with no full-sketch discovery; cold local build/run took 92s and warm rerun 4–11s. It includes the real Blink sketch plus representative color/noise/palette/controller API calls.
- Nightly quick selection passed all 95 host-compatible examples in 27s locally; the manifest covers all 114 sketches, including 19 deliberately filtered for host. The board-only AutoResearch group now has an explicit nightly ESP32-S3 job; the host CLI rejects selecting it directly with a board-build hint.
- Hosted Linux live gate completed in 3m16s versus the 4m43s baseline (31% faster), with an 80,944,978-byte build directory. Linux, macOS, and Windows live example gates passed on PR #4545; final unit/board checks were canceled and remain unverified.
- A later full Python run exposed a pre-existing stale Q16 symbol test and reachable software-float operations in RGBW bind-time slack. The symbol guard and Q16 slack are now fixed: the focused ARM tests, full Python suite, clean C++ suite (310/310 unit cases and 95/95 host examples), lint, and three-bucket review pass locally. The pre-existing ATtiny TwinkleFox workflow startup permission mismatch is fixed in the same pending push.
- Clean nonverbose Nightly exposed zccache restoring `example_runner` without execute permission and a later implicit Meson rebuild undoing the repair. Building the runner explicitly and using `meson test --no-rebuild` fixed the RED repro; clean Nightly, Blink full, and Compile Tests now pass, as do the 315-second full Python suite, lint, and same-reviewer follow-up.
- Nightly/full-sanitizer reusable-workflow caller permissions now match their callees; this also fixes a likely startup failure analogous to ATtiny.
- The OIDC grants copied into those callers and their three reusable templates had no consumer (artifact/cache upload does not use OIDC). After maintainer review, removed them from both sides and the ATtiny caller together; focused RED -> GREEN permission regression, YAML parse, lint, and one-reviewer security follow-up pass. Full Python and hosted CI are being rerun.
- PR merge and a hosted nightly six-group run remain pending.

## Native linker measurement (LLD, mold, Wild, zackees/wild, reld)

- [x] Freeze one Linux debug-thin unit/example object corpus and record the exact compiler, linker, OS, CPU, and linker source revisions.
- [x] Build a reproducible, read-only-to-the-build-tree link replay that tests the core shared library, representative small modules, and the complete 311-module unit-link workload; add RED -> GREEN focused tests.
- [x] Acquire pinned candidate linkers and prove which engine actually linked each output, especially reld native versus its LLD bridge.
- [ ] Run alternating, repeated same-corpus measurements with link-only and whole-build wall time, output size, peak memory, and correctness checks; report failures rather than dropping candidates.
- [ ] Compare installation cost and target support; install upstream Wild 0.10.0 only for Linux native unit/example jobs and preserve existing macOS/Windows linking. Embedded cross-compilers remain out of scope.
- [ ] Review the evidence, run repository gates, publish a recommendation, and land only a measured improvement through a checked PR.

### Review

- Starting evidence: the hosted Linux debug-thin unit job in run 35911301097 completed 311 test-module link targets between +253.28s and +510.90s, a 257.62s link-only phase in a 17m52s job. The example live gate has one roughly 8s core link in a 3m36s job. These are phase spans, not candidate A/B timings.
- reld currently uses native ELF on Linux but can route unsupported flags through an LLD bridge; `RELD_LOG_ENGINE=1` and output provenance must be captured before labeling a measurement native reld.
- Frozen corpus: FastLED `5cce0fb6f9dd899643decaccae60325160a343f1` on Linux 6.18.48, Ryzen 7 3700X (16 logical CPUs), clang 21.1.5, debug-thin; baseline `bash test --cpp --build-mode debug-thin` passed all 311 units and 95 examples. The 789.44s local build was under heavy concurrent load and is not a controlled A/B timing.
- Pinned candidate binaries: LLD 21.1.8, mold 2.41.0, upstream Wild 0.10.0, zackees/wild at `73516247278bb2cf0bc2f355de78eab9c59d09b0`, and reld v0.2.0. Both release archives passed publisher checksum checks; all candidates linked the FastLED core and one test module in a smoke run. Reld logged native ELF for both smoke links. The smoke timing is compatibility evidence only, not a ranking.
- Added and publisher-checksummed mold 2.42.1 for the full comparison. Clang `-###` proves FastLED's original `-fuse-ld=lld` resolves to bundled LLD 21.1.5, not the separate LLD 21.1.8 used in the first five-way replay; an exact-baseline follow-up is required before choosing. The first replay's core timing is also excluded because a shared scratch directory let later candidates reuse the sanitizer runtime copied by the first candidate. The corrected harness isolates scratch directories per candidate/repetition; unit/example module totals from that replay remain comparable.
- The replay harness has 11 RED→GREEN focused tests and real-graph discovery of 1 core, 311 unit modules, and 95 example modules. It bypasses zccache for link timings, balances candidates per target, records ELF outputs, sizes, versions/SHA, exact command/order, and reld engine routes. All five candidates passed a core-plus-one-unit link smoke test; the full 407-target three-pass comparison is underway.
- First 407-target, three-pass local replay (under heavy unrelated host load): median unit-module link totals were LLD 21.1.8 94.05s, mold 2.42.1 100.77s, upstream Wild 0.10.0 57.17s, zackees/wild 57.11s, reld v0.2.0 58.54s. Median 95-example link totals were 26.33s, 29.83s, 16.43s, 17.19s, and 16.50s respectively. All 1,221 reld links logged native ELF; none bridged. Upstream Wild also produced smaller module outputs (68.89 MB total vs LLD 78.71 MB, reld 79.12 MB). Core timings from this first replay are biased by shared sanitizer sidecars and excluded. Exact LLD 21.1.5 + corrected scratch comparison is underway; these are link-only totals, not end-to-end CI savings.
- Corrected exact-baseline replay (three interleaved repetitions, isolated scratch per candidate): median full 407-link sweep was bundled LLD 21.1.5 18.12s, upstream Wild 0.10.0 12.94s, and reld 0.2.0 12.99s; unit-module totals 13.86s, 9.81s, 9.94s. Wild and reld are effectively tied for time. Reld produced larger output on all 407 targets, +14.17 MB aggregate (+12.5%); filed [zackees/reld#205](https://github.com/zackees/reld/issues/205) with a section-level reproduction, without asserting the root cause. Raw replay bypasses zccache, so this does not establish an end-to-end CI wall-time improvement.
- The first live full build under Wild linked 410 targets but ended 405/406 due to the coroutine semaphore test; isolated debug-thin and full-debug reruns passed, indicating a load-sensitive flake, not proven linker issue. A live reld forced run linked the same 410 targets and passed all 311 units and 95 examples in 373.01s (351.14s reported test-link phase), versus 474.01s for Wild's preceding run. Host load was not controlled, so the difference is suggestive, not a causal CI speedup claim. The top-level fingerprint fix and one-time Meson option migration are covered by focused tests. zccache cold link wrapping remains a large live wall-time component.
- Corrected `--ld-path` replay against the reld-selected Meson graph reproduced the deterministic output-size gap exactly and showed median full 407-link sweeps of bundled LLD 16.60s, reld 11.69s, upstream Wild 11.44s (three repetitions; all 1,221 reld links native). This is a 31.1% raw-link improvement for Wild over the exact bundled LLD; reld versus Wild is effectively a tie. A RED→GREEN test covers invalidating old link outputs before committing the changed linker marker, closing an interruption window found in pre-push review. The same reviewer rechecked the original diff cleanly (one review agent). `bash lint`, the focused suite, and full Python tests passed before the latest pivot.
- User subsequently chose upstream Wild Linux-only because Windows GNU uses special linker/DLL injection and macOS linker behavior is uncertain. The installer targets publisher-checksummed Wild 0.10.0 Linux GNU; macOS and Windows retain their existing linkers. A hosted full Linux debug-thin unit run at `636b0e4` passed 311/311 units but took 746.29s in the test step (659.62s compilation, including 306.40s reported unit linking, and 82.44s execution; 0/346 zccache hits). Its total job was 20m39s including 6m50s Python tests, so it does not show an end-to-end improvement against the earlier different-SHA LLD baseline.
- Investigation corrected an earlier claim that no Python runs per wrapped link: `ci/meson/runner_helpers.py` set `ZCCACHE_LINK_DEPLOY_CMD=clang-tool-chain-libdeploy` on Linux, causing a Python runtime-deployment process after every cache-miss test/example link; `--deploy-dependencies` also reached core and runner links. The user confirmed this deployment is needed only for Windows. A RED→GREEN regression now confines both mechanisms to Windows, while Linux/macOS rely on their linked runtime search paths. The 12-test toolchain suite and local lint pass; a fresh hosted run is still needed to exclude stale local sanitizer sidecars.
- Local same-revision debug-thin 410-link comparison with compiled objects reused: LLD with the old deploy hook took 442.95s wall (419.38s reported test linking); after disabling Unix deployment, Wild took 77.16s (56.96s linking) and LLD took 84.98s (65.21s linking). All three runs passed 311 units plus 95 examples. Thus removing deployment is the dominant improvement; with deployment equally disabled, Wild saved 7.82s (9.2%) wall and 8.25s (12.7%) reported linking versus bundled LLD. A subsequent Wild pass took 20.24s because its link outputs were warm in zccache; it is not a cold comparison. Host load and local cache state limit generalization until hosted validation.
