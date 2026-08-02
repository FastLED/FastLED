# Lessons Learned

<!-- Add lessons from corrections and discoveries here -->

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
