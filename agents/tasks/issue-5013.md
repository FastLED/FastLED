# Issue #5013: XIAO SAMD21 assembly link compatibility

Goal: reproduce the reported Arduino Seeeduino SAMD 1.8.6 link failure,
fix it, identify and repair fbuild validation gaps, and merge the resulting PRs.

- [x] Read the original report and inspect the standalone ESP32 assembly.
- [x] Verify Seeed 1.8.6 assembler recipe omits CPU/Thumb flags.
- [x] Probe GCC 9: empty foreign assembly contains ARM v4T attributes but links;
  this newer toolchain does not reproduce the legacy linker failure.
- [x] Reproduce with official GCC 7-2017q4, then the complete Arduino board build.
- [x] Add failing regression coverage before implementing the fix.
- [x] Trace fbuild source selection, assembly flags, and archive/link semantics;
  fix actual backend defects with independent RED/GREEN evidence.
- [x] Validate the actual XIAO Arduino build and ESP32-C6 fbuild Blink.
- [x] Review, push, and merge the source and backend repair PRs.
- [x] Publish fbuild 2.5.51 through GitHub CI and verify the installed wheel.

Final consumer gate: merge the dependency-floor follow-up after its full
hosted CI passes. The PR's checks and merge state are the final evidence.

Evidence belongs under `.build/evidence/issue-5013/` (ignored).
The GCC 7 archive checksum comes from Seeed's official package index:
`96a029e2ae130a1210eaa69e309ea40463028eab18ba19c1086e4c2dafe69a6a`.

## Findings and evidence

Seeed's assembler recipe omits CPU and Thumb flags. Even an empty foreign
assembly object receives default ARM attributes; its GCC 7 linker rejects
these against Cortex-M0+ (`12/0`) and Cortex-M4 (`13/0`). GCC 9 accepts the
same objects, so testing a newer compiler does not cover this failure.

fbuild supplies the board CPU flags to assembly. Its local FastLED path
already discovers `.S`; the recipe difference masks
the Arduino failure. Investigation also found separate backend defects in
extra-library assembly driver/flag selection and ignored archive policy;
these are tracked in FastLED/fbuild#1885. The assembly dispatch fix merged in
FastLED/fbuild#1886; archive policy, cache invalidation, native link regressions,
and version 2.5.51 merged through the queue in FastLED/fbuild#1887.
FastLED/fbuild#1888 subsequently fixed a Windows-only regression fixture and
the CI summary's GitHub API pagination. FastLED/fbuild#1892 fixed cold hosted
Linux runner capacity by uninstalling unused Android NDK packages through
the SDK manager, preserving build caches and disk reserve guards.
The current exact-SHA release dry run is GitHub Actions run `41819723869`, at
`9b26affea5ad8b2cf8ffe09f597759758d5d5359`, with publication disabled;
the entire run passed, including all six native artifact builds and wheel
smoke checks. Publication passed in GitHub Actions run `41825898032` at the
same source commit. Git tag `v2.5.51` resolves to that exact commit; PyPI
provides all five platform wheels. Releases are built and published only by
GitHub CI. The FastLED dependency floor is now 2.5.51; the full-CI follow-up
PR remains pending.

The source repair merged in FastLED/FastLED#5015 at
`90463e920ddcd2e188fca3decdd1dbb2505cdb30`, closing issue #5013.
That PR ran native validation; a dependency-floor follow-up must run full
board validation against the published fbuild 2.5.51 package.

The minimal fix keeps the released `.S` path and its RISC-V instructions
unchanged, and supplies compatible Thumb architecture metadata only for
empty SAMD objects. Renaming a released compiled source would violate
`ci/check_released_sources.py` and leave stale code during overlay upgrades.

Actual Arduino CLI / Seeeduino SAMD 1.8.6 / GCC 7-2017q4:
- Unchanged source: fails linking `fast_isr.S.o`, architecture `12/0`.
- Fixed source: Blink links, **57,428 bytes** of flash.
- Direct legacy recipe probes: Cortex-M0+ and Cortex-M4 pass; a Cortex-M4
  hard-float probe also passes.
- Full native/Python CI now runs the exact old-compiler probes with an official,
  SHA-256-verified toolchain. Cold provisioning plus all nine focused checks
  passed in **35.65 seconds**; local users can supply the compiler explicitly.
  The routine smoke inventory stays unchanged: its expansion requires separate
  whole-event measurements. Platform-sensitive merges require full CI, which
  now includes this previously missing legacy frontend regression.
- ESP32-C6 fbuild Blink: final minimal source passes (28 seconds).
  The published 2.5.51 wheel (`9b26affea5ad`) also passes through
  `bash compile esp32c6 --examples Blink` in four seconds, using the existing
  SDK cache: 317,352 bytes of flash and 17,348 bytes of RAM. The wheel was
  installed with `uv sync --upgrade-package fbuild==2.5.51 --no-build-package
  fbuild`; no local producer build or release was used.
- Full native debug validation: **329 unit groups + 95 examples** passed;
  final-source incremental confirmation passed again in **100.60 seconds**.
- Full Python validation passed: **2425 tests**, plus **3281 subtests**;
  51 skips and two expected failures were reported.
- ESP noexcept linter: a discovered false positive on valid global GNU asm
  has an independent RED/GREEN regression; all **14** targeted checks pass.
