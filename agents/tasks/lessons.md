# Lessons Learned

- Host-tool selection and host-tool import context are separate concerns. When
  Meson executes repository Python helpers for an external consumer, provide
  the repository root through the command environment so package imports do not
  accidentally depend on the caller's working directory.

<!-- Add lessons from corrections and discoveries here -->

- Before diagnosing missing functionality in a local integration checkout,
  fetch and compare it with its upstream branch; a stale checkout can omit the
  entire subsystem under test. For Meson host tools in a cross-build, branch on
  `build_machine.system()`, never `host_machine.system()`.

- QEMU CI must use fbuild's native `test-emu` runner as the owner of the
  build, flash-image preparation, and emulation lifecycle. Do not repair or
  extend PlatformIO-shaped merged-bin artifact plumbing while PlatformIO is
  being phased out; retain only the minimum example staging fbuild requires.

- QEMU success markers must match the emulator's actual hardware model. For
  unmodeled LED peripherals, assert a real build, boot, and driver/channel
  registration and label it as such; reserve transmit-completion assertions
  for hardware-in-the-loop tests.

- Inspect the actual WASM compiler manifest before assuming an npm dependency: it
  currently pins the `@fastled/gfx` GitHub Release tarball by exact URL.

- For the current HydroPack prototype, prioritize a readable ordinary-LED
  approximation of the < | > layout over further EL-shape fidelity.

- HydroPack's two sensitivity levels are visual layers, not separate status
  dots: use the sensitive analyzer for the center and the loud analyzer for
  the triangles so strong bass appears to launch outward.

- For HydroPack's audience-safe behavior, use FastLED's INMP441-calibrated
  SPL meter with its adaptive bass-beat detector rather than a custom
  sound-floor histogram or a separate tempo lock.

- For a responsive music-only visual, require five seconds of recurring
  qualifying beat evidence plus a low-ZCF check to arm a temporary
  music-present state. Never use a fixed tempo-consistency or animation lock.

- For browser media, derive the 0-1 music confidence from Vibe's normalized
  bass rise above its short-term average. The generic raw spectral-flux beat
  detector can remain below its fixed threshold despite healthy decoded audio.

- A WASM-only per-frame telemetry toggle can expose raw Beat and Tempo
  confidence alongside Vibe levels without adding a physical output pixel to
  the HydroPack fixture.

- When consuming a tool's stderr through `RunningProcess.run`, request
  `stderr=PIPE` explicitly and test the real output format. Default capture
  combines streams and leaves `result.stderr` unset.

- GitHub-hosted Azure Linux VMs can return `<not supported>` for every PMU
  event even after relaxing `perf_event_paranoid`. For a reproducible hosted
  gate, record pinned N=30 compiler cycle-counter medians and pair them with
  explicitly labeled deterministic Callgrind instruction/branch simulation;
  never relabel simulated data as hardware `perf` output.

- LLVM operation ledgers must follow product values through O0 casts and spill
  slots before classifying accumulations; direct SSA-use matching changes
  across compiler versions even when the source operations are identical.

- Target codegen audits must keep production optimization flags. If a static
  kernel is fully inlined and its symbol disappears, retain it with a narrow
  no-inline audit caller and measure the optimized kernel body, rather than
  disabling inlining for the entire translation unit.
- When a regression test mutates a primary metric in a schema with validated
  derived metrics, recompute the derived fields first so the test reaches the
  intended regression gate instead of failing schema validation.
- GitHub-hosted runner labels can rotate among materially different CPU
  models. Host performance gates need separate environment-keyed baselines;
  a single baseline for an OS label either flakes or compares unlike machines.
- A multiply identity that avoids materialising the low half of the product is
  only a win where the low half costs something. On a target whose single
  instruction yields all 64 bits it is pure overhead: applying
  `mp3d_mulshift_k` unguarded cost +7.5% in `L3_dct3_9` and +0.95% across the
  fixed-point decode on the x86-64 audit host, while buying 0.43% on an
  ESP32-C6. Guard such lowerings on register width and let the portable form
  serve everyone else, exactly as `fl::math::mul_shift_round32` already does.
- An `exact operation ledger changed` failure is a question, not a chore.
  Re-baselining it from CI's artifact is only correct once the direction of
  every moved figure has been explained; here the ledger and the per-function
  Callgrind budget were both reporting a real host regression, and a
  re-baseline would have recorded it as the new normal. Read the callgrind
  deltas in the artifact before rewriting the file.
- A harness `PASS` means what the harness checked, not what its name implies.
  `runParallelTest` skips RX loopback validation for the RP `PIO0`+`PIO1` pair
  (`AutoResearchRemoteRunParallelTest.cpp:308`, `is_rp_pio_pair`), so its PASS
  proves channel creation and a clean shared `show()` — not byte-correct output.
  It was quoted as "decisive loopback evidence" on #3899 and had to be retracted.
  Read what a green result asserts before citing it as evidence for a checklist
  item.
- On a HIL bench, most "device faults" are host-side. Four in one session on
  RP2350W: a 20 ms RPC timeout that read as a hung board (`--timeout` is a
  whole-run deadline for `--net-peer`, not a per-phase one); `deployed firmware
  schema does not contain rpSpiLoopback` on a board that exposes it (a failed
  `rpc.discover` collapsed to `None`); `zero_capture` on SPI that was an
  unfitted jumper; and `serial driver may be wedged` that was a dead fbuild
  daemon. Check the transport and the harness before concluding anything about
  silicon.
- A second `RpcBench` on a port another client already holds connects without
  error and is then inert — every call returns `None`, while the first client
  keeps working (FastLED#4207). Because the failure is silent, callers read the
  `None` as a statement about the device. Any code spawning a device script
  against a port the harness still holds must release it first.
- When an error path swallows its own signal, stop reasoning and instrument.
  Four successive hypotheses for one `None` (short timeout, `call_flat` frame
  shape, payload size, then the real cause) were each refuted by the next run,
  because `call_flat` collapses every exception to `None`. A twenty-line
  two-client repro settled it immediately and was available the whole time.
- Verify a fix landed on the hardware you think you flashed. A failed build
  followed by a successful RPC answers from resident firmware, so "after"
  numbers can be byte-identical to "before" and look like a null result. Check
  for a field only the new build emits.
- Exhaustive equivalence checks written in Python cannot see C overflow. A
  32-bit fast path for `cycles_from_ns` compared equal to the 64-bit oracle
  across every sampled input while silently overflowing `u32` above 215 MHz;
  only a separate explicit overflow counter caught it. Assert the width bound,
  not just the values.
- Peer-network autoresearch reflashes the fixture as well as the DUT. The
  RP2350W has a watchdog/bootloader escape armed (#4167/#4172); the ESP32-C6
  has none. A wedged C6 CDC gives `EBUSY` with no holding process, and every
  recovery handle (usbfs, `authorized`, `unbind`, `remove`) is root-only — so
  an unattended bench has no way back. Assess the fixture's recovery path, not
  just the DUT's, before running `--net-peer --ota`.
- Building the non-W `rp2350` target changes the board's USB product string, so
  `/dev/serial/by-id` renames from `..._Pico_2W_<serial>` to `..._Pico_2_<serial>`.
  Anything addressing the board by the old by-id path silently stops resolving
  and presents as a missing board. The serial is stable across both variants,
  which is why fbuild's `SER=<serial>` selector is the robust way to address it.
- A single artifact is not a sample when the property depends on cache-hit
  status. I cleared the "missing exec bit" hypothesis (#4205) after checking
  `tests/runner` alone and finding it executable — it had simply been freshly
  linked rather than served from cache that run. A later survey found 4 of 4
  ELF executables in the build dir lacking `+x`. Enumerate the whole class
  before ruling out a permissions or attribute defect.
- Finding a real bug in the right subsystem is not the same as finding the
  cause. `mtime_stabilizer.py` genuinely matched 0 of 383 outputs on Linux
  (it globbed only `*.dll`), and fixing it was correct — but the #4212 stale
  failures recurred, because the stabilizer compares outputs against *input
  file* mtimes while the #3011 guard compares against *build start*. Different
  comparison, so satisfying one says nothing about the other. Re-run and
  confirm the symptom actually clears before claiming causation.
- When a build system reports an opaque wrapper error, force the underlying
  exception out before theorising. `ERROR: Unhandled python OSError ... return
  code 13` named neither the file nor the errno; `MESON_FORCE_BACKTRACE=1`
  turned it into `PermissionError: [Errno 13] Permission denied: <path>` in
  one step. The "13" was the errno, not an exit code — the message actively
  misdirects.
- Check what a manifest actually is before calling it truncated. `help`
  returning 36 of `rpc.discover`'s 72 methods looked like a response-size cap;
  it is a hand-maintained `kHelpEntries[]` table in
  `AutoResearchRemotePinMethods.cpp`. A new RPC method must be added there too
  or `--rpc-smoke` fails on it.
- On RP, `ClocklessController` acquires PIO/DMA/pin resources in `init()` and
  every `release*` call in `clockless_rp_pio.h` is an error-path rollback.
  Before #4214 there was no destructor, so successful controllers leaked their
  state machine, program space, DMA channel, pin claim, and `dma_buf` — and
  left `dma_chan_waits[]` pointing at a destroyed `mWait` for the shared ISR.
  When testing resource arbitration, always include a control leg that claims
  and then releases; the starved leg alone cannot see a leak.
- Read the command output before writing the sentence that summarises it. Three
  times in one session I asserted a verification result I had not looked at: I
  quoted `_EXIT=` values that were measuring `tail` rather than the command
  (without `pipefail`, `cmd | tail; echo $?` reports the tail's status, not the
  command's; shells with `set -o pipefail` report the pipeline's, so the
  reading depends on shell configuration), and twice wrote "`bash test --cpp`
  is clean"/"still fails" from the previous run's behaviour rather than the run
  just executed — once in each direction. None changed a conclusion, because
  the logs were read afterward, but each put a false claim in a PR that then
  needed a correction comment. Capture the result, read it, then write.
- A test that asserts only an exit code can pass for a completely unrelated
  reason. My `--legacy` chipset-rejection test returned 1 from the
  Teensy/`--use-root-platformio-ini` check and never reached the guard it was
  written for; my `--net-peer` summariser test checked call counts and would
  have passed with the summariser deleted. Assert on the *evidence* — the
  message text, the printed rows — and prove the assertion discriminates by
  temporarily disabling the code under test.
- An exhaustive check is only as good as its input space. `cycles_from_ns()`
  was verified against a 64-bit oracle across 16 clock rates and still shipped
  a bug for clocks that are not whole kHz, because all 16 rates were multiples
  of 1000 and the truncation had nothing to truncate. The same PR's earlier
  overflow bug survived a Python equivalence sweep because bignums never
  overflow. When a check passes, ask what shape of input it structurally
  cannot contain.
- `git pull --rebase` is not a safe refresh for a branch that carries a merge.
  Rebasing #4214 linearised it and silently dropped the #4215 merge commit
  along with two files; only a `FileNotFoundError` on the next edit revealed
  it. Reset to the remote to recover. For stacked or merge-carrying branches,
  merge master in or leave the branch alone.
- Eliminating the obvious candidate does not make the next one proven. I
  reported "transient PCB exhaustion" because `SOF_REUSEADDR` ruled out
  TIME_WAIT, and named `-Os` as the cause of a 12x slowdown because alignment
  and `FL_IRAM` were refuted — both wrong, the second demonstrably so once the
  disassembly was read. State what the evidence supports and name the
  unexplained remainder.
- On a fixture with time-varying flakiness, a cross-build comparison proves
  almost nothing; isolate the change *within one build* instead. Four times on
  the RP2350W peer link a small sample looked like causation: a `stopNet`
  settle delay (1 join -> 4), a raised WiFi join budget (2 master failures vs
  3 clean branch runs), and client-side HTTP deadlines (master 0/2 vs branch
  3/3, with master failing on the exact symptom the fix addressed). Reverting
  only the change in question, on the same branch, produced evidence against
  all three — the runs passed anyway. Two runs per arm is not proof of a
  negative on a link this variable; it is enough to stop crediting the change,
  not enough to rule it out. The same technique caught a genuine regression I had
  introduced (a server-side service-gap credit produced 408s that master never
  showed, across three formulations), so it discriminates in both directions.
  This link's behaviour varies over hours; two or three runs per arm cannot
  see through that.
- After resolving a conflict in a test file, check the **collection** count, not
  the pass count. Resolving a merge in `test_autoresearch_phases.py` left two
  tests nested inside another test function, where pytest never collects them.
  The suite reported `180 passed` before and after the fix — the missing tests
  were absent from the count rather than failing, so the green run and the
  plausible number hid it. `pytest --collect-only -q` showed 178 vs 180, and
  grepping the collected names showed 0 vs 2. A pass count cannot distinguish
  "passing" from "not present"; a collection count can.
- Do not reimplement production logic to investigate a bug caused by
  reimplemented production logic. The AutoResearch UART decoder hardcoded one
  chipset's quantised wire timing, so 400 kHz frames decoded every symbol
  wrongly. To test the fix I derived the timing *by hand*, got 1 of 40 LEDs
  still wrong, called the remainder a "missing final byte" capture defect, and
  spent an iteration hunting it. There was no such defect: my derivation had
  omitted the minimum-symbol-separation bump that `fitUartWave()` applies
  (`kMinSymbolSeparationNs`), so the decode windows were subtly wrong and
  clipped one LED. Calling the real function decoded the frame completely. The
  encoder's own comment had named the hazard — *"Shared by buildWave10Lut()
  and canRepresentTiming() so the LUT that gets built is always judged by the
  same rules that admitted it"* — and I reproduced it by hand while
  investigating it. Before positing a new defect to explain a residual,
  suspect the approximation you introduced to look for it.
- A long hardware campaign and a `git checkout` share one working tree. I
  started a five-run `--net-peer` campaign on the branch carrying the
  `netServerStats` probe, then checked out a different branch in the same
  repository to resolve an unrelated PR conflict. The first run had already
  died on the conflict markers still in `ci/autoresearch/net.py`; the
  remaining runs would have built and deployed firmware from a branch that
  does not contain the probe the campaign existed to exercise. Nothing was
  wedged, but the evidence would have been silently worthless. Background
  device work pins the tree: do concurrent branch work in a `git worktree`,
  and have the loop assert its own branch before each run so a stray checkout
  aborts the campaign instead of quietly changing what it measures.
- Parallel worktrees are not free, and the thing they kill is the unattended
  run. Three `git worktree`s, each with its own `.build` tree and its own
  `fbuild-daemon`, plus a device campaign and a platform compile, drove the
  host out of memory; the harness killed the campaign mid-run. Nothing was
  wedged and both boards re-enumerated healthy, but an hour of unattended
  collection was lost, and the kill looked at first glance like a test
  failure rather than an environmental one. Worktrees are the right answer to
  the shared-tree hazard above, so the fix is not to stop using them: retire
  each one as soon as its work is pushed, kill the daemon that belongs to it,
  and have long unattended loops check free memory before each iteration and
  stop cleanly rather than be killed part-way. Read the exit reason before
  concluding anything about the code. See [[shared-tree-hazard]].
- A tty node path is not a device identity, and the failure it produces lies.
  Four unattended runs died on `Could not open /dev/ttyACM2, the port is busy
  or doesn't exist`, which reads like a wedged board. Nothing was wedged: the
  ESP32-C6 had re-enumerated and the kernel gave it `/dev/ttyACM1` instead.
  I had hardcoded both node paths because `--net-peer` requires them and
  neither `bash autoresearch` nor `fbuild deploy` accepts a USB serial, and
  they had been stable for 241 cycles before they were not. The benign
  outcome is a failed deploy; the dangerous one is two boards swapping
  numbers, which turns a hardcoded path into flashing the wrong board and
  breaks the deploy-isolation property #3832 requires. Resolve the USB serial
  to a node immediately before each use and abort cleanly when the serial is
  absent, rather than deploying to whoever inherited the path. Identity comes
  from something stable, never from enumeration order -- the same rule the
  USB VID/PID registry exists to enforce. See [[worktree-memory-exhaustion]].
- When a whole campaign inverts -- 1-in-6 runs failing became 5-in-6 -- suspect
  the harness before the device. I had just raised an HTTP deadline and was
  ready to read the result as "the peer got worse under the longer budget".
  It was the renumbering above: run 1 was real, runs 3-6 never reached a
  single network cycle because the deploy could not open a port. A result that
  large and that sudden is nearly always the measurement, not the thing
  measured; check how many cycles actually executed before interpreting a
  rate.
- A dual-device campaign builds *two* firmware images, and pre-building only
  one leaves the spike in place. Three unattended campaigns were killed for
  host memory, every one during run 1 of a freshly created worktree. I had
  pre-compiled the RP image each time and concluded the builds were cached;
  `--net-peer` also builds the ESP32-C6 peer image, which took 2m18s from
  cold and was the actual spike. Later runs in every campaign were fine
  because run 1 had warmed the cache. Two consequences: pre-build every
  target a run will touch, not just the one under test; and stop creating a
  worktree per experiment, because each new one pays for two full toolchain
  builds when only a few lines differ -- reuse one bench worktree and change
  what is merged into it. Note a memory guard that samples before each run
  cannot see a spike that happens inside a run; smaller batches limit the
  loss instead. See [[worktree-memory-exhaustion]].
- A board missing from the USB bus is not necessarily wedged. Immediately
  after a killed campaign the RP2350W was absent entirely -- the wedge
  signature -- and it was in BOOTSEL as `2e8a:000f RP2350 Boot`, the normal
  transient while fbuild flashes it, returning as `2e8a:f00f` within about
  four seconds. Checking only for the application VID/PID and stopping there
  would have reported a wedged board and halted the loop for nothing. Scan
  the whole vendor family and wait for re-enumeration before calling a board
  lost.
- BOOTSEL is a transient even when a deploy is killed mid-flight. Twice now a
  campaign died during flashing and left the RP2350W showing `2e8a:000f
  RP2350 Boot` with no application device on the bus -- the wedge signature,
  and the second time the deploy had genuinely been interrupted rather than
  merely observed at the wrong moment. Both times the board re-enumerated as
  `2e8a:f00f` within a few seconds on its own, because the bootrom is what
  runs there and nothing had corrupted it. Wait and re-scan the whole vendor
  family before declaring a board lost; an interrupted flash is not the same
  as a bricked one. See [[tty-path-is-not-an-identity]].
- Verify a stale review finding instead of dismissing it. Two PRs sat on
  `CHANGES_REQUESTED` whose comments all predated the fixes, and the obvious
  move was to reply "already addressed" and move on. Checking each against
  the code found one that was only half-stale: the requested bounds existed,
  but the same comment also asked for oversized-request coverage, and nothing
  asserted the host validator treated a refusal as a refusal -- so a later
  loosening would have turned a rejected request into evidence. Pattern-
  matching "I already did that" would have missed the half that was real.
- A throwaway bench branch can inherit an upstream that points at a real PR.
  `bench/join-retry` was created from a PR branch and kept tracking
  `origin/diag/rp-net-server-stats`; it then accumulated 17 commits of
  bench-only merges of three other branches. A bare `git push` in that
  worktree -- and this session ran `git push` in worktrees constantly --
  would have pushed all of it onto an approved PR. Nothing was lost, but only
  because the push never happened. Create bench branches from `origin/master`
  with no upstream, or `git branch --unset-upstream` immediately, and never
  let an experiment branch track the branch it was forked from.
- Count distinct events over a defined set before publishing a number. Three
  times this session I published totals that were wrong: quoted from memory
  once, and twice computed from a glob that swept in ~290 log files from
  earlier sessions while double-counting failures that each log records twice
  (once in the run summary, once in the diagnostic capture). Every correction
  was in the direction of overstating the evidence. The habit that works is
  to restrict the file set explicitly, count the distinct failing rows rather
  than message occurrences, and re-derive the number at publish time instead
  of carrying it forward in prose.
- "Announced success" and "verified success" are different, and I conflated
  them three times in one session, each in a new disguise: `cmd | tail; echo
  $?` reported tail's status, a script ending in `[ $rc -ne 0 ] && ...`
  returned 1 for a run that passed, and `git push; echo pushed` printed
  "pushed" after a rejected non-fast-forward push -- I then told a PR the fix
  was in when it was not. The shape is always the same: reading success from
  something other than the operation itself. Capture the operation's own
  status into a variable, check that variable, and for anything outward-facing
  re-read the remote state before claiming it landed.
- Do not rebuild a branch by hand when the remote already has your work.
  After taking master's copy of a file wholesale and re-applying my changes on
  top, my local branch had a parallel history for the same edits. A teammate's
  commit to that branch then produced a 13-hunk merge, 10 of them in one test
  file, all self-inflicted -- the remote already carried every change I was
  "restoring". `git reset --hard origin/<branch>` followed by applying only
  the genuinely new hunk turned it into one clean commit. Check what the
  remote already has before reconstructing anything. See
  [[bench-branch-upstream]].
- Resolving a conflict by concatenating both sides is not a strategy. It works
  for two independent additions and silently corrupts anything else: one
  boundary here fell mid-expression and the "keep both" splice split a
  function in half, producing a SyntaxError that only surfaced at import.
  Read each hunk, decide whether it is additive or two implementations of the
  same thing, and parse the file afterwards.
