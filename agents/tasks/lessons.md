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
