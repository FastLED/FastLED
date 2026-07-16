# Lessons Learned

<!-- Add lessons from corrections and discoveries here -->

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

- For a responsive music-only visual, use two qualifying bass beats in a
  broad 30-240 BPM-equivalent range plus a low-ZCF check to arm a temporary
  music-present state. Treat that range as music evidence, never a fixed
  tempo-consistency or animation-lock requirement.

- FastLED's `getBeatConfidence()` is a normalized 0-1 spectral-flux score;
  use it only to arm music mode. Keep Vibe as the immediate rhythmic response
  after arming. FFT operates on short PCM frames, while the detector compares
  the resulting spectrum across frames.
