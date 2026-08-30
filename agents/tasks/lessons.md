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
