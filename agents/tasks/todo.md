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
- All five legs now require sketch-specific runtime markers, including the
  complete DriverTest pass marker, instead of accepting entry into `setup()`.
- Local evidence: native ESP32-DEV fbuild/QEMU exit 0, 22 focused tests,
  728 remaining Python tests, 362 C++ tests/examples, actionlint, full lint,
  and three-cycle pre-push review. One unrelated MinGW Renesas guard test and
  one fbuild smoke rerun were environment-blocked by another worktree's daemon;
  isolated PR jobs are the acceptance evidence.

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
