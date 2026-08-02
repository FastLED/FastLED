# Task Tracker

<!-- Add tasks here as checkable items -->

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
